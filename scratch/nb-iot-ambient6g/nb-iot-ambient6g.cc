/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC),
 * Copyright (c) 2022 Communication Networks Institute at TU Dortmund University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>
 */

#include <chrono>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <cstdlib>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>


#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/application-container.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/markov-udp-client.h"
#include "ns3/progress-bar.h"

#include <ns3/propagation-loss-model.h>
#include <ns3/constant-spectrum-propagation-loss.h>
#include <ns3/friis-spectrum-propagation-loss.h>
#include <ns3/winner-plus-propagation-loss-model.h>


#include "ns3/log.h"
#include "ns3/nb-iot-energy.h"
#include "ns3/generic-capacitor.h"
#include "ns3/basic-energy-harvester.h"
#include "ns3/solar-energy-harvester.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NbIotAmbient6G");

/*
 * custom NBIoT module class
 * */
class BG96c : public NbiotChip{
public:
    BG96c(){
        // Compare Joerke nbiot-nidd-ciotopt
        m_psmPower = 3.8* 3.9*std::pow(10,-6);
        m_drxPower = 3.8* 1.56*std::pow(10,-3);
        m_edrxPower = 3.8* 0.81*std::pow(10,-3);
        m_uplinkPower = 3.8* 155*std::pow(10,-3);
        m_downlinkPower = 80*std::pow(10,-3);
        m_idlePower = 3.8 * 0.81*std::pow(10,-3);
    };
};

void log_levels(bool all, enum LogLevel level) 
{
  if (all)
  LogComponentEnableAll(level);
  else {
    LogComponentEnable("LteEnbRrc", level);
    LogComponentEnable("LteEnbMac", level);
    LogComponentEnable("LteEnbPhy", level);

    LogComponentEnable("LteUeRrc", level);
    LogComponentEnable("LteUeMac", level);
    LogComponentEnable("LteUePhy", level);

  }
}


// Per-UE ambient-IoT trackers: harvested energy, depletion/recovery cycles,
// uptime above capacitor threshold. Populated by trace hooks + periodic polling.
struct UeEnergyTracker {
    Ptr<GenericCapacitor> cap;
    NbiotEnergyModel*     energyModel = nullptr;  // for polled brown-out recovery
    double cutoffJ             = 0.0;
    double harvestedJ          = 0.0;
    double harvestedAtStart    = 0.0;   // snapshot at the stats-start cutoff (warm-up window)
    Time   uptime              = Seconds(0);
    uint32_t nDepletions       = 0;
    Time   firstDepletionTime  = Time::Max();
    Time   firstRecoveryTime   = Time::Max();
    bool   wasDepletedLastTick = false;
    bool   everDepleted        = false;
};

static void OnHarvestedTrace (double* slot, double /*oldVal*/, double newVal) {
    *slot = newVal;
}

// Brown-out callback: pause/resume the Markov client.
static void OnBrownout (Ptr<MarkovUdpClient> client,
                        uint32_t /*imsi*/,
                        bool entering) {
    if (!client) return;
    if (entering) client->Pause();
    else          client->Resume();
}

// Periodic poller: compares cap remaining energy to its cutoff and updates
// uptime / depletion-cycle counters. Self-reschedules until simEnd.
static void PollUeEnergy (UeEnergyTracker* t, Time interval, Time simEnd) {
    bool depletedNow = (t->cap->GetRemainingEnergy() <= t->cutoffJ + 1e-9);
    if (!depletedNow) t->uptime += interval;
    if (depletedNow && !t->wasDepletedLastTick) {
        ++t->nDepletions;
        if (!t->everDepleted) {
            t->firstDepletionTime = Simulator::Now();
            t->everDepleted = true;
        }
    }
    if (!depletedNow && t->wasDepletedLastTick
        && t->everDepleted && t->firstRecoveryTime == Time::Max()) {
        t->firstRecoveryTime = Simulator::Now();
    }
    t->wasDepletedLastTick = depletedNow;
    if (t->energyModel) t->energyModel->PollBrownoutRecovery();
    if (Simulator::Now() + interval <= simEnd) {
        Simulator::Schedule(interval, &PollUeEnergy, t, interval, simEnd);
    }
}

// Warm-up reset at the stats-start cutoff: discard everything accumulated during
// the warm-up so the END-of-sim energy metrics (duty cycle, uptime, depletion,
// harvested) reflect ONLY the steady-state window [statsStart, simEnd] -- the
// same window the app-level loss/delay/throughput already use.
static void StatsReset (UeEnergyTracker* t) {
    t->harvestedAtStart    = t->harvestedJ;     // windowed harvest = final - this
    t->uptime              = Seconds(0);
    // A UE already browned out at the cutoff must be counted as depleted-in-window
    // (else a UE that is down across the cutoff and never recovers reports
    // "never depleted"). Seed the window counters from the current state; keep
    // wasDepletedLastTick so the next poll does not double-count the same edge.
    t->everDepleted        = t->wasDepletedLastTick;
    t->nDepletions         = t->wasDepletedLastTick ? 1u : 0u;
    t->firstDepletionTime  = t->wasDepletedLastTick ? Simulator::Now() : Time::Max();
    t->firstRecoveryTime   = Time::Max();
    if (t->energyModel) t->energyModel->ResetAccounting();   // windows the duty cycle
}

/**
 * Tracer function to log state changes to the console.
 * @param oldVal Previous state value.
 * @param newVal New state value.
 */
static void StateChangeTracer(int oldVal, int newVal)
{
  NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s: State changed from " << oldVal << " to " << newVal);
}

/**
 * \brief Report UE measurements.
 *
 * This function is a callback that is invoked whenever a UE
 * reports its measurements to the eNB. The measurements are
 * reported in terms of Reference Signal Received Power (RSRP)
 * and Reference Signal Received Quality (RSRQ).
 *
 * \param rnti the IMSI of the UE that reported the measurements.
 * \param cell_id the CellId of the cell that the UE is currently
 *         camping on.
 * \param avg_rsrp the average RSRP measured by the UE.
 * \param avg_rsrq the average RSRQ measured by the UE.
 * \param same_cell a boolean indicating whether the UE is still
 *         camping on the same cell or not.
 * \param componentCarrierId the ID of the component carrier on
 *         which the measurements were reported.
 */
void ReportUeMeasurements(std::string logdir, uint16_t rnti, uint16_t cell_id, double avg_rsrp, double avg_rsrq, bool same_cell, uint8_t componentCarrierId)
{
  std::ofstream out(logdir + "ReportUeMeasurements.log", std::ios::app);
  double timeMs  = ns3::Simulator::Now().GetMilliSeconds();

  out << "ReportUeMeasurements RNTI: " << rnti << " CellId: " << cell_id << " RSRP: " << avg_rsrp << " RSRQ: " << avg_rsrq << " ComponentCarrierID: " << (componentCarrierId ? componentCarrierId : -1) << " Time: " << timeMs << std::endl;
  out.close();
}


int main (int argc, char *argv[])
{
  Time simDuration {Seconds(3600)};            // 1 hour
  std::string logDir {"output"};
  // Verbose ns-3 LTE NS_LOG_DEBUG. OFF by default: at large N it floods stdout
  // (GBs) and throttles the sim to a crawl (the .out metrics are unaffected).
  bool ns3Debug {false};
  int numUes {10};
  double cellSize {2500};
  double heightOfUes {1.5};
  std::string positioning {"uniform"};

  /*
   * 32 Bytes 5G mMTC payload + 4 Bytes CoAP Header + 13 Bytes DTLS Header
   * UDP Header and IP Header are added by NS-3
   * */
  int packetSize {49};
  Time packetGenInterval {Seconds(300)};       // 5 min - matches deep-ambient KPI
  Time startTime { MilliSeconds(10)};

  // Energy front-end: super-cap + diurnal solar harvester
  // (sin^2 over the sim window: 0 -> Pmax -> 0)
  bool   solarProfile  {true};                 // false => constant Pmean
  double harvestPmaxW  {0.005};                // 5 mW peak (mean = 2.5 mW)
  double capCapacitanceF {1.0};                // 1 F supercap
  double capMaxV       {3.3};
  double capInitV      {2.5};                  // start near cutoff (battery-less cold-start)
  // Ambient-IoT: randomise each UE's start voltage in [capInitVMin, capMaxV]
  // (devices have been harvesting for different times before the run). Off =>
  // every UE starts at the fixed capInitV above.
  bool   capInitVRandom {false};
  double capInitVMin    {2.7};                 // lower bound of the per-UE random start band
  double capCutoffV    {2.4};                  // raise from 1.8 V; usable = 2.5 J
  // Leakage resistance: tau = R_leak * C sets the RC charge time-constant.
  // Default 10 kohm gives tau ~ 10^4 s,
  double capLeakageR   {10000.0};              // 10 kohm


  bool ciot {false};
  bool edt {false};
  std::string propagationLoss{"friis"};
  Time channelDelay {MilliSeconds (10)};
  bool persistentGrant {true};
  bool sendFirst {false};
  // Model 1 (deep-sleep + SR-resurrect): FUG UE sleeps in PSM (~15 uW) between
  // packets and self-resurrects via the SR, instead of paying the connected
  // NPDCCH-monitoring floor (~3 mW). Disable to recover the connected-floor model.
  bool deepSleepFug {true};
  // Connected-mode DRX for the FUG-off baseline (deepSleepFug=false). Instead of
  // the bare connected NPDCCH-monitoring floor, the UE stays RRC-connected but
  // monitors NPDCCH only during the on-duration of each long DRX cycle
  // (TS 36.321 5.7; DRX-Config-NB-r13, drx-Cycle-v1430 -> sf10240 = 10.24 s).
  bool     cdrxFug {false};
  uint32_t cdrxCycleMs     {10240};  // long DRX cycle (sf10240 = 10.24 s)
  uint32_t cdrxInactivityMs {500};   // drx-InactivityTimer (extends Active Time after a grant)
  bool     proactiveFug {false};
  uint32_t srDedicatedSubcarriers {4};   // SR subcarriers carved from the 12-tone NPRACH pool
  uint32_t srBaseNprachPeriodMs   {80};  // CE0 NPRACH occasion period (round-robin base); matches
                                         // lte-enb-rrc.cc CE0 ms80. Sensitivity floor ms40 (CE0-only).
  uint32_t srPeriodMs {0};               // manual override in ms; 0 = derive from round-robin model
  bool     srPreambleSr {false};         // faithful dedicated SR via real NPRACH preamble
  bool     srHybridContention {false};   // unscheduled UEs also contend on the shared pool (needs srPreambleSr)
  uint32_t srContentionSubcarriers {6};  // offset: reserved SR subcarriers start above this many
  // Oracle / ideal-BSR upper bound: the eNB knows each UE's buffer the instant
  // data arrives -- no SR, no RA, no contention, no signalling energy (the UE
  // still transmits the data on the grant). A best-case reference arm, NOT a
  // realistic scheme. Implies persistentGrant + connected (no deep sleep) so the
  // instant-BSR path fires; off by default.
  bool     oracleBsr {false};

  // Warm-up exclusion: app-level loss/delay/throughput only count packets
  // GENERATED at/after this time, so the first cold-start RA herd (~first epoch)
  // does not bias steady-state results. 0 = full run. Energy/depletion stay full-run.
  double   statsStartSec {0.0};
  // Tail exclusion: also drop packets GENERATED after this time -- a last-epoch
  // packet has too little time to be delivered before sim end and would inflate
  // loss. 0 => auto = simDuration - one packet epoch. Applied to app metrics.
  double   statsEndSec {0.0};

  // Realistic NB-IoT mass-IoT timers (3GPP / GSMA deployment guide).
  // Override on the command line to study sensitivity.
  uint32_t t3324_ms     {20000};    // Active timer  (20 s)
  uint64_t t3412_ms     {3600000};  // Periodic TAU  (1 h)
  uint32_t edrxCycle_ms {20480};    // eDRX cycle    (20.48 s)
  uint32_t rrcRelease_ms{5000};     // RRC inactivity release (5 s)

  // Ambient-IoT mode: when true, override the mass-IoT timers above to a
  // Class-C "active batteryless with brief listening windows" set. Short
  // post-data listen and longer eDRX cycle keep the per-packet on-time
  // affordable under harvest-only operation while still leaving a 2 s
  // Energy-budget check: ~1.4 J/h drain vs ~9 J/h harvest = 6.4x margin.
  bool     ambientIoT   {false};
  uint32_t aiotT3324Ms       {2000};      // 2 s post-data listen for ACK / MT trigger
  uint64_t aiotT3412Ms       {86400000};  // 24 h TAU — never fires in a 1 h sim
  uint32_t aiotEdrxCycleMs   {40960};     // 40.96 s — rare paging-window cycling
  uint32_t aiotRrcReleaseMs  {1000};      // 1 s release after RLC drain


  CommandLine cmd (__FILE__);
  cmd.AddValue ("simDuration", "Total duration of the simulation", simDuration);
  cmd.AddValue ("logDir", "Directory for the output stats files", logDir);
  cmd.AddValue ("numUes", "Number of UEs", numUes);
  cmd.AddValue ("ciot", "Cellular IoT Optimization", ciot);
  cmd.AddValue ("edt", "Early Data Transmission", edt);
  cmd.AddValue ("cellSize", "Cell size in meters", cellSize);
  cmd.AddValue("propagationLoss", "Propagation loss model: friis, fixed, or winner", propagationLoss);
  cmd.AddValue("positioning", "Positioning model for ues: uniform, random, or same", positioning);
  cmd.AddValue("heightOfUes", "Height of UEs", heightOfUes);
  cmd.AddValue ("persistentGrant",
                "Skip re-RACH after initial access; rely on DCI0 grants", persistentGrant);
  cmd.AddValue ("sendFirst",
                "Send-first traffic: always send then decide next state", sendFirst);
  cmd.AddValue ("deepSleepFug",
                "Model 1: FUG UE deep-sleeps (PSM) between packets and resurrects via SR "
                "(else stays in the connected NPDCCH-monitoring floor)", deepSleepFug);
  cmd.AddValue ("cdrxFug",
                "FUG-off baseline: use connected-mode DRX (gate NPDCCH monitoring to the "
                "on-duration of each DRX cycle) instead of bare monitoring. Needs !deepSleepFug",
                cdrxFug);
  cmd.AddValue ("cdrxCycleMs", "Connected-mode long DRX cycle in ms (NB-IoT max sf10240=10240)", cdrxCycleMs);
  cmd.AddValue ("proactiveFug",
                "Standalone 4th mode: proactive FUG (eNB predicts each UE's period and pushes "
                "grants, no SR). Implies persistentGrant + deepSleepFug", proactiveFug);
  cmd.AddValue ("srPreambleSr",
                "Faithful dedicated SR: UE transmits a real NPRACH preamble on its reserved "
                "subcarrier; eNB resolves identity from its resource->RNTI map (TS 36.331 "
                "SchedulingRequestConfig-NB)", srPreambleSr);
  cmd.AddValue ("srHybridContention",
                "Hybrid SR: a connected UE waiting for its reserved occasion also contends on the "
                "shared pool every base NPRACH occasion; a singleton from a connected UE is granted "
                "(no RAR). The reserved slot stays the guaranteed floor. Needs srPreambleSr",
                srHybridContention);
  cmd.AddValue ("statsStartSec",
                "Warm-up cutoff (s): app-level loss/delay/throughput exclude packets generated "
                "before this time, so the first cold-start RA does not bias steady state. "
                "0 = full run; set ~one epoch past the first packet (e.g. 600)", statsStartSec);
  cmd.AddValue ("statsEndSec",
                "Tail cutoff (s): exclude packets generated after this from app metrics "
                "(undeliverable last-epoch packets bias loss). 0 = auto (simDuration - one epoch)",
                statsEndSec);
  cmd.AddValue ("oracleBsr",
                "Oracle / ideal-BSR upper bound: eNB learns each UE's buffer instantly on data "
                "arrival (no SR/RA/contention/signalling energy). Implies persistentGrant + "
                "connected (deepSleepFug=false). Reference arm, not a realistic scheme",
                oracleBsr);
  cmd.AddValue ("srContentionSubcarriers", "Offset: reserved SR subcarriers start above this many "
                "(RA/contention pool size)", srContentionSubcarriers);
  cmd.AddValue ("srDedicatedSubcarriers",
                "Dedicated SR subcarriers reserved from the NPRACH pool; effective SR "
                "period = srBaseNprachPeriod * ceil(numUes / this)", srDedicatedSubcarriers);
  cmd.AddValue ("srBaseNprachPeriod",
                "Base NPRACH occasion period in ms (CE0) for the SR round-robin model",
                srBaseNprachPeriodMs);
  cmd.AddValue ("srPeriod",
                "Manual override for the dedicated-NPRACH SR period in ms; 0 = derive from "
                "the round-robin model (srBaseNprachPeriod * ceil(numUes/pool))", srPeriodMs);
  cmd.AddValue ("solarProfile",
                "Use sin^2 diurnal solar harvest profile (else constant)", solarProfile);
  cmd.AddValue ("harvestPmaxW",
                "Peak harvested power in W (mean = Pmax/2 under solar)", harvestPmaxW);
  cmd.AddValue ("capCapacitanceF",
                "Supercap capacitance in F", capCapacitanceF);
  cmd.AddValue ("capMaxV",
                "Cap full-charge voltage in V", capMaxV);
  cmd.AddValue ("capInitV",
                "Cap starting voltage in V (used when capInitVRandom=false)", capInitV);
  cmd.AddValue ("capInitVRandom",
                "Randomise each UE's start voltage uniformly in [capInitVMin, capMaxV]", capInitVRandom);
  cmd.AddValue ("capInitVMin",
                "Lower bound of the per-UE random start voltage band (V)", capInitVMin);
  cmd.AddValue ("capCutoffV",
                "Cap depletion-cutoff voltage in V", capCutoffV);
  cmd.AddValue ("capLeakageR",
                "Leakage resistance in ohm; tau = R*C sets RC time-constant. "
                "Use ~1e6 to disable visible asymptote, ~1e4 to model it",
                capLeakageR);
  cmd.AddValue ("packetGenIntervalSec",
                "Markov tick (= mean inter-arrival in s)",
                packetGenInterval);
  cmd.AddValue ("t3324Ms",     "T3324 active timer in ms (3GPP-typical 20000)", t3324_ms);
  cmd.AddValue ("t3412Ms",     "T3412 periodic TAU timer in ms (typical 3600000)", t3412_ms);
  cmd.AddValue ("eDrxCycleMs", "eDRX cycle in ms (NB-IoT: 2560*2^k; typical 20480)", edrxCycle_ms);
  cmd.AddValue ("rrcReleaseMs","RRC inactivity release in ms (typical 5000)", rrcRelease_ms);
  cmd.AddValue ("ambientIoT",  "If true, use Class-C active-batteryless timers for both "
                                "PG and RA (T3324=2s, T3412=24h, eDRX=40.96s, "
                                "RrcRelease=1s). Trade-off: brief reachability window "
                                "kept so PG can operate; energy still ~6x under harvest.",
                ambientIoT);

  cmd.AddValue ("ns3Debug", "Enable verbose ns-3 LTE NS_LOG_DEBUG (floods stdout at large N; off by default)", ns3Debug);
  cmd.Parse (argc, argv);

  // Proactive FUG (4th mode) is a deep-sleep, persistent-grant mode whose grants
  // come from the eNB predictor instead of an SR. Force the implied settings.
  if (proactiveFug) { persistentGrant = true; deepSleepFug = true; cdrxFug = false; }

  // Oracle / ideal-BSR: the instant-BSR path fires only for a CONNECTED UE
  // (DoReportBufferStatus, !suspended), so keep the UE connected (no deep sleep)
  // with a persistent grant. cDRX may stay ON to save DL-monitoring energy while
  // idle (Idealised FUG = oracle scheduling + cDRX); leave cdrxFug as configured.
  if (oracleBsr) { persistentGrant = true; deepSleepFug = false; }

  if (ns3Debug) log_levels(false, LOG_LEVEL_DEBUG);
  /*
   * make and change into the lodDir
   * */
  std::string mkdir {"mkdir -p " + logDir};
  if ( system(mkdir.c_str()) == -1 ) {
    NS_FATAL_ERROR("Error in creating log directory" << strerror(errno));
  }

  logDir.append("/");

  /* 
   * Component carrier
   * UlBandwidth represents the uplink transmission bandwidth configuration in terms of number of Resource Blocks (RBs)
   * */
  Config::SetDefault ("ns3::ComponentCarrier::UlBandwidth", UintegerValue (50));
  Config::SetDefault ("ns3::ComponentCarrier::DlBandwidth", UintegerValue (50));
  Config::SetDefault ("ns3::ComponentCarrier::PrimaryCarrier", BooleanValue (true));

  /*
   * Create eNB and Ues and set their position and mobilities
   *
   * */
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(numUes);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator> ();
  // Place our single eNb right in the center of the cell. Height of enb is 25m
  positionAllocEnb->Add (Vector (cellSize/2, cellSize/2, 25));
  // Install Mobility Model. Fix eNB at the center
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.SetPositionAllocator(positionAllocEnb);
  mobilityEnb.Install(enbNodes);


  Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator> ();
  if (positioning == "same")
  {
    // Place all UEs at the same position, in the center of the cell
    positionAllocUe->Add (Vector (cellSize/2, cellSize/2, heightOfUes));
  }
  else if (positioning == "uniform")
  {
    // Place UEs uniformly at the same distance from the BS in the cell
    double radius = cellSize / 2; // radius of the circle where UEs are placed
    for (int i = 0; i < numUes; ++i)
    {
      double PI = 3.14159265358979323846;
      double angle = 2 * PI * i / numUes; // distribute UEs uniformly around the eNB
      double x = radius + radius * std::cos(angle);  // x-coordinate = centerX + radius * cos(angle)
      double y = radius + radius * std::sin(angle);  // y-coordinate = centerY + radius * sin(angle)
      positionAllocUe->Add (Vector (x, y, heightOfUes));
    }
  }
  else if (positioning == "random")
  {
    // Install Mobility Model for the UEs
    // The UEs are placed randomly inside a disc around the eNB, with radius cell_size
    ObjectFactory pos_a;
    pos_a.SetTypeId ("ns3::UniformDiscPositionAllocator");
    pos_a.Set ("X", StringValue (std::to_string(cellSize/2)));
    pos_a.Set ("Y", StringValue (std::to_string(cellSize/2)));
    pos_a.Set ("Z", DoubleValue (heightOfUes));  // height of the UEs, we should also vary this in the future
    pos_a.Set ("rho", DoubleValue (cellSize/2));
    Ptr<PositionAllocator> m_position = pos_a.Create ()->GetObject<PositionAllocator> ();
    for (int i = 0; i < numUes; ++i){
        Vector position = m_position->GetNext ();
      positionAllocUe->Add (position);
    }
  }
  // Install Mobility Model
  // Nodes are static. No movement is simulated.
  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityUe.SetPositionAllocator(positionAllocUe);
  mobilityUe.Install (ueNodes);

  /*
   * configure LTE
   * */
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create the EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Enable RRC logging
  lteHelper->EnableRrcLogging ();
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetUeAntennaModelType ("ns3::IsotropicAntennaModel");

  
  // Propagation Loss Model
  if (propagationLoss == "friis")
  {
    // single-frequency path loss model
    lteHelper->SetPathlossModelType(ns3::FriisPropagationLossModel::GetTypeId());

  }
  else if (propagationLoss == "friis-spectrum")
  {
    lteHelper->SetPathlossModelType(FriisSpectrumPropagationLossModel::GetTypeId());
  }
  else if (propagationLoss == "fixed")
  {
    // set the loss value using --ns3::ConstantSpectrumPropagationLossModel::Loss=x
    lteHelper->SetPathlossModelType(ConstantSpectrumPropagationLossModel::GetTypeId());
  }
  else if (propagationLoss == "winner")
  {
    // Note that the Winner+ pathloss model isn't available in the current release of ns3. It can be downloaded at https://github.com/tudo-cni/ns3-propagation-winner-plus
    lteHelper->SetPathlossModelType(ns3::WinnerPlusPropagationLossModel::GetTypeId());
    lteHelper->SetPathlossModelAttribute ("HeightBasestation", DoubleValue (50));
    lteHelper->SetPathlossModelAttribute ("Environment", EnumValue (UMaEnvironment));
    lteHelper->SetPathlossModelAttribute ("LineOfSight", BooleanValue (false));
  }
  else
  {
    // Cannot validate input for the propagation loss model
    NS_FATAL_ERROR("Invalid propagationLossModel: must be 'friis', 'friis-spectrum', 'fixed', or 'winner'");
  }

  // No fast-fading model. The UEs are STATIC (ConstantPositionMobilityModel),
  // so a 3 km/h ETU fast-fading trace is physically inappropriate for them.
  // It is also the source of run-to-run non-determinism: TraceFadingLossModel
  // keys its per-channel fading RNG by a pointer pair (ChannelRealizationId_t)
  // and assigns RNG streams in pointer/iteration order, so ASLR randomised the
  // fading-trace offset per link each run -> different SINR -> different RA
  // outcomes. Channel = Friis path loss + noise (deterministic).
  // lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
  // lteHelper->SetFadingModelAttribute("TraceFilename", StringValue(
  //     std::string(NS3_ROOT_DIR) + "/src/lte/model/fading-traces/fading_trace_ETU_3kmph.fad"));

  lteHelper->SetAttribute ("UseIdealRrc", BooleanValue (false));
  lteHelper->SetAttribute ("UsePdschForCqiGeneration", BooleanValue (true));
  //disable Uplink Power Control
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));



  /*
   * PointToPointEpcHelper internally creates two more nodes:
   * - MME (Mobility Management Entity)
   *   - SGW (Serving Gateway)
   *   These two nodes are part of the EPC core and are not directly exposed, but they are still actual Node objects that get registered in the simulator, hence IDs 1 and 2.
   * */

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);

  // Create the Internet
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);
  // point-to-point link between remote host and PGW
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (channelDelay));
  // place the PGW and the remote host on the same network
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install LTE Devices to the nodes
  Config::SetDefault ("ns3::LteEnbRrc::PersistentGrant", BooleanValue (persistentGrant));
  Config::SetDefault ("ns3::LteEnbRrc::ProactiveFug", BooleanValue (proactiveFug));

  if (ambientIoT) {
      t3324_ms      = aiotT3324Ms;
      t3412_ms      = aiotT3412Ms;
      edrxCycle_ms  = aiotEdrxCycleMs;
      rrcRelease_ms = aiotRrcReleaseMs;
  }

  // Apply effective timer values to the eNB-RRC defaults.
  Config::SetDefault ("ns3::LteEnbRrc::T3324",              IntegerValue ((int32_t) t3324_ms));
  Config::SetDefault ("ns3::LteEnbRrc::T3412",              IntegerValue ((int64_t) t3412_ms));
  Config::SetDefault ("ns3::LteEnbRrc::TeDRXC",             IntegerValue ((int32_t) edrxCycle_ms));
  Config::SetDefault ("ns3::LteEnbRrc::RrcReleaseInterval", UintegerValue ((uint16_t) rrcRelease_ms));
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Proactive FUG (4th mode): put the cell scheduler in proactive mode so it
  // predicts each UE's period and pushes grants instead of waiting for an SR.
  Ptr<LteEnbNetDevice> enbDev = enbLteDevs.Get (0)->GetObject<LteEnbNetDevice> ();
  enbDev->GetMac ()->SetProactiveFug (proactiveFug);


  // Attach a Ipv4 to UEs
  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
  {
    Ptr<Node> ueNode = ueNodes.Get (u);
    // Set the default gateway for the UE
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  // Activate EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);

  // Create a Traffic Flow Template (TFT)
  Ptr<EpcTft> tft = Create<EpcTft>();
  EpcTft::PacketFilter pf;
  pf.localPortStart = 2000;
  pf.localPortEnd = 2000;
  pf.remotePortStart = 2000;
  pf.remotePortEnd = 2000;
  pf.direction = EpcTft::UPLINK;
  tft->Add(pf);

  // Activate the dedicated bearer
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, tft);

  // Install and start applications on UEs and remote host
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;


  uint32_t srRoundRobinPeriods =
      (srDedicatedSubcarriers == 0)
          ? 1u
          : (static_cast<uint32_t> (numUes) + srDedicatedSubcarriers - 1) / srDedicatedSubcarriers;
  uint32_t effSrPeriodMs =
      (srPeriodMs != 0) ? srPeriodMs : srBaseNprachPeriodMs * srRoundRobinPeriods;
  std::cout << "[SR model] numUes=" << numUes
            << " dedicatedSubcarriers=" << srDedicatedSubcarriers
            << " base=" << srBaseNprachPeriodMs << "ms"
            << " roundRobinPeriods=" << srRoundRobinPeriods
            << " => effSrPeriod=" << effSrPeriodMs << "ms" << std::endl;

  if (srPreambleSr && !proactiveFug) {
    enbDev->GetMac ()->SetSrTopology (srDedicatedSubcarriers, srContentionSubcarriers,
                                      srRoundRobinPeriods);
    enbDev->GetMac ()->SetSrHybridContention (srHybridContention);
  }

  // Set up the data transmission for the Pre-Run
  for (uint16_t i = 0; i < numUes; ++i)
  {
    lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

    // LteUeNetDevice contains GetMac(), GetRrc(), and GetPhy()
    // GetImsi()
    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();

    // Log the received packets
    //ueLteDevice->SetReceiveCallback (MakeBoundCallback (&ReceiveCallback, logdir, ueLteDevice->GetImsi()));

    ueRrc->m_energyModel.SetModule(BG96c()); // Set the NBIoT module to BG96
    //ueRrc->EnableLogging();  // disabled: produces RA.log, DataTrans.log, Energy.log
    ueRrc->m_energyModel.EnableLogging();     // enable only nbiot_energy.log
    ueRrc->m_energyModel.SetLogDir(logDir);   // set the log directory for the energy model
    /*
     * BUG: When edt is true, there is a certain (fixed) quantum of packets
     * that are received at the server side, although the clients keep
     * generating packets.
     * */
    if(ciot == true){
      ueRrc->SetAttribute("CIoT-Opt", BooleanValue(true));
    }
    else{
      ueRrc->SetAttribute("CIoT-Opt", BooleanValue(false));
    }
    if(edt == true){
      //std::cout << "EDT" << std::endl;
      ueRrc->SetAttribute("EDT", BooleanValue(true));
    }
    else{
      ueRrc->SetAttribute("EDT", BooleanValue(false));
    }

    if (persistentGrant) {
      ueRrc->SetAttribute ("PSM",  BooleanValue (deepSleepFug));
      ueRrc->SetAttribute ("eDRX", BooleanValue (false));
      ueRrc->SetAttribute ("PersistentGrant", BooleanValue (persistentGrant));
      ueRrc->SetAttribute ("ProactiveFug", BooleanValue (proactiveFug));
      ueLteDevice->GetMac ()->SetPersistentGrant (persistentGrant);   // mirror to MAC
      ueLteDevice->GetMac ()->SetSrPeriod (effSrPeriodMs);            // round-robin SR cadence: base*ceil(N/pool)
      ueLteDevice->GetMac ()->SetDeepSleepFug (deepSleepFug);         // (Stage B will use this for SR-resume)
      ueLteDevice->GetMac ()->SetCdrx (cdrxFug && !deepSleepFug, cdrxCycleMs, cdrxInactivityMs);
      ueLteDevice->GetMac ()->SetProactiveFug (proactiveFug);         // 4th mode: no SR, await predicted grant
      ueLteDevice->GetMac ()->SetOracleBsr (oracleBsr);               // upper-bound arm: instant ideal BSR
      if (srPreambleSr && !proactiveFug) {
        ueLteDevice->GetMac ()->SetSrDedicated (i, srDedicatedSubcarriers, srContentionSubcarriers);
        ueLteDevice->GetMac ()->SetSrHybridContention (srHybridContention);
      }
      }

    UdpServerHelper server (ulPort);
    serverApps.Add(server.Install (remoteHost));

    Ptr<MarkovUdpClient> ulClient = CreateObject<MarkovUdpClient>();

    ulClient->SetRemote(remoteHostAddr, ulPort);
    ++ulPort;
    ulClient->SetRates(packetGenInterval, packetGenInterval); // INACTIVE and ACTIVE intervals
    ulClient->SetAttribute ("MaxPackets", UintegerValue (1000000));
    ulClient->SetAttribute ("PacketSize", UintegerValue(packetSize));
    ulClient->SetTransitionProbabilities(0.7, 0.2);  // P(INACTIVE→ACTIVE), P(ACTIVE→INACTIVE)
    ulClient->SetAttribute ("SendFirst", BooleanValue(sendFirst));
    ulClient->TraceConnectWithoutContext("State", MakeCallback(&StateChangeTracer));

      Ptr<Node> client = ueNodes.Get(i);
      // ulClient->SetNode (client);
      client->AddApplication (ulClient);
      clientApps.Add (ulClient);

      Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
      Time jitter = MilliSeconds(rand->GetValue(0, 500));

      clientApps.Get(i)->SetStartTime(startTime + jitter);
      //clientApps.Get(i)->SetStartTime (startTime);
      clientApps.Get(i)->SetStopTime (simDuration);

    // this callback is used to log the UE measurements
    Ptr< LteUePhy > uePhy = ueLteDevice->GetPhy ();
    //uePhy->TraceConnectWithoutContext("ReportUeMeasurements", MakeBoundCallback(&ReportUeMeasurements, logDir));

    // uePhy->SetAttribute ("TxPower", DoubleValue (23.0));
    // uePhy->SetAttribute ("NoiseFigure", DoubleValue (9.0));
  }

  // Add a GenericCapacitor and a SolarEnergyHarvester ----
  const Time   pollInterval    = MilliSeconds(100);
  const double cutoffJ         = 0.5 * capCapacitanceF * capCutoffV * capCutoffV;
  const double meanHarvestW    = solarProfile ? 0.5 * harvestPmaxW : harvestPmaxW;

  // Brown-out gate: when cap voltage falls below capCutoffV the energy model
  // stops draining and the Markov client stops generating traffic, mimicking a
  // real PMIC's brown-out cut-off. Recovery requires the cap to climb back
  // above (capCutoffV + brownoutHystV) to avoid chatter.
  const double brownoutHystV   = 0.2;             // 200 mV hysteresis
  const double recoveryV       = capCutoffV + brownoutHystV;
  const double brownoutJ       = cutoffJ;
  const double recoveryJ       = 0.5 * capCapacitanceF * recoveryV * recoveryV;

  std::vector<Ptr<GenericCapacitor>>     ueCaps(numUes);
  std::vector<Ptr<SolarEnergyHarvester>> ueHarvs(numUes);
  std::vector<UeEnergyTracker>           ueTracker(numUes);
  // Per-UE random start voltage (ambient-IoT cold-start heterogeneity). Own
  // RngStream so the draw is reproducible and varies with RngRun.
  Ptr<UniformRandomVariable> capInitVar = CreateObject<UniformRandomVariable> ();
  capInitVar->SetAttribute ("Min", DoubleValue (capInitVMin));
  capInitVar->SetAttribute ("Max", DoubleValue (capMaxV));
  capInitVar->SetStream (777);
  for (uint16_t i = 0; i < numUes; ++i)
  {
    Ptr<Node> node = ueNodes.Get(i);
    Ptr<LteUeNetDevice> ueDev = ueLteDevs.Get(i)->GetObject<LteUeNetDevice>();
    Ptr<LteUeRrc> ueRrc = ueDev->GetRrc();

    double initV = capInitVRandom ? capInitVar->GetValue () : capInitV;
    Ptr<GenericCapacitor> cap = CreateObject<GenericCapacitor>();
    cap->SetAttribute("Capacitance",                   DoubleValue(capCapacitanceF));
    cap->SetAttribute("MaxCapacitorVoltage",           DoubleValue(capMaxV));
    cap->SetAttribute("InitialCapacitorVoltage",       DoubleValue(initV));
    cap->SetAttribute("ThresholdVoltage",              DoubleValue(capCutoffV));
    cap->SetAttribute("SupplyVoltage",                 DoubleValue(capMaxV));
    cap->SetAttribute("InternalResistance",            DoubleValue(0.1));
    cap->SetAttribute("LeakageResistance",             DoubleValue(capLeakageR));
    cap->SetAttribute("PeriodicEnergyUpdateInterval",  TimeValue(pollInterval));
    cap->SetNode(node);

    // SolarEnergyHarvester: encapsulates the analytic
    //   P_h(t) = Pmax * sin^2(pi * (t / DayPeriod + PhaseOffset))
    Ptr<SolarEnergyHarvester> harv = CreateObject<SolarEnergyHarvester>();
    if (solarProfile) {
        harv->SetAttribute("PeakPower",   DoubleValue(harvestPmaxW));
        harv->SetAttribute("DayPeriod",   TimeValue(simDuration));
        harv->SetAttribute("PhaseOffset", DoubleValue(0.0));
    } else {
        // Flat profile at meanHarvestW: long DayPeriod + noon offset
        // pins sin^2 at ~1 for the entire sim.
        harv->SetAttribute("PeakPower",   DoubleValue(meanHarvestW));
        harv->SetAttribute("DayPeriod",   TimeValue(Seconds(1e9)));
        harv->SetAttribute("PhaseOffset", DoubleValue(0.5));
    }
    harv->SetAttribute("UpdateInterval", TimeValue(pollInterval));
    harv->SetNode(node);
    harv->SetEnergySource(cap);
    cap->ConnectEnergyHarvester(harv);      // register on the source side too

    cap->Initialize();
    harv->Initialize();

    ueRrc->m_energyModel.SetEnergySource(cap);
    ueRrc->m_energyModel.SetBrownoutThresholds(brownoutJ, recoveryJ);

    // Brown-out callback
    Ptr<MarkovUdpClient> uClient = clientApps.Get(i)->GetObject<MarkovUdpClient>();
    if (uClient)
    {
      ueRrc->m_energyModel.SetBrownoutCallback(
          MakeBoundCallback(&OnBrownout, uClient));
    }

    ueCaps[i]  = cap;
    ueHarvs[i] = harv;

    // Ambient-IoT trackers
    ueTracker[i].cap         = cap;
    ueTracker[i].energyModel = &ueRrc->m_energyModel;
    ueTracker[i].cutoffJ     = cutoffJ;
    harv->TraceConnectWithoutContext(
        "TotalEnergyHarvested",
        MakeBoundCallback(&OnHarvestedTrace, &ueTracker[i].harvestedJ));
    Simulator::Schedule(pollInterval, &PollUeEnergy,
                        &ueTracker[i], pollInterval, simDuration);
    // Warm-up window: at the cutoff, reset the energy/depletion accounting so
    // end-of-sim duty/uptime/depletion/harvested cover only [statsStart, end].
    if (statsStartSec > 0.0)
      Simulator::Schedule(Seconds(statsStartSec), &StatsReset, &ueTracker[i]);
  }

  serverApps.Start(startTime);
  serverApps.Stop(simDuration);

  // Steady-state window [statsStartSec, statsEndEff]: exclude the first
  // synchronized cold-start RA herd (warm-up) AND the last epoch (tail, whose
  // packets cannot be delivered before sim end) from app loss/delay/throughput.
  const double statsEndEff = (statsEndSec > 0.0)
      ? statsEndSec
      : std::max (statsStartSec + 1.0,
                  simDuration.GetSeconds () - packetGenInterval.GetSeconds ());
  for (uint32_t i = 0; i < serverApps.GetN (); i++)
    {
      Ptr<UdpServer> srv = DynamicCast<UdpServer> (serverApps.Get (i));
      srv->SetStatsStartTime (Seconds (statsStartSec));
      srv->SetStatsEndTime (Seconds (statsEndEff));
      Ptr<MarkovUdpClient> cli = clientApps.Get (i)->GetObject<MarkovUdpClient> ();
      if (cli) { cli->SetStatsStartTime (Seconds (statsStartSec));
                 cli->SetStatsEndTime (Seconds (statsEndEff)); }
    }

  // FlowMonitor: IP-layer delay + packet loss (UE -> remote host)
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor>  flowMon = flowHelper.InstallAll();

  ProgressBar pg ((simDuration));
  Simulator::Stop (simDuration); // Run
  Simulator::Run ();

   // Statistics
  uint64_t rxBytes = 0;

  // Per-UE byte logging
  std::ofstream perUeOutStream;
  perUeOutStream.open (logDir + "rxbytes_per_ue.out", std::ios::out);
  perUeOutStream << "UE_ID\tRxBytes_bits\tThroughput_kbps" << std::endl;

  // App-level loss + delay (exact; no FlowMonitor 10s timeout/cap). Counted over
  // the steady-state window (packets GENERATED at/after statsStartSec): sent from
  // the MarkovUdpClient, received + accumulated delay from the UdpServer.
  uint64_t appSent = 0, appReceived = 0, appRxBytesWin = 0;
  Time     appDelaySum = Seconds (0);
  const double statsWindowS = std::max (1.0, statsEndEff - statsStartSec);
  for (uint32_t i = 0; i < serverApps.GetN (); i++)
    {
      Ptr<UdpServer> srv = DynamicCast<UdpServer> (serverApps.Get (i));
      uint64_t ueRxBytes = srv->GetTotalRx ();        // full-run bytes (per-UE detail file)
      rxBytes += ueRxBytes;
      double ueThroughput = (ueRxBytes * 8) / (simDuration.GetSeconds()) / 1000.0;
      perUeOutStream << (i + 1) << "\t" << (ueRxBytes * 8) << "\t" << ueThroughput << std::endl;

      Ptr<MarkovUdpClient> cli = clientApps.Get (i)->GetObject<MarkovUdpClient> ();
      if (cli) appSent += cli->GetSentWindow ();
      appReceived   += srv->GetReceivedWindow ();
      appDelaySum   += srv->GetDelaySumWindow ();
      appRxBytesWin += srv->GetTotalRxWindow ();
    }
  perUeOutStream.close ();

  double appLossRatio   = appSent ? double (appSent - appReceived) / appSent : 0.0;
  double appMeanDelayMs = appReceived ? appDelaySum.GetMilliSeconds () / double (appReceived) : 0.0;
  double appThroughputKbps = (appRxBytesWin * 8) / statsWindowS / 1000.0; // steady-state throughput

  // FlowMonitor: per-flow delay + loss
  // UE -> remote-host flow end-to-end delay from FlowMonitor includes the
  // fixed EPC delays (S1-U + PGW<->remoteHost p2p link). Subtract those to
  // report the UE -> eNB radio-layer delay directly.
  TimeValue s1Delay;
  epcHelper->GetAttribute("S1uLinkDelay", s1Delay);
  const double excludeMs =
      s1Delay.Get().GetMilliSeconds() + channelDelay.GetMilliSeconds();

  flowMon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

  std::ofstream flowOut(logDir + "flow_stats.out");
  flowOut << "FlowId\tSrc\tDst\tTxPkts\tRxPkts\tLostPkts\tLossRatio\t"
             "RawDelay_ms\tUE_to_eNB_Delay_ms\tMeanJitter_ms\n";

  uint64_t aggTx = 0, aggRx = 0, aggLost = 0;
  double   aggDelayMs = 0.0;        // accumulates UE->eNB (corrected) delay * rxPkts
  for (auto& kv : flowMon->GetFlowStats())
    {
      const auto& s = kv.second;
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv.first);
      double rawDelay   = s.rxPackets
          ? s.delaySum.GetMilliSeconds()  / double(s.rxPackets)      : 0.0;
      double ueEnbDelay = std::max(0.0, rawDelay - excludeMs);
      double meanJitter = s.rxPackets > 1
          ? s.jitterSum.GetMilliSeconds() / double(s.rxPackets - 1)  : 0.0;
      double lossRatio  = s.txPackets
          ? double(s.lostPackets) / s.txPackets                      : 0.0;

      flowOut << kv.first << "\t" << t.sourceAddress << "\t" << t.destinationAddress
              << "\t" << s.txPackets << "\t" << s.rxPackets << "\t" << s.lostPackets
              << "\t" << lossRatio << "\t" << rawDelay << "\t" << ueEnbDelay
              << "\t" << meanJitter << "\n";

      aggTx      += s.txPackets;
      aggRx      += s.rxPackets;
      aggLost    += s.lostPackets;
      aggDelayMs += ueEnbDelay * s.rxPackets;    // rx-weighted mean of UE->eNB
    }
  flowOut.close();
  double aggMeanDelay = aggRx ? aggDelayMs / double(aggRx) : 0.0;  // UE->eNB only
  double aggLossRatio = aggTx ? double(aggLost) / aggTx   : 0.0;

  // Per-UE ambient-IoT snapshot. GetEnergyRemaining() flushes the last state
  // into m_timeSpendInState so GetDutyCycle() reflects the full sim.
  std::ofstream enOut(logDir + "energy_per_ue.out");
  enOut << "UE_ID\tRemaining_J\tFraction\tDepleted\tFirstDepletionTime_ms\t"
           "FirstRecoveryTime_ms\tNDepletions\tHarvested_J\t"
           "UptimeFraction\tDutyCycle\n";

  uint32_t nDepleted     = 0;
  double   sumHarvestedJ = 0.0;
  double   sumUptimeFrac = 0.0;
  double   sumDutyCycle  = 0.0;
  uint32_t sumNDep       = 0;
  const double capMaxJ = 0.5 * capCapacitanceF * capMaxV * capMaxV;
  for (uint16_t i = 0; i < numUes; ++i)
    {
      Ptr<LteUeNetDevice> ueDev = ueLteDevs.Get(i)->GetObject<LteUeNetDevice>();
      auto& em = ueDev->GetRrc()->m_energyModel;
      em.FlushStateTime();                       // no battery access -> no NaN

      const auto& tr = ueTracker[i];
      double rem  = tr.cap->GetRemainingEnergy();
      double frac = capMaxJ > 0 ? rem / capMaxJ : 0.0;
      double duty = em.GetDutyCycle();   // windowed by ResetAccounting() at statsStart

      bool dep = tr.everDepleted;        // depleted within the window
      if (dep) ++nDepleted;

      // uptime / harvested over the steady-state window [statsStart, end].
      double uptimeFrac = tr.uptime.GetSeconds() / statsWindowS;
      double harvestedWin = tr.harvestedJ - tr.harvestedAtStart;

      enOut << (i + 1) << "\t" << rem << "\t" << frac << "\t"
            << dep << "\t"
            << (dep ? tr.firstDepletionTime.GetMilliSeconds() : -1) << "\t"
            << ((tr.firstRecoveryTime == Time::Max()) ? -1
                : tr.firstRecoveryTime.GetMilliSeconds()) << "\t"
            << tr.nDepletions << "\t" << harvestedWin << "\t"
            << uptimeFrac << "\t" << duty << "\n";

      sumHarvestedJ += harvestedWin;
      sumUptimeFrac += uptimeFrac;
      sumDutyCycle  += duty;
      sumNDep       += tr.nDepletions;
    }
  enOut.close();

  double avgHarvestedJ = numUes > 0 ? sumHarvestedJ / numUes : 0.0;
  double avgUptimeFrac = numUes > 0 ? sumUptimeFrac / numUes : 0.0;
  double avgDutyCycle  = numUes > 0 ? sumDutyCycle  / numUes : 0.0;

  // issued >> packets carried => many wasted (mispredicted) grants.
  uint64_t proactiveGrants = enbDev->GetMac ()->GetProactiveGrantsIssued ();

  std::ofstream sumOut(logDir + "summary.out");
  sumOut << "numUes\tpersistentGrant\tsendFirst\taggTx\taggRx\taggLost\taggLossRatio\t"
            "aggMeanUEtoENBDelay_ms\tnDepleted\tavgHarvested_J\tavgUptimeFrac\t"
            "avgDutyCycle\tsumDepletionEvents\tproactiveGrants\t"
            "appSent\tappReceived\tappLossRatio\tappMeanDelay_ms\tappThroughput_kbps\tstatsStartSec\n";
  sumOut << numUes << "\t" << persistentGrant << "\t" << sendFirst << "\t"
         << aggTx << "\t" << aggRx << "\t" << aggLost << "\t" << aggLossRatio
         << "\t" << aggMeanDelay << "\t" << nDepleted << "\t"
         << avgHarvestedJ << "\t" << avgUptimeFrac << "\t"
         << avgDutyCycle << "\t" << sumNDep << "\t" << proactiveGrants << "\t"
         << appSent << "\t" << appReceived << "\t" << appLossRatio << "\t" << appMeanDelayMs
         << "\t" << appThroughputKbps << "\t" << statsStartSec << "\n";
  sumOut.close();

  Simulator::Destroy ();

  double throughput = (rxBytes * 8) / (simDuration.GetSeconds()) / 1000.0; //kbit/s

  std::ofstream totalStatsOutStream;
  totalStatsOutStream.open (logDir + "rxbytes.out", std::ios::out);
  totalStatsOutStream << "RxBytes_bits\tThroughput_kbps" << std::endl;
  totalStatsOutStream <<(rxBytes * 8) <<"\t" <<throughput << std::endl;
  totalStatsOutStream.close ();

  std::cout << "\nDone." << std::endl;


  return 0;



}

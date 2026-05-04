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

// Diurnal solar profile: P(t) = Pmax * sin^2(pi * t / T_day), driven by
// retuning a per-UE ConstantRandomVariable that the harvester reads each
// tick.
struct SolarRetuneCfg {
    Ptr<ConstantRandomVariable> rv;
    double Pmax;     // W
    Time   period;   // full sin^2 hump (= one "day")
    Time   step;     // retune cadence
    Time   simEnd;
};

static void RetuneSolarHarvester(SolarRetuneCfg cfg) {
    double phase = std::fmod(Simulator::Now().GetSeconds(),
                             cfg.period.GetSeconds())
                   / cfg.period.GetSeconds();
    double s = std::sin(M_PI * phase);
    double P = cfg.Pmax * s * s;             // sin^2 envelope: 0 -> Pmax -> 0
    cfg.rv->SetAttribute("Constant", DoubleValue(P));
    if (Simulator::Now() + cfg.step <= cfg.simEnd) {
        Simulator::Schedule(cfg.step, &RetuneSolarHarvester, cfg);
    }
}

// Periodic poller: compares cap remaining energy to its cutoff and updates
// uptime / depletion-cycle counters. Self-reschedules until simEnd.
//
// Also drives the energy model's brown-out recovery probe: in RA mode the UE
// parks in PSM during brown-out and the LTE stack stops generating state
// changes, so the event-driven recovery check inside DoNotifyStateChange
// would never fire. Polling decouples that check from LTE activity.
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
  double capCutoffV    {2.4};                  // raise from 1.8 V; usable = 2.5 J
  // Leakage resistance: tau = R_leak * C sets the RC charge time-constant.
  // Default 10 kohm gives tau ~ 10^4 s,
  // so the cap visibly approaches its asymptote (charging slows near V_max)
  // instead of charging linearly. Trade-off: leakage power = V^2/R_leak,
  // ~0.9 mW at 3 V (i.e. ~36 % of the 2.5 mW mean harvest is dissipated).
  // Matches the CAP-XX HS-series 1 F super-cap leakage spec (~10 kohm).
  double capLeakageR   {10000.0};              // 10 kohm


  bool ciot {false};
  bool edt {false};
  std::string propagationLoss{"friis"};
  Time channelDelay {MilliSeconds (10)};
  bool persistentGrant {true};
  bool sendFirst {false};

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
  cmd.AddValue ("solarProfile",
                "Use sin^2 diurnal solar harvest profile (else constant)", solarProfile);
  cmd.AddValue ("harvestPmaxW",
                "Peak harvested power in W (mean = Pmax/2 under solar)", harvestPmaxW);
  cmd.AddValue ("capCapacitanceF",
                "Supercap capacitance in F", capCapacitanceF);
  cmd.AddValue ("capMaxV",
                "Cap full-charge voltage in V", capMaxV);
  cmd.AddValue ("capInitV",
                "Cap starting voltage in V", capInitV);
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

  cmd.Parse (argc, argv);

  log_levels(false, LOG_LEVEL_DEBUG);
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

  // enable fading
  lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
  std::string fadingTracePath = std::string(NS3_ROOT_DIR) + "/src/lte/model/fading-traces/fading_trace_ETU_3kmph.fad";
  lteHelper->SetFadingModelAttribute("TraceFilename", StringValue(fadingTracePath));

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
      ueRrc->SetAttribute ("PSM", BooleanValue (false));
      ueRrc->SetAttribute ("PersistentGrant", BooleanValue (persistentGrant));
      ueLteDevice->GetMac ()->SetPersistentGrant (persistentGrant);   // mirror to MAC
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

  // Add a GenericCapacitor and a BasicEnergyHarvester ----
  const Time   pollInterval    = MilliSeconds(100);
  const Time   solarRetuneStep = Seconds(60);     // sun moves slowly: 60 s tick is plenty
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
  std::vector<Ptr<BasicEnergyHarvester>> ueHarvs(numUes);
  std::vector<UeEnergyTracker>           ueTracker(numUes);
  for (uint16_t i = 0; i < numUes; ++i)
  {
    Ptr<Node> node = ueNodes.Get(i);
    Ptr<LteUeNetDevice> ueDev = ueLteDevs.Get(i)->GetObject<LteUeNetDevice>();
    Ptr<LteUeRrc> ueRrc = ueDev->GetRrc();

    Ptr<GenericCapacitor> cap = CreateObject<GenericCapacitor>();
    cap->SetAttribute("Capacitance",                   DoubleValue(capCapacitanceF));
    cap->SetAttribute("MaxCapacitorVoltage",           DoubleValue(capMaxV));
    cap->SetAttribute("InitialCapacitorVoltage",       DoubleValue(capInitV));
    cap->SetAttribute("ThresholdVoltage",              DoubleValue(capCutoffV));
    cap->SetAttribute("SupplyVoltage",                 DoubleValue(capMaxV));
    cap->SetAttribute("InternalResistance",            DoubleValue(0.1));
    cap->SetAttribute("LeakageResistance",             DoubleValue(capLeakageR));
    cap->SetAttribute("PeriodicEnergyUpdateInterval",  TimeValue(pollInterval));
    cap->SetNode(node);

    Ptr<BasicEnergyHarvester> harv = CreateObject<BasicEnergyHarvester>();
    harv->SetAttribute("PeriodicHarvestedPowerUpdateInterval",
                       TimeValue(pollInterval));

    // Harvester is fed by a per-UE ConstantRandomVariable. In solar mode a
    // periodic retune callback rewrites that constant from a sin^2 envelope.
    Ptr<ConstantRandomVariable> hrv = CreateObject<ConstantRandomVariable>();
    hrv->SetAttribute("Constant", DoubleValue(solarProfile ? 0.0 : meanHarvestW));
    harv->SetAttribute("HarvestablePower", PointerValue(hrv));
    harv->SetNode(node);
    harv->SetEnergySource(cap);
    cap->ConnectEnergyHarvester(harv);      // register on the source side too

    cap->Initialize();
    harv->Initialize();

    if (solarProfile) {
      SolarRetuneCfg cfg{hrv, harvestPmaxW, simDuration, solarRetuneStep, simDuration};
      Simulator::ScheduleNow(&RetuneSolarHarvester, cfg);
    }

    ueRrc->m_energyModel.SetEnergySource(cap);
    ueRrc->m_energyModel.SetBrownoutThresholds(brownoutJ, recoveryJ);

    // Brown-out callback: pause this UE's Markov client when the energy model
    // crosses the cut-off threshold; resume it when the cap recharges past the
    // hysteresis recovery point. Captures the per-UE client by index, so the
    // ulClient pointer (in the apps loop above) is reachable via clientApps.
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
  }

  serverApps.Start(startTime);
  serverApps.Stop(simDuration);

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

  for (uint32_t i = 0; i < serverApps.GetN (); i++)
    {
      uint64_t ueRxBytes = DynamicCast<UdpServer> (serverApps.Get (i))->GetTotalRx ();
      rxBytes += ueRxBytes;
      double ueThroughput = (ueRxBytes * 8) / (simDuration.GetSeconds()) / 1000.0;
      perUeOutStream << (i + 1) << "\t" << (ueRxBytes * 8) << "\t" << ueThroughput << std::endl;
    }
  perUeOutStream.close ();

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
      double duty = em.GetDutyCycle();

      bool dep = tr.everDepleted;
      if (dep) ++nDepleted;

      double uptimeFrac = (simDuration.GetSeconds() > 0)
          ? tr.uptime.GetSeconds() / simDuration.GetSeconds() : 0.0;

      enOut << (i + 1) << "\t" << rem << "\t" << frac << "\t"
            << dep << "\t"
            << (dep ? tr.firstDepletionTime.GetMilliSeconds() : -1) << "\t"
            << ((tr.firstRecoveryTime == Time::Max()) ? -1
                : tr.firstRecoveryTime.GetMilliSeconds()) << "\t"
            << tr.nDepletions << "\t" << tr.harvestedJ << "\t"
            << uptimeFrac << "\t" << duty << "\n";

      sumHarvestedJ += tr.harvestedJ;
      sumUptimeFrac += uptimeFrac;
      sumDutyCycle  += duty;
      sumNDep       += tr.nDepletions;
    }
  enOut.close();

  double avgHarvestedJ = numUes > 0 ? sumHarvestedJ / numUes : 0.0;
  double avgUptimeFrac = numUes > 0 ? sumUptimeFrac / numUes : 0.0;
  double avgDutyCycle  = numUes > 0 ? sumDutyCycle  / numUes : 0.0;

  std::ofstream sumOut(logDir + "summary.out");
  sumOut << "numUes\tpersistentGrant\tsendFirst\taggTx\taggRx\taggLost\taggLossRatio\t"
            "aggMeanUEtoENBDelay_ms\tnDepleted\tavgHarvested_J\tavgUptimeFrac\t"
            "avgDutyCycle\tsumDepletionEvents\n";
  sumOut << numUes << "\t" << persistentGrant << "\t" << sendFirst << "\t"
         << aggTx << "\t" << aggRx << "\t" << aggLost << "\t" << aggLossRatio
         << "\t" << aggMeanDelay << "\t" << nDepleted << "\t"
         << avgHarvestedJ << "\t" << avgUptimeFrac << "\t"
         << avgDutyCycle << "\t" << sumNDep << "\n";
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

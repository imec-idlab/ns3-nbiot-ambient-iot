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
 * Authors: Henrique Duarte Moura <henrique.duartemoura@imec.be>
 */

/**
 This code is a sample simulation script for LTE+EPC.
 It initializes the logging system for different components such as "LenaNb5G-Cap" and "GenericCapacitor".
 The main function sets up a simulation environment with specific parameters:
 - simDuration: Duration of the simulation.
 - seed: The seed for random number generation, initialized to 1.
 - simName: The name of the simulation, set to "cap".
 - cell_size: The size of the cell in meters, set to 2500 meters.
 - num_ues: Number of User Equipments (UEs) per application, set to 1.
 - packetsize_app: The packet size for Application A, set to 49 bytes (32 bytes payload + headers).
 - packetinterval_app: The interval between packets for Application A, set to 1 day.
 The script intends to create a network setup with eNodeBs (base stations) and UEs (user devices),
 establishing communication flows between the UEs and a remote host.

 It uses a generic-capacitor as power source.

 Usage:
 ./waf --run nb-scenario3.cc 2>&1 | tee nb-scenario2.log

 You can use `process_log.py` to see some outputs in the log file.

 */
#include <chrono>
#include <iomanip>
#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <cstdlib>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/markov-udp-client.h"  // Include the custom MarkovUdpClient header
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/lte-module.h"

#include <ns3/propagation-loss-model.h>
// #include <ns3/spectrum-error-model.h>
#include <ns3/constant-spectrum-propagation-loss.h>
#include <ns3/friis-spectrum-propagation-loss.h>
#include <ns3/winner-plus-propagation-loss-model.h>

// #include "ns3/single-model-spectrum-channel.h"
// #include "ns3/spectrum-channel.h"
// #include "ns3/spectrum-helper.h"

#include "ns3/log.h"
#include "ns3/nb-iot-energy.h"
#include "ns3/basic-energy-harvester.h"

#include "ns3/netanim-module.h"

using namespace ns3;

constexpr double PI = 3.14159265358979323846;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It can also start another flow between each UE pair.


  To run the script, use the following command:
  $ ./waf --run "nb-scenario4.cc --simDuration=1800 --num_ues=4 \
        --ns3::LteUePhy::RsrpSinrSamplePeriod=1 \
        --ns3::ConstantSpectrumPropagationLossModel::Loss=5 \
        --ns3::LteUePhy::NoiseFigure=5 --ns3::LteUePhy::TxPower=10 \
        --ns3::LteEnbPhy::NoiseFigure=5 --ns3::LteEnbPhy::TxPower=30"

 */

NS_LOG_COMPONENT_DEFINE ("LenaNb5G-Cap");



/**
 * Set the logging levels for the NS-3 components used in the simulation.
 *
 * @param logLevel the desired logging level.
 *
 * This function sets the logging level for the components used in the simulation.
 * It is useful for debugging purposes, as it allows to control the verbosity of
 * the logging output.
 */
void setLogLevels(ns3::LogLevel logLevel)
{
  ns3::LogComponentEnable("LenaNb5G-Cap", logLevel);
  ns3::LogComponentDisable("LenaNb5G-Cap", LOG_LEVEL_DEBUG);
  // ns3::LogComponentEnable("LteUeRrc", logLevel);
  ns3::LogComponentEnable("MarkovUdpClient", logLevel);
  ns3::LogComponentEnable("MarkovUdpClient", logLevel);

  ns3::LogComponentEnable ("LteUeRrc", logLevel);
  ns3::LogComponentEnable ("LteUeMac", logLevel);
  ns3::LogComponentEnable ("LteUePhy", logLevel);

  ns3::LogComponentEnable ("LteEnbRrc", logLevel);
  ns3::LogComponentEnable ("LteEnbMac", logLevel);
  ns3::LogComponentEnable ("LteEnbPhy", logLevel);

  ns3::LogComponentEnable ("LteSpectrumPhy", logLevel);
}

void MyShowProgress(double totalTime, double interval) {
    double currentTime = Simulator::Now().GetSeconds();
    double percent = (currentTime / totalTime) * 100.0;
    std::cout << "\rSimulation progress: " << int(percent) << "% completed" << std::flush;

    if (currentTime < totalTime) {
        Simulator::Schedule(Seconds(interval), &MyShowProgress, totalTime, interval);
    }
}


void CreateLogDirectory(const std::string& simName,
                        int num_ues,
                        const Time& simDuration,
                        bool ciot,
                        bool edt,
                        std::string& outputPath)
{
    auto start = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    auto tm = *std::localtime(&start_time);
    std::stringstream ss;
    ss << std::put_time(&tm, "%d_%m_%Y_%H_%M_%S");

    std::string logdir = "logs/" + simName;
    logdir += "/u" + std::to_string(num_ues);
    logdir += "_t" + std::to_string(simDuration.GetInteger());
    logdir += "_c" + std::to_string(ciot);
    logdir += "_e" + std::to_string(edt);
    logdir += "/" + ss.str();

    std::string makedir = "mkdir -p " + logdir;
    int z = std::system(makedir.c_str());
    NS_LOG_DEBUG("cmd: " << makedir << " : " << z);
    // NS_LOG_INFO("Log dir: " << logdir);

    outputPath = logdir + "/";  // return the final path via reference
}


// ---------------------------------------------------------------------
//
// custom NBIoT module class
//
// ---------------------------------------------------------------------
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


// *******************************************************************************
//
// Callback wrapper
//
// *******************************************************************************

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


/**
 * Logs information about a received packet.
 *
 * This function is a callback function, registered with a NetDevice to receive
 * packets. It logs the details of the received packet, including IMSI of the
 * UE, local address of the NetDevice, source address of the packet, protocol
 * type of the packet, and the size of the packet in bytes.
 *
 * \param logdir  Log directory where the log should be written.
 * \param imsi    IMSI of the UE.
 * \param nd      NetDevice that received the packet.
 * \param p       The packet that was received.
 * \param protocol  Protocol of the packet.
 * \param addr    Source address of the packet.
 *
 * \return True, indicating that the packet was successfully processed.
 *
 * **Note**: The last 4 arguments are mandatory for the callback function.
 */
bool ReceiveCallback (std::string logdir, uint64_t imsi, Ptr<NetDevice> nd, Ptr<const Packet> p, uint16_t protocol, const Address& addr)
{
  // **Note**: Apparently this only logs the data packets

  // imsi, local address, source address, protocol, packet size (bytes), timestamp
  std::ofstream out(logdir + "receive.log", std::ios::app);
  std::stringstream ss;
  ss << imsi << ", " << nd->GetAddress() << ", " << addr << ", " << protocol << ", " << p->GetSize () << ", " << Now ().GetSeconds () << std::endl;
  out << "RECEIVE, " << ss.str();
  out.close();
  // NS_LOG_INFO (ss.str());

  return true;
}

bool ReceiveEnb (std::string logdir, Ptr<NetDevice> nd, Ptr<const Packet> p, uint16_t protocol, const Address& addr)
{
  // **Note**: Apparently this only logs the data packets

  // imsi, local address, source address, protocol, packet size (bytes), timestamp
  // imsi is 0 for eNB
  std::ofstream out(logdir + "receive-ENB.log", std::ios::app);
  std::stringstream ss;
  ss << 0 << ", " << nd->GetAddress() << ", " << addr << ", " << protocol << ", " << p->GetSize () << ", " << Now ().GetSeconds () << std::endl;
  out << "RECEIVE, " << ss.str();
  out.close();
  NS_LOG_INFO (ss.str());

  return true;
}

/**
 * Callback function that logs the establishment of a UE connection.
 *
 * This function is triggered when a UE establishes a connection. It logs the
 * details of the connection, including IMSI, Cell ID, and RNTI, to a specified
 * log directory and outputs the information to the NS-3 logging system.
 * It maps RNTI to IMSI.
 *
 * @param logdir The directory where the connection log file will be saved.
 * @param context The context in which the connection is established.
 * @param imsi The International Mobile Subscriber Identity of the UE.
 * @param cellId The ID of the cell to which the UE is connected.
 * @param rnti The Radio Network Temporary Identifier for the connection.
 */
void ConnectionEstablishedUeCallback (
  std::string logdir,
  std::string context,
  uint64_t imsi,
  uint16_t cellId,
  uint16_t rnti)
{
  std::ofstream out(logdir + "cell_connection.log", std::ios::app);
  out << "IMSI: " << imsi << " CellId: " << cellId << " RNTI: " << rnti << std::endl;
  out.close();
  NS_LOG_INFO ("Connection established for IMSI: " << imsi << ", CellId: " << cellId << ", RNTI: " << rnti);
}


/**
 * Tracer function to log state changes to the console.
 * @param oldVal Previous state value.
 * @param newVal New state value.
 */
static void StateChangeTracer(int oldVal, int newVal)
{
  NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: State changed from " << oldVal << " to " << newVal);
}


/**
 * Tracer function to log state changes to a file.
 * @param logdir Directory where the log file will be saved.
 * @param context Context of the state change (e.g., application name).
 * @param oldVal Previous state value.
 * @param newVal New state value.
 */
static void StateChangeTracerToFile(std::string logdir, std::string context, int oldVal, int newVal)
{
  std::ofstream logFile(logdir + "state-changes.log", std::ios::app);  // Append mode

  logFile << Simulator::Now().GetSeconds()
          << "s [" << context << "] State changed from "
          << oldVal << " to " << newVal << std::endl;
}


// *******************************************************************************
//
// eNB and UEs creation functions
//
// *******************************************************************************


/**
 * Create a specified number of UEs with a given positioning strategy.
 * @param num_ues The number of UEs to create.
 * @param positioning The positioning strategy to use. Can be one of "same", "uniform", or "random".
 * @param cell_size The size of the cell in meters.
 * @param heightOfUes The height of the UEs in meters.
 * @return A NodeContainer containing all the created UEs.
 */
NodeContainer create_ues(int num_ues, std::string positioning, double cell_size, double heightOfUes)
{
  /*
    --------------- create UEs ---------------
  */
  NodeContainer ueNodes;
  ueNodes.Create (num_ues); // Pre-Run, Run, Post-Run.
  Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator> ();
  if (positioning == "same")
  {
    // Place all UEs at the same position, in the center of the cell
    positionAllocUe->Add (Vector (cell_size/2, cell_size/2, heightOfUes));
  }
  else if (positioning == "uniform")
  {
    // Place UEs uniformly at the same distance from the BS in the cell
    double radius = cell_size / 2; // radius of the circle where UEs are placed
    for (int i = 0; i < num_ues; ++i)
    {
      double angle = 2 * PI * i / num_ues; // distribute UEs uniformly around the eNB
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
    pos_a.Set ("X", StringValue (std::to_string(cell_size/2)));
    pos_a.Set ("Y", StringValue (std::to_string(cell_size/2)));
    pos_a.Set ("Z", DoubleValue (heightOfUes));  // height of the UEs, we should also vary this in the future
    pos_a.Set ("rho", DoubleValue (cell_size/2));
    Ptr<PositionAllocator> m_position = pos_a.Create ()->GetObject<PositionAllocator> ();
    for (int i = 0; i < num_ues; ++i){
      Vector position = m_position->GetNext ();
      positionAllocUe->Add (position);
      NS_LOG_INFO("Node#" << i << " Position:" << position.x << "," << position.y << "," << position.z);
    }
  }
  // Install Mobility Model
  // Nodes are static. No movement is simulated.
  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityUe.SetPositionAllocator(positionAllocUe);
  mobilityUe.Install (ueNodes);

  return ueNodes;
}


/**
 * Create a specified number of eNBs with a given positioning strategy.
 * @param num_enbs The number of eNBs to create. Should be 1.
 * @param cell_size The size of the cell in meters.
 * @param heightOfEnb The height of the eNB in meters.
 * @return A NodeContainer containing all the created eNBs.
 */
NodeContainer create_enb(uint32_t num_enbs, double cell_size, double heightOfEnb)
{
  NodeContainer enbNodes;

  enbNodes.Create (num_enbs);  // should be 1

  NS_LOG_INFO("eNB node id: " << enbNodes.Get(0)->GetId ());

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  // Place our single eNb right in the center of the cell
  positionAlloc->Add (Vector (cell_size/2, cell_size/2, heightOfEnb));
  // Install Mobility Model. Fix eNB at the center
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.SetPositionAllocator(positionAlloc);
  mobilityEnb.Install(enbNodes);

  return enbNodes;
}

// *******************************************************************************
//
// Propagation loss factory
//
// *******************************************************************************


/**
 * Set the propagation loss model for the LTE simulation.
 *
 * @param lteHelper The helper object for the LTE simulation.
 * @param propagationLossModel The type of propagation loss model. Can be "friis", "fixed", or "winner".
 *
 * The "friis" model uses a simplified version of Friis' formula (L = 4 * pi * d * f / C^2) to calculate the propagation loss.
 * The "fixed" model uses a constant value for the propagation loss.
 * The "winner" model uses the Winner+ pathloss model (not available in the current release of ns3).
 * For the "winner" model, the user can set the following parameters:
 * - "HeightBasestation" (double): the height of the base station in meters.
 * - "Environment" (enum): the type of environment (UMaEnvironment, UrbanMacrocellEnvironment, etc.).
 * - "LineOfSight" (bool): whether the pathloss model considers line of sight or not.
 */
bool define_propagation_loss_model (Ptr<LteHelper> lteHelper, std::string propagationLossModel)
{
  if (propagationLossModel == "friis")
  {
    // single-frequency path loss model
    std::cout << "friis propagation loss model" << std::endl;
    lteHelper->SetPathlossModelType(ns3::FriisPropagationLossModel::GetTypeId());

  }
  else if (propagationLossModel == "friis-spectrum")
  {
    std::cout << "friis spectrum propagation loss model" << std::endl;
    lteHelper->SetPathlossModelType(ns3::FriisSpectrumPropagationLossModel::GetTypeId());
    // there is no configurable parameter for this pathloss model
  }
  else if (propagationLossModel == "fixed")
  {
    // set the loss value using --ns3::ConstantSpectrumPropagationLossModel::Loss=x
    lteHelper->SetPathlossModelType(ns3::ConstantSpectrumPropagationLossModel::GetTypeId());
  }
  else if (propagationLossModel == "winner")
  {
    lteHelper->SetPathlossModelType(ns3::WinnerPlusPropagationLossModel::GetTypeId()); // Note that the Winner+ pathloss model isn't available in the current release of ns3. It can be downloaded at https://github.com/tudo-cni/ns3-propagation-winner-plus
    lteHelper->SetPathlossModelAttribute ("HeightBasestation", DoubleValue (50));
    lteHelper->SetPathlossModelAttribute ("Environment", EnumValue (UMaEnvironment));
    lteHelper->SetPathlossModelAttribute ("LineOfSight", BooleanValue (false));
  }
  else
  {
    // Cannot validate input for the propagation loss model
    NS_FATAL_ERROR("Invalid propagationLossModel: must be 'friis', 'friis-spectrum', 'fixed', or 'winner'");
    return false;
  }
  return true;
}

// *******************************************************************************
//
//                                     MAIN
//
// *******************************************************************************

/**
 * Main function to set up and run the simulation.
 * It initializes the LTE network, configures UEs and eNBs, and runs applications.
 * @return Exit status of the program.
 */
int main (int argc, char *argv[])
{
  setLogLevels(LOG_LEVEL_INFO); // Set the log level to debug

  // --------------------------------------------------------------------------
  //
  // Simulation parameters
  //
  // --------------------------------------------------------------------------
  ns3::Time simDuration = Minutes(30);  // Total duration of the simulation
  std::string simName = "markov";  // Name of the simulation, used for logging

  int seed = 1;
  double cell_size = 2500; // in meters

  uint32_t mtu = 1500;
  Time channelDelay = MilliSeconds (10);

  // Number of UEs per application
  int num_ues = 1;  // For now, 1 UE talks to a remote host via one eNB
  double heightOfUes = 1.5; // height of the UEs

  // 32 Bytes 5G mMTC payload + 4 Bytes CoAP Header + 13 Bytes DTLS Header
  // UDP Header and IP Header are added by NS-3
  int packetsize_app = 49;

  // Packet interval
  Time packetinterval_app = Seconds(10);

  // Access delay for the application, in milliseconds
  Time access = MilliSeconds(10);

  std::string positioning = "uniform"; // default value is to use random position in the border of the cell
  // std::string propagationLossModel = "friis";  // default value is to use simple propagation loss model (friis)

  // default value is to use constant propagation loss model
  std::string propagationLossModel = "friis";

  bool ciot = false;
  bool edt = false;

  // --------------------------------------------------------------------------
  // Useful Command line arguments
  // =============================
  //
  // --ns3::LteEnbPhy::NoiseFigure=x // eNodeB noise figure in dB
  //
  // NoiseFigure models the receiver noise figure of the UE's physical layer, i.e., how much thermal noise and hardware imperfections degrade the received signal
  // --ns3::LteUePhy::NoiseFigure=x
  //
  // Path loss (dB) between transmitter and receiver that provides a constant attenuation applied to all signals, regardless of distance, frequency, or environment.
  // --ns3::ConstantSpectrumPropagationLossModel::Loss=x
  // --------------------------------------------------------------------------
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simDuration", "Total duration of the simulation", simDuration);
  cmd.AddValue ("simName", "Name of the simulation", simName);
  cmd.AddValue ("randomSeed", "randomSeed", seed);
  cmd.AddValue ("num_ues", "Number of UEs", num_ues);
  cmd.AddValue ("ciot", "Cellular IoT Optimization", ciot);
  cmd.AddValue ("edt", "Early Data Transmission", edt);
  cmd.AddValue ("cell_size", "Cell size in meters", cell_size);
  cmd.AddValue("propagationLossModel", "Propagation loss model: friis, fixed, or winner", propagationLossModel);
  cmd.AddValue("positioning", "Positioning model: uniform, random, or same", positioning);
  cmd.AddValue("heightOfUes", "Height of UEs", heightOfUes);
  // parse again so you can override default values from the command line
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // create the log directory
  std::string logdir;
  CreateLogDirectory(simName, num_ues, simDuration, ciot, edt, logdir);

  // --------------------------------------------------------------------------
  //
  // log all config arguments
  //
  // --------------------------------------------------------------------------
  // Save all parameters to a log file
  std::ofstream logCmdArgs(logdir + "simulation_config.log");
  if (!logCmdArgs.is_open()) {
    NS_LOG_ERROR("Failed to open log file.");
    return 1;
  }

  logCmdArgs << "Simulation Parameters:\n";
  logCmdArgs << "simDuration = " << simDuration << "\n";
  logCmdArgs << "simName = " << simName << "\n";
  logCmdArgs << "randomSeed = " << seed << "\n";
  logCmdArgs << "num_ues = " << num_ues << "\n";
  logCmdArgs << "ciot = " << (ciot ? "true" : "false") << "\n";
  logCmdArgs << "edt = " << (edt ? "true" : "false") << "\n";
  logCmdArgs << "cell_size = " << cell_size << "\n";
  logCmdArgs << "propagationLossModel = " << propagationLossModel << "\n";
  logCmdArgs << "positioning = " << positioning << "\n";

  // Save all other NS-3 command-line arguments
  logCmdArgs << "\nRaw Command-Line Arguments:\n";
  for (int i = 0; i < argc; ++i) {
    logCmdArgs << argv[i] << " ";
  }
  logCmdArgs << "\n";

  logCmdArgs.close();


  // --------------------------------------------------------------------------
  //
  // redirect log to file
  //
  // --------------------------------------------------------------------------
  const std::string log_fname = "ns3_log_output.log";
  std::ofstream logFile(logdir + log_fname);
  std::streambuf* defaultBuf = std::clog.rdbuf();  // Save original buffer
  // Redirect clog to file
  std::clog.rdbuf(logFile.rdbuf());


  // Component carrier
  // UlBandwidth represents the uplink transmission bandwidth configuration in terms of number of Resource Blocks (RBs)
  Config::SetDefault ("ns3::ComponentCarrier::UlBandwidth", UintegerValue (50));
  // Config::SetDefault ("ns3::ComponentCarrier::DlBandwidth", UintegerValue (50));  // downlink bandwidth in RBs
  Config::SetDefault ("ns3::ComponentCarrier::PrimaryCarrier", BooleanValue (true));  // whether the primary carrier is enabled

  // random seed is set manually here for repetition
  RngSeedManager::SetSeed (seed);
  Ptr<UniformRandomVariable> RaUeUniformVariable = CreateObject<UniformRandomVariable> ();
  NS_LOG_DEBUG("seed: " << seed);

  /*
    ---------------- Create a single eNB  -------------
  */
  NodeContainer enbNodes = create_enb(1, cell_size, 25.0);  // height of eNB is 25m

  // /*
  //   --------------- create UEs ---------------
  // */
  NodeContainer ueNodes = create_ues(num_ues, positioning, cell_size, heightOfUes);

  // configure LTE
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create the EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Enable RRC logging
  lteHelper->EnableRrcLogging ();
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetUeAntennaModelType ("ns3::IsotropicAntennaModel");

  /*
   *
   * --------------- Propagation loss model -------------------
   *
   */
  if (!define_propagation_loss_model (lteHelper, propagationLossModel)) return 1;  // exit program if propagation loss model is not defined

  // Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (false));
  lteHelper->SetAttribute ("UseIdealRrc", BooleanValue (false));
  lteHelper->SetAttribute ("UsePdschForCqiGeneration", BooleanValue (true));

  //Disable Uplink Power Control
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  // This sets the MAC scheduler to Round-Robin (RR) using the RrFfMacScheduler class
  // that allocates resources equally among UEs in a cyclic fashion, without considering channel quality.
  // lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");
  // lteHelper->SetSchedulerAttribute ("UlCqiFilter", EnumValue (FfMacScheduler::PUSCH_UL_CQI));

  // ---------------------- BUGS ------------------------------
  // BUG: I can tell that with the current configuration, LteSpectrumPhy::UpdateSinrPerceived is never called
  //      the calls should be assigned by LteHelper::InstallSingleUeDevice, which is called by lteHelper::InstallUeDevice

  // if CtrlErrorModelEnabled is true, the phy error model is enabled for DL ctrl frame
  // BUG: why CtrlErrorModelEnabled and DataErrorModelEnabled cannot be set to true?
  // Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (true));

  // BUG: raises error an instance of 'std::out_of_range'
  // Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (true));

  // Set the noise figure for UEs and eNodeBs
  // - dont need this because can set in command line using -ns3::LteUePhy::NoiseFigure=x
  // Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue (0));  // Noise figure in dB

  // --------------------- What creates nodes 1 and 2?  ---------------------
  //
  // PointToPointEpcHelper internally creates two more nodes:
  // - MME (Mobility Management Entity)
  // - SGW (Serving Gateway)
  // These two nodes are part of the EPC core and are not directly exposed, but they are still actual Node objects that get registered in the simulator, hence IDs 1 and 2.

  // One Packet Data Network Gateway (PGW) in the simulation
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NS_LOG_INFO("PGW node id: " << pgw->GetId ());  // shows the node id = 0

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  NS_LOG_INFO("Remote host node id: " << remoteHost->GetId ());  // shows the node id = 3

  // Create the Internet
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);
  // point-to-point link between remote host and PGW
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (mtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (channelDelay));
  // place the PGW and the remote host on the same network
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);
  NS_LOG_INFO("Remote host address: " << remoteHostAddr);
  NS_LOG_INFO("PGW address: ");
  for(uint32_t i = 0; i < pgw->GetObject<Ipv4>()->GetNInterfaces(); i++)
  {
    NS_LOG_INFO(" * Interface #" << i << ": " << pgw->GetObject<Ipv4>()->GetAddress(i, 0).GetLocal());
  }

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install LTE Devices to the nodes
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
      NS_LOG_INFO("UE node id: " << ueNode->GetId() << " IP: " << ueNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    }


  // Activate EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);

  // Create a Traffic Flow Template (TFT)
  Ptr<EpcTft> tft = Create<EpcTft>();
  EpcTft::PacketFilter pf;
  pf.localPortStart = 1000;
  pf.localPortEnd = 1000;
  pf.remotePortStart = 1000;
  pf.remotePortEnd = 1000;
  pf.direction = EpcTft::UPLINK;
  tft->Add(pf);

  // Activate the dedicated bearer
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, tft);

  // Install and start applications on UEs and remote host
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  //
  // Log the received packets by the eNB
  //
  // **PROBLEM**:
  //   - if eNBLteDevice->SetReceiveCallback is set, then the callback registered in ueLteDevice->SetReceiveCallback
  //     is not called.
  //   - Only logs the data packets
  //
  // Ptr<LteEnbNetDevice> eNBLteDevice = enbLteDevs.Get(0)->GetObject<LteEnbNetDevice> ();
  // eNBLteDevice->SetReceiveCallback (MakeBoundCallback (&ReceiveEnb, logdir));

  // Set up the data transmission for the Pre-Run
  for (uint16_t i = 0; i < num_ues; i++)
  {
    lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

    // LteUeNetDevice contains GetMac(), GetRrc(), and GetPhy()
    // GetImsi()
    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();

    // Log the received packets
    ueLteDevice->SetReceiveCallback (MakeBoundCallback (&ReceiveCallback, logdir, ueLteDevice->GetImsi()));

    // set NB-IoT module to BG96
    ueRrc->m_energyModel.SetModule(BG96c()); // Set the NBIoT module to BG96
    ueRrc->EnableLogging();
    ueRrc->m_energyModel.SetLogDir(logdir);  // set the log directory for the energy model
    if(ciot == true){
      //std::cout << "ciot" << std::endl;
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

    ++ulPort;
    UdpEchoServerHelper server (ulPort);
    serverApps.Add(server.Install (remoteHost));
    //
    // Create a UdpEchoClient application to send UDP datagrams from node zero to
    // node one.
    //
    if (i < num_ues){
      uint packetsize = packetsize_app;
      Ptr<MarkovUdpClient> ulClient = CreateObject<MarkovUdpClient>();
      ulClient->SetRemote(remoteHostAddr, ulPort);
      ulClient->SetRates(packetinterval_app, packetinterval_app); // INACTIVE and ACTIVE intervals
      ulClient->SetAttribute ("MaxPackets", UintegerValue (1000000));
      ulClient->SetAttribute ("PacketSize", UintegerValue(packetsize));
      ulClient->SetTransitionProbabilities(0.7, 0.2);  // P(INACTIVE→ACTIVE), P(ACTIVE→INACTIVE)
      ulClient->TraceConnectWithoutContext("State", MakeCallback(&StateChangeTracer));

      Ptr<Node> client = ueNodes.Get(i);
      // ulClient->SetNode (client);
      client->AddApplication (ulClient);
      clientApps.Add (ulClient);

      serverApps.Get(i)->SetStartTime (access);
      clientApps.Get(i)->SetStartTime (access);
    }

    // this callback is used to log the UE measurements
    Ptr< LteUePhy > uePhy = ueLteDevice->GetPhy ();
    // uePhy->TraceConnectWithoutContext("ReportUeMeasurements", MakeCallback(&ReportUeMeasurements));
    uePhy->TraceConnectWithoutContext("ReportUeMeasurements", MakeBoundCallback(&ReportUeMeasurements, logdir));

    // uePhy->SetAttribute ("TxPower", DoubleValue (23.0));
    // uePhy->SetAttribute ("NoiseFigure", DoubleValue (9.0));
  }

  lteHelper->SetLogDir(logdir);

  // Log the state changes to a file
  Config::Connect("/NodeList/*/ApplicationList/*/State",
                  MakeBoundCallback(&StateChangeTracerToFile, logdir));

  for (uint16_t i = 0; i < ueNodes.GetN(); i++)
  {
    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
    Ptr<LteUeMac> ueMac = ueLteDevice->GetMac();
    Ptr<LteUePhy> uePhy = ueLteDevice->GetPhy ();

    ueRrc->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier
    ueMac->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier

  }
  NS_LOG_INFO("Number of UEs: " << ueNodes.GetN());

  // Get the eNodeB device
  Ptr<LteEnbNetDevice> enbLteDevice = enbLteDevs.Get(0)->GetObject<LteEnbNetDevice>();
  Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetRrc();
  Ptr<LteEnbPhy> enbPhy = enbLteDevs.Get(0)->GetObject<LteEnbNetDevice>()->GetPhy();
  enbRrc->SetLogDir(logdir);

  // Log the noise figure and tx power to a file
  std::ofstream out(logdir + "NoiseFigure.log", std::ios::app);
  for (uint16_t i = 0; i < ueNodes.GetN(); i++)
  {
    Ptr<LteUePhy> uePhy = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ()->GetPhy ();
    out << "UE " << i << " NoiseFigure: " << uePhy->GetNoiseFigure() << " TxPower: " << uePhy->GetTxPower() << std::endl;
  }
  out << "eNB NoiseFigure: " << enbPhy->GetNoiseFigure() << " TxPower: " << enbPhy->GetTxPower() << std::endl;
  out << "eNB Bandwidth DL: " << enbLteDevice->GetDlBandwidth() << " UL: " << enbLteDevice->GetUlBandwidth() << std::endl;
  out.close();

  /*
    ***********************************
    *
    * Enable traces and pcap
    *
    * *********************************
  */
  NS_LOG_INFO("Generating traces");
  lteHelper->EnableMacTraces();  // Enable MAC traces (to identify transmission patterns)
  lteHelper->EnablePhyTraces();  // Enable Phy traces ()
  lteHelper->EnableDlPhyTraces();

  // lteHelper->EnableRlcTraces();  // Enable RLC traces. RAISES error
  // lteHelper->EnablePdcpTraces(); // Enable PDCP traces. RAISES error

  // enable PCAP tracing
  p2ph.EnablePcapAll(logdir + "lena-simple-epc");


  // NOTE: Code in `lena-pathloss-traces.cc` keeps track of all path loss values in two centralized objects,
  // but does not work here. Config::Connect ("/ChannelList/0/PathLoss",...) does not work.

  // Connect the callback to log IMSI and RNTI when the connection is established
  // This allows us to map RNTI to IMSI
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
    MakeBoundCallback(&ConnectionEstablishedUeCallback, logdir));

  //ensures the PHY uses the NB-IoT spectrum model (180 kHz)
  Config::Set("/NodeList/*/DeviceList/*/LteUePhy/NbIoT", BooleanValue(true));
  Config::Set("/NodeList/*/DeviceList/*/LteEnbPhy/NbIoT", BooleanValue(true));
  // Use overlapping frequency configurations
  lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));


  double updateInterval = 1.0; // update every 1 second
  Simulator::Schedule(Seconds(updateInterval), &MyShowProgress, simDuration.GetSeconds(), updateInterval);

  /*
    ***********************************
    *
    * Start the simulation
    *
    * *********************************
  */
  AnimationInterface anim (logdir + "lena-simple-epc.xml");
  Simulator::Stop (simDuration); // Run
  auto start = std::chrono::system_clock::now();
  std::time_t start_time = std::chrono::system_clock::to_time_t(start);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&start_time), "%Y-%m-%d_%H-%M-%S");
  std::cout << "Started computation at " << ss.str() << std::endl;
  std::cout << "Log file: " << logdir << "/" << log_fname << std::endl;
  std::cout << "Log dir: ";
  Simulator::Run ();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end-start;
  std::time_t end_time = std::chrono::system_clock::to_time_t(end);
  NS_LOG_INFO("Finished computation at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count() << "s" );

  // finilise the simulation
  Simulator::Destroy ();
  std::cout << "\nDone." << std::endl;

  // Restore before logFile is destroyed
  // This ensures std::clog isn’t left pointing to a buffer that’s about to vanish.
  std::clog.rdbuf(defaultBuf);

  return 0;
}

/**
 * Example:


 ./waf --run "nb-scenario4 --simDuration=10 --num_ues=4 \
    --ns3::LteUePhy::RsrpSinrSamplePeriod=1 \
    --ns3::ConstantSpectrumPropagationLossModel::Loss=5 \
    --ns3::LteUePhy::NoiseFigure=5 --ns3::LteUePhy::TxPower=10 \
    --ns3::LteEnbPhy::NoiseFigure=5 --ns3::LteEnbPhy::TxPower=30"

 */
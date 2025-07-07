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
 - simTime: Duration of the simulation.
 - worker: A flag for selecting a specific worker, initialized to 0.
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

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/markov-udp-client.h" // Include the custom MarkovUdpClient header
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/lte-module.h"
#include <ns3/propagation-loss-model.h>
#include <ns3/winner-plus-propagation-loss-model.h>
#include "ns3/log.h"
#include "ns3/nb-iot-energy.h"
#include "ns3/basic-energy-harvester.h"

using namespace ns3;



/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It can also start another flow between each UE pair.


  The log of this script can be processed by `check_scenario3.py` to visualize the transitions of states and energy consumption.
  To run the script, use the following command:
  $ ./waf --run "nb-scenario3.cc" 2>&1 | tee nb-scenario3.log
  $ python py-code/check_scenario3.py --log-fname nb-scenario3.log

 */

NS_LOG_COMPONENT_DEFINE ("LenaNb5G-Cap");


#define GENERATE_TRACES true

/**
 * Tracer function to log state changes to the console.
 * @param oldVal Previous state value.
 * @param newVal New state value.
 */
static void StateChangeTracer(int oldVal, int newVal)
{
  std::cout << Simulator::Now().GetSeconds() << "s: State changed from "
            << oldVal << " to " << newVal << std::endl;
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


/**
 * Main function to set up and run the simulation.
 * It initializes the LTE network, configures UEs and eNBs, and runs applications.
 * @return Exit status of the program.
 */
int main (int argc, char *argv[])
{
  LogLevel logLevel = LOG_LEVEL_INFO; // Set the log level to debug

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


  // --------------------------------------------------------------------------
  //
  // Simulation parameters
  //
  // --------------------------------------------------------------------------
  ns3::Time simTime = Minutes(30);  // Total duration of the simulation
  std::string simName = "markov";  // Name of the simulation, used for logging

  uint8_t worker = 0;
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
  // BUG: if the packet interval is too small (e.g. 1 second), the simulation will crash (on lte-enb-rrc.cc)
  Time packetinterval_app = Seconds(10);

  std::string propagationLossModel = "friis";  // default value is to use simple propagation loss model (Friis)
  double rss = -100.0; // default value for fixed RSSI, used in FixedRssLossModel
  double minLoss = 0.0; // default value for minimum loss, used in FriisPropagationLossModel
  bool ciot = false;
  bool edt = false;
  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("simName", "Name of the simulation", simName);
  cmd.AddValue ("worker", "worker id when using multithreading to not confuse logging", worker);
  cmd.AddValue ("randomSeed", "randomSeed",seed);
  cmd.AddValue ("num_ues", "Number of UEs", num_ues);
  cmd.AddValue ("ciot", "Cellular IoT Optimization",ciot);
  cmd.AddValue ("edt", "Early Data Transmission",edt);
  cmd.AddValue ("cell_size", "Cell size in meters", cell_size);
  cmd.AddValue("propagationLossModel", "Propagation loss model: friis, fixed, or winner", propagationLossModel);
  cmd.AddValue("RSS", "Fixed RSSI in dBm for FixedRssLossModel", rss);
  cmd.AddValue("minLoss", "Minimum loss in dB for FriisPropagationLoss", minLoss);
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line

  // Validate input
  if (propagationLossModel != "friis" &&
      propagationLossModel != "fixed" &&
      propagationLossModel != "winner")
  {
        NS_FATAL_ERROR("Invalid propagationLossModel: must be 'friis', 'fixed', or 'winer'");
  }

  // random seed is set manually here for repetition
  RngSeedManager::SetSeed (seed);
  Ptr<UniformRandomVariable> RaUeUniformVariable = CreateObject<UniformRandomVariable> ();


  // configure LTE
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  lteHelper->EnableRrcLogging ();
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetUeAntennaModelType ("ns3::IsotropicAntennaModel");
  if (propagationLossModel == "friis")
  {
    lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::FriisPropagationLossModel"));
    lteHelper->SetPathlossModelAttribute("MinLoss", DoubleValue (minLoss)); // Set minimum loss to 0 dB
  }
  else if (propagationLossModel == "fixed")
  {
    lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::FixedRssLossModel"));
    lteHelper->SetPathlossModelAttribute("Rss", DoubleValue (rss)); // Set fixed RSSI to -100 dBm
  }
  else if (propagationLossModel == "winner")
  {
    lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::WinnerPlusPropagationLossModel")); // Note that the Winner+ pathloss model isn't available in the current release of ns3. It can be downloaded at https://github.com/tudo-cni/ns3-propagation-winner-plus
    lteHelper->SetPathlossModelAttribute ("HeightBasestation", DoubleValue (50));
    lteHelper->SetPathlossModelAttribute ("Environment", EnumValue (UMaEnvironment));
    lteHelper->SetPathlossModelAttribute ("LineOfSight", BooleanValue (false));
  }

  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (false));
  Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));

  // One PGW node
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
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
  std::cout << "Remote host address: " << remoteHostAddr << std::endl;
  std::cout << "PGW address: " << std::endl;
  for(uint32_t i = 0; i < pgw->GetObject<Ipv4>()->GetNInterfaces(); i++)
  {
    std::cout << " * Interface #" << i << ": " << pgw->GetObject<Ipv4>()->GetAddress(i, 0).GetLocal() << std::endl;
  }

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Create a single eNB
  NodeContainer enbNodes;
  enbNodes.Create (1);
  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  // Place our single eNb right in the center of the cell
  positionAlloc->Add (Vector (cell_size/2, cell_size/2, 25));
  // Install Mobility Model. Fix eNB at the center
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.SetPositionAllocator(positionAlloc);
  mobilityEnb.Install(enbNodes);

  /*
  --------------- create UEs
  */
  NodeContainer ueNodes;
  ueNodes.Create (num_ues); // Pre-Run, Run, Post-Run.
  Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator> ();
  // Install Mobility Model for Application A
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

  // Install Mobility Model
  // Nodes are static. No movement is simulated.
  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityUe.SetPositionAllocator(positionAllocUe);
  mobilityUe.Install (ueNodes);


  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

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


  // Install and start applications on UEs and remote host
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  // --------------------------------------------------------------
  //
  // define log directory used by NS3 modules
  //
  // --------------------------------------------------------------
  auto start = std::chrono::system_clock::now();
  std::time_t start_time = std::chrono::system_clock::to_time_t(start);

  // create the log directory structure
  std::string logdir = "logs/";
  std::string makedir = "mkdir -p ";
  std::string techdir = makedir;

  // create logdir
  techdir += logdir;
  int z = std::system(techdir.c_str());  // mkdir
  NS_LOG(LOG_DEBUG, "cmd: " << techdir << " :" << z);

  // create logdir / simName
  techdir += "/" + simName + "/";
  z = std::system(techdir.c_str());  // mkdir
  NS_LOG_DEBUG("cmd: " << techdir <<" : " << z);

  // logdir / simName / num_ues _ simTime _ ciot _ edt
  logdir += simName;
  logdir += "/u" + std::to_string(ueNodes.GetN());
  logdir += "_t" + std::to_string(simTime.GetInteger());
  logdir += "_c" + std::to_string(ciot);
  logdir += "_e" + std::to_string(edt);
  // create the directory
  techdir = makedir + logdir;
  z = std::system(techdir.c_str());  // mkdir
  NS_LOG_DEBUG("cmd: " << techdir <<" : " << z);

  // create folder with date
  auto tm = *std::localtime(&start_time);
  std::stringstream ss;
  ss << std::put_time(&tm, "%d_%m_%Y_%H_%M_%S");
  logdir += "/" + ss.str();
  techdir = makedir + logdir;
  z = std::system(techdir.c_str());  // mkdir
  NS_LOG_DEBUG("cmd: " << techdir <<" : " << z);

  // define path + initial part of the the log filenames used
  logdir += "/w" + std::to_string(worker);
  logdir += "_s" + std::to_string(seed) + "_";

  // Set up the data transmission for the Pre-Run
  for (uint16_t i = 0; i < num_ues; i++)
    {
      Time access = MilliSeconds(10); // Access delay for the application, in milliseconds
      lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

      Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
      Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
      ueRrc->EnableLogging();
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
        // ulClient->SetTransitionProbabilities(0.7, 0.2);  // P(INACTIVE→ACTIVE), P(ACTIVE→INACTIVE)
        ulClient->TraceConnectWithoutContext("State", MakeCallback(&StateChangeTracer));

        Ptr<Node> client = ueNodes.Get(i);
        // ulClient->SetNode (client);
        client->AddApplication (ulClient);
        clientApps.Add (ulClient);

        serverApps.Get(i)->SetStartTime (access);
        clientApps.Get(i)->SetStartTime (access);
      }
    }

  // Log the state changes to a file
  Config::Connect("/NodeList/*/ApplicationList/*/State",
                  MakeBoundCallback(&StateChangeTracerToFile, logdir));

  /* **********************************
   *
   * Start the simulation
   *
   * **********************************/
  std::cout << "Started computation at " << std::ctime(&start_time);

  for (uint16_t i = 0; i < ueNodes.GetN(); i++)
  {

    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
    Ptr<LteUeMac> ueMac = ueLteDevice->GetMac();
    ueRrc->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier
    ueMac->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier

  }
  lteHelper->SetLogDir(logdir);

  Ptr<LteEnbNetDevice> enbLteDevice = enbLteDevs.Get(0)->GetObject<LteEnbNetDevice>();
  Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetRrc();
  enbRrc->SetLogDir(logdir);

  // Ptr<LteEnbMac> enbMac = enbLteDevice->GetMac();
  // enbMac->SetLogDir(logdir);  // private !!

  std::cout << "Number of UEs: " << ueNodes.GetN() << std::endl;

  #if GENERATE_TRACES
  lteHelper->EnableMacTraces();  // Enable MAC traces (to identify transmission patterns)
  lteHelper->EnablePhyTraces();  // Enable Phy traces ()
  // lteHelper->EnableRlcTraces();  // Enable RLC traces. RAISES error
  // lteHelper->EnablePdcpTraces(); // Enable PDCP traces. RAISES error

  // enable PCAP tracing
  p2ph.EnablePcapAll(logdir + "lena-simple-epc");
  #endif

  Simulator::Stop (simTime); // Run
  std::cout << "Log dir: ";
  Simulator::Run ();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end-start;
  std::time_t end_time = std::chrono::system_clock::to_time_t(end);
  std::cout << "Finished computation at " << std::ctime(&end_time);
  std::cout << "elapsed time: " << elapsed_seconds.count() << "s" << std::endl;
  Simulator::Destroy ();
  return 0;
}

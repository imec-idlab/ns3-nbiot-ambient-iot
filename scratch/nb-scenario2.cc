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
 - simTime: Duration of the simulation, set to 3 seconds.
 - worker: A flag for selecting a specific worker, initialized to 0.
 - seed: The seed for random number generation, initialized to 1.
 - simName: The name of the simulation, set to "cap".
 - cellsize: The size of the cell in meters, set to 2500 meters.
 - num_ues: Number of User Equipments (UEs) per application, set to 1.
 - packetsize_app_a: The packet size for Application A, set to 49 bytes (32 bytes payload + headers).
 - packetinterval_app_a: The interval between packets for Application A, set to 1 day.
 The script intends to create a network setup with eNodeBs (base stations) and UEs (user devices),
 establishing communication flows between the UEs and a remote host.

 It uses a generic-capacitor as power source.


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
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/lte-module.h"
#include <ns3/winner-plus-propagation-loss-model.h>
//#include "ns3/gtk-config-store.h"
#include "ns3/log.h"
#include "ns3/nb-iot-energy.h"
#include "ns3/basic-energy-harvester.h"
#include "ns3/generic-capacitor.h"

using namespace ns3;



/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It can also start another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("LenaNb5G-Cap");



int
main (int argc, char *argv[])
{

  // TODO: expand the parser to read more configuration parameters from the command line and make this code more general

  ns3::LogComponentEnable("LenaNb5G-Cap", LOG_LEVEL_INFO);
  ns3::LogComponentDisable("LenaNb5G-Cap", LOG_LEVEL_DEBUG);
  // ns3::LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);
  ns3::LogComponentEnable("GenericCapacitor", LOG_LEVEL_INFO);
  ns3::LogComponentEnable("EnergySource", LOG_LEVEL_INFO);
  ns3::LogComponentDisable("EnergySource", LOG_LEVEL_DEBUG);

  ns3::Time simTime = Minutes(6);
  // ns3::Time simTime = Seconds(3);

  uint8_t worker = 0;
  int seed = 1;
  std::string simName = "cap";
  double cellsize = 2500; // in meters

  // Number of UEs per application
  int num_ues = 1;  // For now, 1 UE talks to a remote host via one eNB

  // 32 Bytes 5G mMTC payload + 4 Bytes CoAP Header + 13 Bytes DTLS Header
  // UDP Header and IP Header  are added by NS-3
  int packetsize_app_a = 49;

  // Packet interval
  Time packetinterval_app_a = Days(1);

  bool ciot = false;
  bool edt = false;
  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("simName", "Total duration of the simulation", simName);
  cmd.AddValue ("worker", "worker id when using multithreading to not confuse logging", worker);
  cmd.AddValue ("randomSeed", "randomSeed",seed);
  cmd.AddValue ("numUeAppA", "Number of UEs",num_ues);
  cmd.AddValue ("ciot", "Cellular IoT Optimization",ciot);
  cmd.AddValue ("edt", "Early Data Transmission",edt);
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line

  // configure LTE
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  lteHelper->EnableRrcLogging ();
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetUeAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::WinnerPlusPropagationLossModel")); // Note that the Winner+ pathloss model isn't available in the current release of ns3. It can be downloaded at https://github.com/tudo-cni/ns3-propagation-winner-plus
  lteHelper->SetPathlossModelAttribute ("HeightBasestation", DoubleValue (50));
  lteHelper->SetPathlossModelAttribute ("Environment", EnumValue (UMaEnvironment));
  lteHelper->SetPathlossModelAttribute ("LineOfSight", BooleanValue (false));
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
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
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
  positionAlloc->Add (Vector (cellsize/2, cellsize/2, 25));
  // Install Mobility Model. Fix eNB at the center
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.SetPositionAllocator(positionAlloc);
  mobilityEnb.Install(enbNodes);


  /*
  --------------- create UEs


  For all scenarios, 3*X minutes of simulation time are simulated,
  but only the intermediate X minutes are evaluated.
  The first X minutes produce no significant results since devices at the beginning
  are scheduled in an empty cell and experience very good transmission conditions.
  After X minutes, new devices will find ongoing transmissions of previous devices,
  which enables a more realistic situation and produces significant results.
  Since devices that have started transmissions within the intermediate X minutes
  of the simulation may not complete their transmissions in this intermediate time slot,
  additional X minutes are simulated with more new transmissions to keep the channels busy
  and let the intermediate devices complete their transmissions.
  */
  NodeContainer ueNodes;
  ueNodes.Create (num_ues*3); // Pre-Run, Run, Post-Run.
  Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator> ();

  for (uint32_t j = 0; j<3; j++){ // Pre-Run, Run, Post-Run.
    // Install Mobility Model for Application A
    ObjectFactory pos_a;
    pos_a.SetTypeId ("ns3::UniformDiscPositionAllocator");
    pos_a.Set ("X", StringValue (std::to_string(cellsize/2)));
    pos_a.Set ("Y", StringValue (std::to_string(cellsize/2)));
    pos_a.Set ("Z", DoubleValue (1.5));
    pos_a.Set ("rho", DoubleValue (cellsize/2));
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
  RngSeedManager::SetSeed (seed);
  Ptr<UniformRandomVariable> RaUeUniformVariable = CreateObject<UniformRandomVariable> ();


  // Install and start applications on UEs and remote host
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;


  // Set up the data transmission for the Pre-Run
  for (uint16_t i = 0; i < num_ues; i++)
    {
      int access = RaUeUniformVariable->GetInteger (50, simTime.GetMilliSeconds());
      lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

      Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
      Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
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
      uint packetsize = packetsize_app_a;
      UdpEchoClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (packetinterval_app_a));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
      ulClient.SetAttribute ("PacketSize", UintegerValue(packetsize));
      clientApps.Add (ulClient.Install (ueNodes.Get(i)));

      serverApps.Get(i)->SetStartTime (MilliSeconds (access));
      clientApps.Get(i)->SetStartTime (MilliSeconds (access));
    }


  // Set up the data transmission for the UEs to be considered in the results
  for (uint16_t i = num_ues; i < num_ues*2; i++)
    {
      int access = RaUeUniformVariable->GetInteger (simTime.GetMilliSeconds(), 2*simTime.GetMilliSeconds());
      lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

      Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
      Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();

      // -------------------------------------------------------------------
      //
      // Energy
      //
      // -------------------------------------------------------------------
      //
      // TODO: create a harvester that simulates solar panels (basic-solar-energy-harvester.cc)

      Ptr<ns3::Node> node = ueNodes.Get(i);  // node to install

      Ptr<GenericCapacitor> capacitor = CreateObject<GenericCapacitor> ();
      capacitor->SetInitialVoltage(3.3);
      capacitor->SetEnergyUpdateInterval(MilliSeconds(10));
      ueRrc->m_energyModel.SetEnergySource(capacitor);
      capacitor->SetNode(ueNodes.Get(i));  // you MUST set the node to the capacitor

      // create harverster to charge the capacitor
      // BasicEnergyHarvester provides a random energy value in an interval (default is from 0 to 2 W)
      Ptr<ns3::BasicEnergyHarvester> harvester = CreateObject<ns3::BasicEnergyHarvester>();
      // set the distribution of the harvested energy
      // Ptr<ns3::BasicEnergyHarvester> harvester = CreateObjectWithAttributes<ns3::BasicEnergyHarvester> (
      //   // "HarvestablePower", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")
      //   "HarvestablePower", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]")
      // );
      harvester->SetAttribute("HarvestablePower", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
      // harvester->SetAttribute("HarvestablePower", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=0.000001]"));
      harvester->SetHarvestedPowerUpdateInterval (MilliSeconds (10));
      capacitor->ConnectEnergyHarvester(harvester);
      harvester->SetNode(node);
      harvester->SetEnergySource(capacitor);

      node->AggregateObject(harvester);
      node->AggregateObject(capacitor);

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
        uint packetsize = packetsize_app_a;
        UdpEchoClientHelper ulClient (remoteHostAddr, ulPort);
        ulClient.SetAttribute ("Interval", TimeValue (packetinterval_app_a));
        ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
        ulClient.SetAttribute ("PacketSize", UintegerValue(packetsize));
        clientApps.Add (ulClient.Install (ueNodes.Get(i)));

        serverApps.Get(i)->SetStartTime (MilliSeconds (access));
        clientApps.Get(i)->SetStartTime (MilliSeconds (access));
      }
    }



  // Set up the data transmission for the Post-Run
  for (uint16_t i = num_ues*2; i < num_ues*3; i++)
  {
    int access = RaUeUniformVariable->GetInteger (simTime.GetMilliSeconds()*2, simTime.GetMilliSeconds()*3);
    lteHelper->AttachSuspendedNb(ueLteDevs.Get(i), enbLteDevs.Get(0));

    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
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
    if (i < num_ues*2){
      uint packetsize = packetsize_app_a;
      UdpEchoClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (packetinterval_app_a));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
      ulClient.SetAttribute ("PacketSize", UintegerValue(packetsize));
      clientApps.Add (ulClient.Install (ueNodes.Get(i)));

      serverApps.Get(i)->SetStartTime (MilliSeconds (access));
      clientApps.Get(i)->SetStartTime (MilliSeconds (access));
    }
  }

  /* **********************************
   * Start the simulation
   */
  auto start = std::chrono::system_clock::now();
  std::time_t start_time = std::chrono::system_clock::to_time_t(start);
  std::cout << "Started computation at " << std::ctime(&start_time);

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
  logdir += "/" + std::to_string(ueNodes.GetN());
  logdir += "_" + std::to_string(simTime.GetInteger());
  logdir += "_" + std::to_string(ciot);
  logdir += "_" + std::to_string(edt);
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
  logdir += "/" + std::to_string(worker);
  logdir += "_" + std::to_string(seed) + "_";

  for (uint16_t i = 0; i < ueNodes.GetN(); i++)
  {

    Ptr<LteUeNetDevice> ueLteDevice = ueLteDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
    Ptr<LteUeMac> ueMac = ueLteDevice->GetMac();
    ueRrc->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier
    ueMac->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier

  }
  Ptr<LteEnbNetDevice> enbLteDevice = enbLteDevs.Get(0)->GetObject<LteEnbNetDevice>();
  Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetRrc();
  enbRrc->SetLogDir(logdir);
  lteHelper->SetLogDir(logdir);

  std::cout << "Number of UEs: " << ueNodes.GetN() / 3 << " at each stage" << std::endl;

  //lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  //p2ph.EnablePcapAll("lena-simple-epc");

  Simulator::Stop (3*simTime); // Pre-Run, Run, Post-Run
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

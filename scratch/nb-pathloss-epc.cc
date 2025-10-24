#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/epc-tft.h"
#include "ns3/mobility-module.h"
#include <ns3/log.h>


using namespace ns3;

constexpr double PI = 3.14159265358979323846;

// change the line below to #define to create a trace for the pathloss
// however, it is not working with epcHelper.
// it raises Could not connect callback to /ChannelList/0/PathLoss
#undef PATHLOSS_TRACE

#define progname "NbPathlossEpc"
NS_LOG_COMPONENT_DEFINE (progname);


/**
 * Create a specified number of eNBs with a given positioning strategy.
 * @param num_enbs The number of eNBs to create. Should be 1.
 * @param cell_size The size of the cell in meters.
 * @param heightOfEnb The height of the eNB in meters.
 * @return A NodeContainer containing all the created eNBs.
 */
NodeContainer create_enb(double cell_size, double heightOfEnb)
{
  NodeContainer enbNodes;

  enbNodes.Create (1);  // should be 1

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


int main(int argc, char *argv[])
{
  Time simTime = Seconds(50.0);

  // Create LTE and EPC helpers
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // NOTE: the PropagationLoss trace source of the SpectrumChannel
  // works only for single-frequency path loss model.
  // e.g., it will work with the following models:
  // ns3::FriisPropagationLossModel,
  // ns3::TwoRayGroundPropagationLossModel,
  // ns3::LogDistancePropagationLossModel,
  // ns3::ThreeLogDistancePropagationLossModel,
  // ns3::NakagamiPropagationLossModel
  // ns3::BuildingsPropagationLossModel
  // etc.
  // but it WON'T work if you ONLY use SpectrumPropagationLossModels such as:
  // ns3::FriisSpectrumPropagationLossModel
  // ns3::ConstantSpectrumPropagationLossModel
  lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::Cost231PropagationLossModel"));


  Ptr<Node> pgw = epcHelper->GetPgwNode();

  // Create remote host (acts as Internet or application server)
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHost);

  // connect remoteHost to PGW
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
  p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
      ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Create eNodeB and UE
  NodeContainer ueNodes = create_ues(1, "same", 1000, 1.5);
  NodeContainer enbNodes = create_enb(1000, 10);

  // Install LTE Devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install IP stack on UE
  internet.Install(ueNodes);
  ns3::Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ns3::NetDeviceContainer(ueLteDevs));

  // Attach UE to eNB (creates default bearer)
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Define a specific EPS bearer with QCI 9 (non-GBR)
  ns3::EpsBearer bearer(ns3::EpsBearer::NGBR_VIDEO_TCP_DEFAULT); // QCI 9 — typical for NB-IoT
  bearer.arp.priorityLevel = 15;
  bearer.arp.preemptionCapability = false;
  ns3::Ptr<ns3::EpcTft> tft = ns3::EpcTft::Default();

  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(0), bearer, tft);

  // Create a UDP server on remoteHost (e.g., cloud or app server)
  uint16_t port = 9000;
  UdpServerHelper udpServer(port);
  ApplicationContainer serverApps = udpServer.Install(remoteHost);
  serverApps.Start(Seconds(0.1));
  serverApps.Stop(simTime);

  // Create a UDP client on UE that sends to remoteHost
  UdpClientHelper udpClient(remoteHostAddr, port);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 pkt/sec — IoT-like
  udpClient.SetAttribute("PacketSize", UintegerValue(50));     // small payload

  ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(simTime);

  // Enable traces (optional)
  lteHelper->EnableTraces();

  #ifdef PATHLOSS_TRACE
  // keep track of all path loss values in two centralized objects
  DownlinkLteGlobalPathlossDatabase dlPathlossDb;
  UplinkLteGlobalPathlossDatabase ulPathlossDb;
  // we rely on the fact that LteHelper creates the DL channel object first, then the UL channel object,
  // hence the former will have index 0 and the latter 1
  Config::Connect ("/ChannelList/0/PathLoss",
                   MakeCallback (&DownlinkLteGlobalPathlossDatabase::UpdatePathloss, &dlPathlossDb));
  Config::Connect ("/ChannelList/1/PathLoss",
                    MakeCallback (&UplinkLteGlobalPathlossDatabase::UpdatePathloss, &ulPathlossDb));
  #else
    std::cout << "No pathloss tracing enabled" << std::endl;
  #endif

  // Run simulation
  Simulator::Stop(simTime);
  Simulator::Run();

  #ifdef PATHLOSS_TRACE
  // print the pathloss values at the end of the simulation
  std::cout << std::endl << "Downlink pathloss:" << std::endl;
  dlPathlossDb.Print ();
  std::cout << std::endl << "Uplink pathloss:" << std::endl;
  ulPathlossDb.Print ();
  #endif

  Simulator::Destroy();

  // Print results
  Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(0));
  std::cout << "Packets received: " << server->GetReceived() << std::endl;

  return 0;
}

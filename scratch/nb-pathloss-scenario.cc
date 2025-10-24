/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 */

#include <numbers>
#include <iomanip>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store.h"
#include "ns3/radio-bearer-stats-calculator.h"
#include "ns3/lte-global-pathloss-database.h"

#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

#include <ns3/log.h>
#include "ns3/netanim-module.h"

#include "ns3/markov-udp-client.h"  // Include the custom MarkovUdpClient header
#include "ns3/udp-echo-helper.h"



using namespace ns3;

constexpr double PI = 3.14159265358979323846;
#define progname "NbPathlossScenario"

NS_LOG_COMPONENT_DEFINE (progname);



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

/**
 * Redirects the log output of the ns3::Log component
 * to a file in the given directory.
 */
class RedirectLogToFile
{
public:
  /**
   * Constructor that redirects the log output of the ns3::Log component
   * to a file in the given directory.
   *
   * @param logdir The directory where the log file should be written.
   *
   * This constructor will redirect the logging output to a file named
   * "ns3_log_output.log" in the given directory. Any existing log output
   * will be written to this file instead of being written to the console.
   */
  RedirectLogToFile(std::string logdir) {
    defaultBuf = std::clog.rdbuf();  // Save original buffer

    // Redirect clog to file
    m_logFile.open(logdir + m_log_fname);
    std::clog.rdbuf(m_logFile.rdbuf());
    std::cout << "Logging to file: " << logdir + m_log_fname << std::endl;
    std::clog << "Logging started" << std::endl;
  }

  // This ensures std::clog isn’t left pointing to a buffer that’s about to vanish.
  void Close() {
    std::clog.rdbuf(defaultBuf);
    if (m_logFile.is_open()) {
      m_logFile.close();
    }
  }

private:
  std::streambuf* defaultBuf;
  std::ofstream m_logFile;
  std::string m_log_fname = "ns3_log_output.log";
};


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

void setLogLevels (LogLevel logLevel = LOG_LEVEL_INFO)
{
  ns3::LogComponentEnable(progname, logLevel);
  ns3::LogComponentDisable(progname, LOG_LEVEL_DEBUG);

  // ns3::LogComponentEnable ("LteUeRrc", logLevel);
  // ns3::LogComponentEnable ("LteUeMac", logLevel);
  // ns3::LogComponentEnable ("LteUePhy", logLevel);

  // ns3::LogComponentEnable ("LteEnbRrc", logLevel);
  // ns3::LogComponentEnable ("LteEnbMac", logLevel);
  // ns3::LogComponentEnable ("LteEnbPhy", logLevel);

  // ns3::LogComponentEnable ("LteSpectrumPhy", logLevel);
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
  NS_LOG_INFO (ss.str());

  return true;
}

// Callback wrapper
void SinrTraceCallback(uint64_t imsi, Ptr<ns3::SpectrumValue> sinr)
{
  std::cout << "SinrTraceCallback IMSI: " << imsi << " SINR: " << std::endl;
}

/**
 * Callback function for logging the SINR of a UE.
 *
 * This function is triggered when a UE reports its SINR. It logs the
 * details of the SINR, including IMSI and the SINR value, to the
 * NS-3 logging system.
 *
 * @param imsi The International Mobile Subscriber Identity of the UE.
 * @param sinr The SINR value reported by the UE.
 */
void LteSinrTraceCallback(uint16_t imsi, Ptr<ns3::SpectrumValue> sinr)
{
  std::cout << "LteSinrTraceCallback IMSI: " << imsi << " SINR: " << std::endl;
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
  ---------------------------------------------------------------------

                                 MAIN

  ---------------------------------------------------------------------
*/
int main (int argc, char *argv[])
{
  // Set the log level to debug
  setLogLevels(LOG_LEVEL_INFO);

  ns3::Time simTime = Seconds(30);  // Total duration of the simulation

  double enbDist = 20.0;
  double cell_size = 1000; // in meters
  double enb_height = 25.0; // in meters

  // Number of UEs per application
  int num_ues = 4;  // For now, 4 UEs talk to a remote host via one eNB
  double heightOfUes = 1.5; // height of the UEs
  std::string positioning = "uniform"; // positioning strategy for UEs: "same", "uniform", or "random"

  std::string logdir = "logs/pathloss/";  // Note: must end with a slash
  std::cout << "Logging to directory: " << logdir << std::endl;
  // create the log directory if it does not exist
  std::string makedir = "mkdir -p " + logdir;
  int z = std::system(makedir.c_str());
  NS_LOG_DEBUG("cmd: " << makedir << " : " << z);

  // Redirect log output to a file
  RedirectLogToFile redirectLogToFile(logdir);
  redirectLogToFile.Close();


  CommandLine cmd (__FILE__);
  cmd.AddValue ("enbDist", "distance between the two eNBs", enbDist);
  cmd.AddValue ("radius", "the radius of the disc where UEs are placed around an eNB", cell_size);
  cmd.AddValue ("numUes", "how many UEs are attached to each eNB", num_ues);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse (argc, argv);

  // determine the string tag that identifies this simulation run
  // this tag is then appended to all filenames

  UintegerValue runValue;
  GlobalValue::GetValueByName ("RngRun", runValue);

  std::ostringstream tag;
  tag  << "_enbDist" << std::setw (3) << std::setfill ('0') << std::fixed << std::setprecision (0) << enbDist
       << "_radius"  << std::setw (3) << std::setfill ('0') << std::fixed << std::setprecision (0) << cell_size
       << "_numUes"  << std::setw (3) << std::setfill ('0')  << num_ues
       << "_rngRun"  << std::setw (3) << std::setfill ('0')  << runValue.Get () ;

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();


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


  // Create Nodes: eNodeB and UE & position them
  NodeContainer enbNodes = create_enb(cell_size, enb_height);
  NodeContainer ueNodes = create_ues(num_ues, positioning, cell_size, heightOfUes);

  // Create Devices and install them in the Nodes (eNB and UE)
  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  // Create Devices and install them in the Nodes (eNB and UE)
  enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  ueDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach UEs to a eNB
  lteHelper->Attach (ueDevs, enbDevs.Get (0));

  std::cout << "EPS-only scenario" << std::endl;
  // Activate an EPS bearer on all UEs
  // Qci defines standardized traffic types (or profiles) defined in 3GPP TS 23.203, TS 23.401, and TS 23.501 for mapping application types to suitable EPS bearers.
  // https://www.nsnam.org/docs/release/3.16/doxygen/structns3_1_1_eps_bearer.html#aecf0c67109c5eb4ec0b07226fff5885e
  // enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;  // Conversational Voice
  enum EpsBearer::Qci q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;  // Standard best-effort video streaming and web
  EpsBearer bearer (q);
  bearer.arp.priorityLevel = 15;  // lowest ARP priority (optional)
  lteHelper->ActivateDataRadioBearer (ueDevs, bearer);

  // Set up the data transmission for the Pre-Run
  // Set the Nb-IoT module to BG96
  for (uint16_t i = 0; i < num_ues; i++)
  {
    std::cout << " Setting up UE " << i << std::endl;
    // LteUeNetDevice contains GetMac(), GetRrc(), GetPhy(), GetImsi()
    Ptr<LteUeNetDevice> ueLteDevice = ueDevs.Get(i)->GetObject<LteUeNetDevice> ();
    Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();
    Ptr<LteUeMac> ueMac = ueLteDevice->GetMac();

    // Log the received packets
    ueLteDevice->SetReceiveCallback (MakeBoundCallback (&ReceiveCallback, logdir, ueLteDevice->GetImsi()));
    // set NB-IoT module to BG96
    ueRrc->m_energyModel.SetModule(BG96c()); // Set the NBIoT module to BG96
    ueRrc->EnableLogging();
    ueRrc->m_energyModel.SetLogDir(logdir);  // set the log directory for the energy model
    ueRrc->SetAttribute("CIoT-Opt", BooleanValue(false));
    ueRrc->SetAttribute("EDT", BooleanValue(false));
    ueRrc->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier
    ueMac->SetLogDir(logdir); // Will be changed to real ns3 traces later on. For now this logging is easier

    // BUG: this callback is never called with the current implementation
    Ptr< LteUePhy > uePhy = ueLteDevice->GetPhy ();
    uePhy->TraceConnectWithoutContext("ReportUeSinr", MakeCallback(&SinrTraceCallback));
    // uePhy->SetAttribute ("TxPower", DoubleValue (23.0));
    // uePhy->SetAttribute ("NoiseFigure", DoubleValue (9.0));
  }

  std::cout << "Starting simulation" << std::endl;

  AnimationInterface anim (logdir + "neatanin-" + progname + ".xml");
  Simulator::Stop (simTime);

  // Insert RLC Performance Calculator
  std::string dlOutFname = "DlRlcStats";
  dlOutFname.append (tag.str ());
  std::string ulOutFname = "UlRlcStats";
  ulOutFname.append (tag.str ());

  // Enable logging
  lteHelper->SetLogDir(logdir);
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  lteHelper->EnableRrcLogging ();
  lteHelper->EnableDlPhyTraces();

  Ptr<LteEnbNetDevice> enbLteDevice = enbDevs.Get(0)->GetObject<LteEnbNetDevice>();
  Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetRrc();
  enbRrc->SetLogDir(logdir);


  // Connect the callback to log IMSI and RNTI when the connection is established
  // This allows us to map RNTI to IMSI
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
    MakeBoundCallback(&ConnectionEstablishedUeCallback, logdir));

  //ensures the PHY uses the NB-IoT spectrum model (180 kHz)
  Config::Set("/NodeList/*/DeviceList/*/LteUePhy/NbIoT", BooleanValue(true));
  Config::Set("/NodeList/*/DeviceList/*/LteEnbPhy/NbIoT", BooleanValue(true));

  // keep track of all path loss values in two centralized objects
  DownlinkLteGlobalPathlossDatabase dlPathlossDb;
  UplinkLteGlobalPathlossDatabase ulPathlossDb;
  // we rely on the fact that LteHelper creates the DL channel object first, then the UL channel object,
  // hence the former will have index 0 and the latter 1
  Config::Connect ("/ChannelList/0/PathLoss",
                   MakeCallback (&DownlinkLteGlobalPathlossDatabase::UpdatePathloss, &dlPathlossDb));
  Config::Connect ("/ChannelList/1/PathLoss",
                    MakeCallback (&UplinkLteGlobalPathlossDatabase::UpdatePathloss, &ulPathlossDb));

  Simulator::Run ();
  std::cout << "Simulation finished" << std::endl;

  // print the pathloss values at the end of the simulation
  std::cout << std::endl << "Downlink pathloss:" << std::endl;
  dlPathlossDb.Print ();
  std::cout << std::endl << "Uplink pathloss:" << std::endl;
  ulPathlossDb.Print ();

  Simulator::Destroy ();

  // Restore before logFile is destroyed
  redirectLogToFile.Close();

  NS_LOG_INFO("Done");
  return 0;
}

/**
 * Example:
 *
 * ./waf --run "nb-pathloss-scenario.cc --simTime=10"
 */

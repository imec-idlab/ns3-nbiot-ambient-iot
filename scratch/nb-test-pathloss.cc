/**
 * \file nb-test-pathloss.cc
 * \brief This program tests the pathloss model HybridBuildingsPropagationLossModel
 * by measuring the SINR at a distance given by m_distance. However, it does not catch the value of SINR (at all!).
 * This program is based on `lte-test-pathloss-model.cc`.
 *
 * \Author: Henrique Moura (henrique.duartemoura@imec.be)
 */

#include "ns3/log.h"
#include <ns3/enum.h>
#include "ns3/string.h"
#include <ns3/boolean.h>
#include <ns3/integer.h>
#include <ns3/double.h>
#include <ns3/config.h>
#include <ns3/mobility-helper.h>
#include <ns3/lte-helper.h>
#include <ns3/node-container.h>
#include <ns3/net-device-container.h>
#include <ns3/lte-ue-net-device.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-enb-phy.h>
#include <ns3/lte-ue-phy.h>
#include <ns3/hybrid-buildings-propagation-loss-model.h>
#include "ns3/simulator.h"
#include "ns3/test.h"
#include <ns3/buildings-helper.h>
#include "ns3/lte-chunk-processor.h"



using namespace ns3;

int
main (int argc, char *argv[])
{
  double m_distance = 10.0; ///< the distance

  Config::SetDefault ("ns3::MacStatsCalculator::DlOutputFilename", StringValue ("DlMacStats.txt"));
  Config::SetDefault ("ns3::MacStatsCalculator::UlOutputFilename", StringValue ("UlMacStats.txt"));
  Config::SetDefault ("ns3::RadioBearerStatsCalculator::DlRlcOutputFilename", StringValue ("DlRlcStats.txt"));
  Config::SetDefault ("ns3::RadioBearerStatsCalculator::UlRlcOutputFilename", StringValue ("UlRlcStats.txt"));
  /**
  * Simulation Topology
  */
  //Disable Uplink Power Control
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  //   lteHelper->EnableLogComponents ();
  lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::HybridBuildingsPropagationLossModel"));

  // set frequency. This is important because it changes the behavior of the path loss model
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (200));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18200));
  lteHelper->SetUeDeviceAttribute ("DlEarfcn", UintegerValue (200));

  // remove shadowing component
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaOutdoor", DoubleValue (0.0));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaIndoor", DoubleValue (0.0));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaExtWalls", DoubleValue (0.0));

  // Create Nodes: eNodeB and UE
  NodeContainer enbNodes;
  NodeContainer ueNodes;
  enbNodes.Create (1);
  ueNodes.Create (1);
  NodeContainer allNodes = NodeContainer ( enbNodes, ueNodes );

  // Install Mobility Model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);
  BuildingsHelper::Install (allNodes);


  // Create Devices and install them in the Nodes (eNB and UE)
  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;
  lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");
  enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  ueDevs = lteHelper->InstallUeDevice (ueNodes);

  Ptr<MobilityModel> mm_enb = enbNodes.Get (0)->GetObject<MobilityModel> ();
  mm_enb->SetPosition (Vector (0.0, 0.0, 30.0));
  Ptr<MobilityModel> mm_ue = ueNodes.Get (0)->GetObject<MobilityModel> ();
  mm_ue->SetPosition (Vector (m_distance, 0.0, 1.0));

  Ptr<LteEnbNetDevice> lteEnbDev = enbDevs.Get (0)->GetObject<LteEnbNetDevice> ();
  Ptr<LteEnbPhy> enbPhy = lteEnbDev->GetPhy ();
  enbPhy->SetAttribute ("TxPower", DoubleValue (30.0));
  enbPhy->SetAttribute ("NoiseFigure", DoubleValue (5.0));

  Ptr<LteUeNetDevice> lteUeDev = ueDevs.Get (0)->GetObject<LteUeNetDevice> ();
  Ptr<LteUePhy> uePhy = lteUeDev->GetPhy ();
  uePhy->SetAttribute ("TxPower", DoubleValue (23.0));
  uePhy->SetAttribute ("NoiseFigure", DoubleValue (9.0));


  // Attach a UE to a eNB
  lteHelper->Attach (ueDevs, enbDevs.Get (0));

  // Activate an EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueDevs, bearer);

  // Use testing chunk processor in the PHY layer
  // It will be used to test that the SNR is as intended
  //Ptr<LtePhy> uePhy = ueDevs.Get (0)->GetObject<LteUeNetDevice> ()->GetPhy ()->GetObject<LtePhy> ();
  Ptr<LteChunkProcessor> testSinr = Create<LteChunkProcessor> ();
  LteSpectrumValueCatcher sinrCatcher;
  testSinr->AddCallback (MakeCallback (&LteSpectrumValueCatcher::ReportValue, &sinrCatcher));
  uePhy->GetDownlinkSpectrumPhy ()->AddCtrlSinrChunkProcessor (testSinr);

//   Config::Connect ("/NodeList/0/DeviceList/0/LteEnbMac/DlScheduling",
//                    MakeBoundCallback (&LteTestPathlossDlSchedCallback, this));

  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();

  Simulator::Stop (Seconds (1));
  Simulator::Run ();

  Ptr<SpectrumValue> value = sinrCatcher.GetValue();
  if (value) {
    double calculatedSinrDb = 10.0 * std::log10 (value->operator[] (0));
    std::cout << "Distance " << m_distance << " Calculated SINR " << calculatedSinrDb << std::endl;
  } else {
    std::cout << "⚠ No value from catcher" << std::endl;
  }
  Simulator::Destroy ();

  return 0;
}
#include "mmpp-traffic-app.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MmppTrafficApp");
NS_OBJECT_ENSURE_REGISTERED(MmppTrafficApp);

MmppTrafficApp::MmppTrafficApp()
  : m_socket(0),
    m_sendEvent(),
    m_currentState(LOW),
    m_lambdaLow(10.0),
    m_lambdaHigh(100.0),
    m_pLowToHigh(0.1),
    m_pHighToLow(0.2)
{
  m_uniformRv = CreateObject<UniformRandomVariable>();
}

MmppTrafficApp::~MmppTrafficApp()
{
  m_socket = 0;
}

void MmppTrafficApp::SetRemote(Address address)
{
  m_peer = address;
}

void MmppTrafficApp::SetRates(double lowRate, double highRate)
{
  m_lambdaLow = lowRate;
  m_lambdaHigh = highRate;
}

void MmppTrafficApp::SetTransitionProbabilities(double pLowToHigh, double pHighToLow)
{
  m_pLowToHigh = pLowToHigh;
  m_pHighToLow = pHighToLow;
}

void MmppTrafficApp::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_peer);
  m_currentState = LOW;
  ScheduleNextPacket();
}

void MmppTrafficApp::StopApplication()
{
  if (m_sendEvent.IsRunning())
    Simulator::Cancel(m_sendEvent);
  if (m_socket)
    m_socket->Close();
}

void MmppTrafficApp::ScheduleNextPacket()
{
  Time t = Seconds(GetInterArrivalTime());
  m_sendEvent = Simulator::Schedule(t, &MmppTrafficApp::SendPacket, this);
}

void MmppTrafficApp::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(1024);
  m_socket->Send(packet);
  UpdateState();
  ScheduleNextPacket();
}

void MmppTrafficApp::UpdateState()
{
  double r = m_uniformRv->GetValue();
  if (m_currentState == LOW)
    m_currentState = (r < m_pLowToHigh) ? HIGH : LOW;
  else
    m_currentState = (r < m_pHighToLow) ? LOW : HIGH;
}

double MmppTrafficApp::GetInterArrivalTime()
{
  double lambda = (m_currentState == LOW) ? m_lambdaLow : m_lambdaHigh;
  return -std::log(1.0 - m_uniformRv->GetValue()) / lambda;
}

} // namespace ns3
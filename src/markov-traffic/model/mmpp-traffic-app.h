#ifndef MMPP_TRAFFIC_APP_H
#define MMPP_TRAFFIC_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/random-variable-stream.h"
#include "ns3/address.h"


namespace ns3 {

class MmppTrafficApp : public Application
{
public:
  MmppTrafficApp();
  virtual ~MmppTrafficApp();

  void SetRemote(Address address);
  void SetRates(double lowRate, double highRate);
  void SetTransitionProbabilities(double pLowToHigh, double pHighToLow);

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  enum State { LOW, HIGH };
  void ScheduleNextPacket();
  void SendPacket();
  void UpdateState();
  double GetInterArrivalTime();

  Ptr<Socket> m_socket;
  Address m_peer;
  EventId m_sendEvent;

  State m_currentState;
  double m_lambdaLow;
  double m_lambdaHigh;
  double m_pLowToHigh;
  double m_pHighToLow;

  Ptr<UniformRandomVariable> m_uniformRv;
};

} // namespace ns3

#endif // MMPP_TRAFFIC_APP_H
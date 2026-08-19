/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 * Copyright (c) 2026 IDLab (UAntwerp & imec)
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 * Modified by: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/global-value.h"
#include "ns3/seq-ts-header.h"
#include "markov-udp-client.h"
#include <cstdlib>
#include <cstdio>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MarkovUdpClient");

NS_OBJECT_ENSURE_REGISTERED (MarkovUdpClient);

// Gate the brown-out pause/resume traces behind the same --NbIotDebugTrace=1
// runtime toggle as the LTE-module diagnostics. Looked up by GlobalValue name
// so this module needs no lte dependency; resolves to false if the global is
// absent. Cached once (after CommandLine::Parse).
static bool
MarkovDebugTrace ()
{
  static bool cached = [] {
    BooleanValue b (false);
    GlobalValue::GetValueByNameFailSafe ("NbIotDebugTrace", b);
    return b.Get ();
  }();
  return cached;
}

TypeId
MarkovUdpClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MarkovUdpClient")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MarkovUdpClient> ()
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&MarkovUdpClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("InactiveInterval",
                   "The time to wait between packets in the inactive state", TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&MarkovUdpClient::m_intervalInactive),
                   MakeTimeChecker ())
    .AddAttribute ("ActiveInterval",
                   "The time to wait between packets in the active state", TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&MarkovUdpClient::m_intervalActive),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&MarkovUdpClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&MarkovUdpClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize",
                   "Size of packets generated. The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&MarkovUdpClient::m_size),
                   MakeUintegerChecker<uint32_t> (12,65507))
    .AddAttribute ("TransitionProbabilityInactiveToActive",
                   "Probability of transitioning from the inactive state to the active state",
                   DoubleValue (0.7),
                   MakeDoubleAccessor (&MarkovUdpClient::m_pInactiveToActive),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("TransitionProbabilityActiveToInactive",
                   "Probability of transitioning from the active state to the inactive state",
                   DoubleValue (0.1),
                   MakeDoubleAccessor (&MarkovUdpClient::m_pActiveToInactive),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("SendFirst",
                   "If true, always send a packet then decide next state (send-first mode). "
                   "If false, decide state first then send only when active (original mode).",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MarkovUdpClient::m_sendFirst),
                   MakeBooleanChecker ())
    .AddTraceSource("State",
      "Markov State: 0 = INACTIVE, 1 = ACTIVE",
      MakeTraceSourceAccessor(&MarkovUdpClient::m_stateTrace),
      "ns3::TracedValueCallback::Int32")
  ;
  return tid;
}

MarkovUdpClient::MarkovUdpClient ()
  : m_sent(0),
    m_state(INACTIVE),
    m_intervalInactive(Seconds(0.1)),
    m_intervalActive(Seconds(0.01)),
    m_pInactiveToActive(0.7),
    m_pActiveToInactive(0.1),
    m_sendFirst(false)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_sendEvent = EventId ();
  m_uniformRv = CreateObject<UniformRandomVariable>();
}

MarkovUdpClient::~MarkovUdpClient ()
{
  NS_LOG_FUNCTION (this);
}

void
MarkovUdpClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void
MarkovUdpClient::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

void
MarkovUdpClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
MarkovUdpClient::SetRates(Time inactiveTime, Time activeTime)
{
  m_intervalInactive = inactiveTime;
  m_intervalActive = activeTime;
}

void
MarkovUdpClient::SetTransitionProbabilities(double inactiveToActive, double activeToInactive)
{
  m_pInactiveToActive = inactiveToActive;
  m_pActiveToInactive = activeToInactive;
}


void
MarkovUdpClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else if (Inet6SocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else
        {
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
        }
    }

  m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  m_socket->SetAllowBroadcast (true);
  // Front-load the first packet into the start jitter window. The app StartTime is
  // already jittered within [10, 510] ms per UE (scenario), and the first Send()
  // fires at that instant (offset 0.0 below). Seeding the Markov state to ACTIVE
  // makes that first Send() transmit immediately, instead of the state machine
  // starting INACTIVE and slipping the first packet a full inactive interval
  // (~300 s) later. This makes the one-time RRC cold-start happen up front so the
  // remainder of the run is steady state (contention-free under FUG). Subsequent
  // packets follow the normal sendFirst=false decide-then-send cadence.
  m_state = ACTIVE;
  m_pendingIsWakeUp = false;
  m_sendEvent = Simulator::Schedule (Seconds (0.0), &MarkovUdpClient::Send, this);
}

void
MarkovUdpClient::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
}

void
MarkovUdpClient::SendPacket (void)
{
  SeqTsHeader seqTs;
  seqTs.SetSeq (m_sent);
  Ptr<Packet> p = Create<Packet> (m_size-(8+4)); // 8+4 : the size of the seqTs header
  p->AddHeader (seqTs);

  std::stringstream peerAddressStringStream;
  if (Ipv4Address::IsMatchingType (m_peerAddress))
  {
    peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
  }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
  {
    peerAddressStringStream << Ipv6Address::ConvertFrom (m_peerAddress);
  }

  if ((m_socket->Send (p)) >= 0)
  {
    ++m_sent;
    if (Simulator::Now () >= m_statsStart && Simulator::Now () <= m_statsEnd)
      ++m_sentWin;                                      // exclude warm-up + tail generation
    NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
      << peerAddressStringStream.str () << " Uid: "
      << p->GetUid () << " Time: "
      << (Simulator::Now ()).As (Time::S));
  }
  else
  {
    NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
      << peerAddressStringStream.str ());
  }
}

void
MarkovUdpClient::Send (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());

  // Brown-out gate: when paused by the energy front-end, swallow this tick
  // and reschedule one inactive interval later so we wake up to check again.
  if (m_paused)
    {
      if (MarkovDebugTrace ())
        std::cerr << "[APP-TICK-PAUSED] node=" << (GetNode () ? GetNode ()->GetId () : 9999)
                  << " t=" << Simulator::Now ().GetSeconds () << std::endl;
      if (m_sent < m_count)
        {
          m_pendingIsWakeUp = false;
          m_sendEvent = Simulator::Schedule (m_intervalInactive,
                                             &MarkovUdpClient::Send, this);
        }
      return;
    }

  if (m_sendFirst)
    {
      // Send-first mode: always send, then decide next state.
      // ACTIVE  -> send, wait activeInterval, send again
      // INACTIVE -> wait inactiveInterval, re-decide (WakeUp)
      SendPacket ();
      if (m_sent < m_count)
        {
          UpdateState ();
          if (m_state == ACTIVE)
            {
              m_pendingIsWakeUp = false;
              m_sendEvent = Simulator::Schedule (m_intervalActive, &MarkovUdpClient::Send, this);
            }
          else
            {
              m_pendingIsWakeUp = true;
              m_sendEvent = Simulator::Schedule (m_intervalInactive, &MarkovUdpClient::WakeUp, this);
            }
        }
    }
  else
    {
      // Original mode: decide state at fixed intervals, only send when ACTIVE
      if (m_state == ACTIVE)
        {
          SendPacket ();
        }
      UpdateState ();
      if (m_sent < m_count)
        {
          m_pendingIsWakeUp = false;
          m_sendEvent = Simulator::Schedule (GetNextInterval (), &MarkovUdpClient::Send, this);
        }
    }
}

void
MarkovUdpClient::WakeUp (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());

  UpdateState ();
  if (m_state == ACTIVE)
    {
      Send ();
    }
  else if (m_sent < m_count)
    {
      m_pendingIsWakeUp = true;
      m_sendEvent = Simulator::Schedule (m_intervalInactive, &MarkovUdpClient::WakeUp, this);
    }
}

void
MarkovUdpClient::UpdateState()
{
  double r = m_uniformRv->GetValue();
  if (m_state == INACTIVE)
    m_state = (r < m_pInactiveToActive) ? ACTIVE : INACTIVE;
  else
    m_state = (r < m_pActiveToInactive) ? INACTIVE : ACTIVE;

  m_stateTrace = static_cast<int>(m_state);
}

Time
MarkovUdpClient::GetNextInterval()
{
  return (m_state == INACTIVE) ? m_intervalInactive : m_intervalActive;
}

void
MarkovUdpClient::Pause()
{
  if (m_paused) return;
  m_paused = true;
  if (MarkovDebugTrace ())
    std::cerr << "[APP-PAUSE] node=" << (GetNode () ? GetNode ()->GetId () : 9999)
              << " t=" << Simulator::Now ().GetSeconds () << std::endl;
  // Remember how much of the tick was left so Resume() can preserve phase.
  // Re-arming a FULL interval instead livelocks under brown-out oscillation:
  // any pause period shorter than the tick interval resets the timer forever
  // and the client silently generates nothing (seen on fug-rr N=1).
  m_pausedDelayLeft = m_sendEvent.IsExpired () ? Time::Min ()
                                               : Simulator::GetDelayLeft (m_sendEvent);
  Simulator::Cancel (m_sendEvent);
}

void
MarkovUdpClient::Resume()
{
  if (!m_paused) return;
  m_paused = false;
  if (MarkovDebugTrace ())
    std::cerr << "[APP-RESUME] node=" << (GetNode () ? GetNode ()->GetId () : 9999)
              << " t=" << Simulator::Now ().GetSeconds () << std::endl;
  if (m_sent < m_count)
    {
      // Re-arm the tick Pause() cancelled with the delay it had LEFT, and the
      // SAME handler (send-first mode parks WakeUp in m_sendEvent; re-arming
      // Send there would mint a packet). Do NOT mint a new packet on
      // recovery: a packet generated before the brown-out is still buffered in
      // the RLC and is delivered by the access layer once the radio is back.
      // Injecting a fresh packet here (new SeqTs, ++m_sent) double-counts the
      // offered load and inflates the loss ratio for any scheme that depletes,
      // confounding the cross-scheme comparison.
      Time delay = m_pausedDelayLeft.IsNegative () ? GetNextInterval ()
                                                   : m_pausedDelayLeft;
      m_pausedDelayLeft = Time::Min ();
      if (m_pendingIsWakeUp)
        {
          m_sendEvent = Simulator::Schedule (delay, &MarkovUdpClient::WakeUp, this);
        }
      else
        {
          m_sendEvent = Simulator::Schedule (delay, &MarkovUdpClient::Send, this);
        }
    }
}


} // Namespace ns3

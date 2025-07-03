/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
#include "ns3/seq-ts-header.h"
#include "markov-udp-client.h"
#include <cstdlib>
#include <cstdio>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MarkovUdpClient");

NS_OBJECT_ENSURE_REGISTERED (MarkovUdpClient);

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
    .AddAttribute ("Interval",
                   "The time to wait between packets", TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&MarkovUdpClient::m_interval),
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
    m_intervalLow(Seconds(0.1)),
    m_intervalHigh(Seconds(0.01)),
    m_pLowToHigh(0.1),
    m_pHighToLow(0.2)
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
MarkovUdpClient::SetRates(Time low, Time high)
{
  m_intervalLow = low;
  m_intervalHigh = high;
}

void
MarkovUdpClient::SetTransitionProbabilities(double lowToHigh, double highToLow)
{
  m_pLowToHigh = lowToHigh;
  m_pHighToLow = highToLow;
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
  m_sendEvent = Simulator::Schedule (Seconds (0.0), &MarkovUdpClient::Send, this);
}

void
MarkovUdpClient::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
}

void
MarkovUdpClient::Send (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());
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

  UpdateState();  // after sending a packet, update the state using the traffic model

  if (m_sent < m_count)
    {
      Time next = GetNextInterval();

      m_sendEvent = Simulator::Schedule (next, &MarkovUdpClient::Send, this);
    }
}

void
MarkovUdpClient::UpdateState()
{
  double r = m_uniformRv->GetValue();
  if (m_state == INACTIVE)
    m_state = (r < m_pLowToHigh) ? ACTIVE : INACTIVE;
  else
    m_state = (r < m_pHighToLow) ? INACTIVE : ACTIVE;

  m_stateTrace = static_cast<int>(m_state);
}

Time
MarkovUdpClient::GetNextInterval()
{
  return (m_state == INACTIVE) ? m_intervalLow : m_intervalHigh;
}


} // Namespace ns3

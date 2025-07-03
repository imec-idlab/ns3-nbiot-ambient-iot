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
 * Author: Henrique Duarte Moura <henrique.duartemoura@imec.be>
 *
 */

#ifndef MARKOV_UDP_CLIENT_H
#define MARKOV_UDP_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-value.h"


namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup udpclientserver
 *
 * \brief A Udp client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class MarkovUdpClient : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  MarkovUdpClient ();

  virtual ~MarkovUdpClient ();

  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Address ip, uint16_t port);
  /**
   * \brief set the remote address
   * \param addr remote address
   */
  void SetRemote (Address addr);

  /**
   * \brief Set the rates for the Markov model.
   * \param inactiveTime Time between packets in the inactive state
   * \param activeTime Time between packets in the active state
   */
  void SetRates(Time inactiveTime, Time activeTime);

  /**
   * @brief Sets the probability of transitioning from the inactive state to the active state.
   *
   * @param inactiveToActive Probability of transitioning from the inactive state to the active state.
    * @param activeToInactive Probability of transitioning from the active state to the inactive state.
   */
  void SetTransitionProbabilities(double inactiveToActive, double activeToInactive);

protected:
  virtual void DoDispose (void);

  void UpdateState();   // Update the state of the Markov model based on the transition probabilities
  Time GetNextInterval();  // Get the next interval based on the current state. Used for scheduling the next packet send.

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Send a packet
   */
  void Send (void);

  uint32_t m_count; //!< Maximum number of packets the application will send
  Time m_interval; //!< Packet inter-send time
  uint32_t m_size; //!< Size of the sent packet (including the SeqTsHeader)

  uint32_t m_sent; //!< Counter for sent packets
  Ptr<Socket> m_socket; //!< Socket
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port
  EventId m_sendEvent; //!< Event to send the next packet

  // Markov model parameters
  enum State { INACTIVE, ACTIVE };
  State m_state;
  Time m_intervalInactive;
  Time m_intervalActive;
  double m_pInactiveToActive;
  double m_pActiveToInactive;

  Ptr<UniformRandomVariable> m_uniformRv;

  TracedValue<int> m_stateTrace;
};

} // namespace ns3

#endif /* MARKOV_UDP_CLIENT_H */

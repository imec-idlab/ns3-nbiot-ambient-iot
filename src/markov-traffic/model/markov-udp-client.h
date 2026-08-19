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
 * Author: Henrique Duarte Moura <henrique.duartemoura@imec.be>
 * Modified by: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>
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

  /**
   * @brief Pause Markov ticking. Cancels the next send event without tearing
   *        down the socket. Idempotent. Used by the energy front-end to
   *        suppress traffic generation while a UE is browned out.
   */
  void Pause();

  /**
   * @brief Resume Markov ticking. Re-arms the event cancelled by Pause() with
   *        the delay it had LEFT (phase-preserving), so repeated short
   *        brown-outs cannot keep resetting a long tick timer and starve the
   *        traffic source. Falls back to a full interval if nothing was
   *        pending. Idempotent.
   */
  void Resume();

  bool IsPaused() const { return m_paused; }

  /**
   * @brief Number of packets this client has generated/transmitted. With the
   *        server's GetReceived() this gives exact app-level loss (sent-received),
   *        independent of the FlowMonitor 10 s timeout.
   */
  uint32_t GetSent() const { return m_sent; }

  /**
   * @brief Warm-up cutoff: packets generated before this time are excluded from
   *        the windowed counters (so the first synchronized cold-start RA does
   *        not bias steady-state loss/delay/throughput). Default 0 = full run.
   */
  void SetStatsStartTime (Time t) { m_statsStart = t; }
  /** @brief End of the steady-state window: packets generated AFTER this are
   *  excluded from the sent counter (they have too little time to be delivered
   *  before sim end -> would inflate loss). Default Max = no end cutoff. */
  void SetStatsEndTime (Time t) { m_statsEnd = t; }
  /** @brief Packets generated within the [start,end] window. */
  uint32_t GetSentWindow () const { return m_sentWin; }

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

  /**
   * \brief Create and transmit a single UDP packet
   */
  void SendPacket (void);

  /**
   * \brief Re-evaluate state after an inactive period (send-first mode only)
   */
  void WakeUp (void);

  uint32_t m_count; //!< Maximum number of packets the application will send
  Time     m_statsStart {Seconds (0)};   //!< warm-up cutoff for the windowed sent counter
  Time     m_statsEnd {Time::Max ()};    //!< tail cutoff; packets generated after are excluded
  uint32_t m_sentWin {0};                //!< packets generated within [m_statsStart, m_statsEnd]
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

  bool m_sendFirst; //!< If true, always send then decide; if false, original interval-based behavior
  bool m_paused {false}; //!< Pause flag set by Pause()/Resume() (energy brown-out gate)
  bool m_pendingIsWakeUp {false}; //!< m_sendEvent holds WakeUp (send-first mode), not Send
  Time m_pausedDelayLeft {Time::Min ()}; //!< delay left on the tick cancelled by Pause(); negative = none

  Ptr<UniformRandomVariable> m_uniformRv;

  TracedValue<int> m_stateTrace;
};

} // namespace ns3

#endif /* MARKOV_UDP_CLIENT_H */

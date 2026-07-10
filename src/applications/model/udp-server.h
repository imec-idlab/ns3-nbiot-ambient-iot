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
 *
 */

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "packet-loss-counter.h"
#include <vector>

namespace ns3 {
/**
 * \ingroup applications
 * \defgroup udpclientserver UdpClientServer
 */

/**
 * \ingroup udpclientserver
 *
 * \brief A UDP server, receives UDP packets from a remote host.
 *
 * UDP packets carry a 32bits sequence number followed by a 64bits time
 * stamp in their payloads. The application uses the sequence number
 * to determine if a packet is lost, and the time stamp to compute the delay.
 */
class UdpServer : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  UdpServer ();
  virtual ~UdpServer ();
  /**
   * \brief Returns the number of lost packets
   * \return the number of lost packets
   */
  uint32_t GetLost (void) const;

  /**
   * \brief Returns the number of received packets
   * \return the number of received packets
   */
  uint64_t GetReceived (void) const;

  /**
   * \brief Returns the size of the window used for checking loss.
   * \return the size of the window used for checking loss.
   */
  uint16_t GetPacketWindowSize () const;

  /**
     * \return the total bytes received in this sink app
     */
    uint64_t GetTotalRx() const;

  /**
   * \brief Sum of per-packet end-to-end delays (RX time - SeqTs TX time) over
   * all received packets. Exact app-level delay: no FlowMonitor timeout/cap.
   * \return the accumulated delay
   */
  Time GetDelaySum (void) const;
  /**
   * \return mean per-packet delay = GetDelaySum()/GetReceived() (0 if none)
   */
  Time GetMeanDelay (void) const;

  /**
   * \brief Warm-up cutoff: packets whose SeqTs TX timestamp is before this time
   * are excluded from the windowed received/delay/bytes counters, so the first
   * cold-start RA does not bias steady-state metrics. Default 0 = full run.
   */
  void SetStatsStartTime (Time t);
  /** @brief Tail cutoff: received packets whose generation (SeqTs TX) time is
   *  AFTER this are excluded from the windowed counters, symmetric with the
   *  client, so undeliverable last-epoch packets don't bias loss. Max = off. */
  void SetStatsEndTime (Time t);
  uint64_t GetReceivedWindow (void) const;   //!< received packets generated at/after the cutoff
  Time     GetDelaySumWindow (void) const;    //!< delay sum over those packets
  uint64_t GetTotalRxWindow (void) const;     //!< bytes received over those packets
  /** @brief Per-packet end-to-end delays [ms] of the windowed packets, in
   *  reception order. 
   */
  const std::vector<double>& GetDelaysWindow (void) const { return m_delaysWin; }

  /**
   * \brief Set the size of the window used for checking loss. This value should
   *  be a multiple of 8
   * \param size the size of the window used for checking loss. This value should
   *  be a multiple of 8
   */
  void SetPacketWindowSize (uint16_t size);
protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  uint16_t m_port; //!< Port on which we listen for incoming packets.
  Ptr<Socket> m_socket; //!< IPv4 Socket
  Ptr<Socket> m_socket6; //!< IPv6 Socket
  uint64_t m_received; //!< Number of received packets
  uint64_t m_totalRx;   //!< Total bytes received
  Time m_delaySum;     //!< Accumulated per-packet delay (RX - SeqTs TX time)
  Time m_statsStart {Seconds (0)}; //!< warm-up cutoff (on SeqTs TX time)
  Time m_statsEnd {Time::Max ()};  //!< tail cutoff (on SeqTs TX time); Max = off
  uint64_t m_receivedWin {0};      //!< received packets generated at/after m_statsStart
  Time     m_delaySumWin {Seconds (0)}; //!< delay sum over windowed packets
  uint64_t m_totalRxWin {0};       //!< bytes received over windowed packets
  std::vector<double> m_delaysWin; //!< per-packet windowed delays [ms] (percentiles/CDF)
  PacketLossCounter m_lossCounter; //!< Lost packet counter

  /// Callbacks for tracing the packet Rx events
  TracedCallback<Ptr<const Packet> > m_rxTrace;

  /// Callbacks for tracing the packet Rx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;

};

} // namespace ns3

#endif /* UDP_SERVER_H */

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2022 Communication Networks Institute at TU Dortmund University
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
 * Author: Nicola Baldo  <nbaldo@cttc.es>
 * Author: Marco Miozzo <mmiozzo@cttc.es>
 * Modified by: 
 *          Tim Gebauer <tim.gebauer@tu-dortmund.de> (NB-IoT Extension)
 *          Pascal Jörke <pascal.joerke@tu-dortmund.de> (NB-IoT Extension)
 *          Douglas D. Agbeve <douglas.agbeve@uantwerpen.be> (NB-IoT Extension)
 */

#ifndef LTE_UE_MAC_ENTITY_H
#define LTE_UE_MAC_ENTITY_H



#include <map>

#include <ns3/lte-mac-sap.h>
#include <ns3/lte-ue-cmac-sap.h>
#include <ns3/lte-ue-phy-sap.h>
#include "lte-control-messages.h"
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <vector>
#include <ns3/packet.h>
#include <ns3/packet-burst.h>
#include <ns3/traced-callback.h>


namespace ns3 {

class UniformRandomVariable;

class LteUeMac :   public Object
{
  /// allow UeMemberLteUeCmacSapProvider class friend access
  friend class UeMemberLteUeCmacSapProvider;
  /// allow UeMemberLteMacSapProvider class friend access
  friend class UeMemberLteMacSapProvider;
  /// allow UeMemberLteUePhySapUser class friend access
  friend class UeMemberLteUePhySapUser;

public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  LteUeMac ();
  virtual ~LteUeMac ();
  virtual void DoDispose (void);

  /**
   * \brief TracedCallback signature for RA response timeout events
   * exporting IMSI, contention flag, preamble transmission counter
   * and the max limit of preamble transmission
   *
   * \param [in] imsi
   * \param [in] contention
   * \param [in] preambleTxCounter
   * \param [in] maxPreambleTxLimit
   */
  typedef void (* RaResponseTimeoutTracedCallback)
    (uint64_t imsi, bool contention, uint8_t preambleTxCounter, uint8_t maxPreambleTxLimit);

  /**
  * \brief Get the LTE MAC SAP provider
  * \return a pointer to the LTE MAC SAP provider
  */
  LteMacSapProvider*  GetLteMacSapProvider (void);
  /**
  * \brief Set the LTE UE CMAC SAP user
  * \param s the LTE UE CMAC SAP User
  */
  void  SetLteUeCmacSapUser (LteUeCmacSapUser* s);
  /**
  * \brief Get the LTE CMAC SAP provider
  * \return a pointer to the LTE CMAC SAP provider
  */
  LteUeCmacSapProvider*  GetLteUeCmacSapProvider (void);
  
  /**
  * \brief Set the component carried ID
  * \param index the component carrier ID
  */
  void SetComponentCarrierId (uint8_t index);

  /**
  * \brief Get the PHY SAP user
  * \return a pointer to the SAP user of the PHY
  */
  LteUePhySapUser* GetLteUePhySapUser ();

  /**
  * \brief Set the PHY SAP Provider
  * \param s a pointer to the PHY SAP Provider
  */
  void SetLteUePhySapProvider (LteUePhySapProvider* s);
  
  /**
  * \brief Forwarded from LteUePhySapUser: trigger the start from a new frame
  *
  * \param frameNo frame number
  * \param subframeNo subframe number
  */
  void DoSubframeIndication (uint32_t frameNo, uint32_t subframeNo);

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

  void SetLogDir(std::string dirname);

  void LogMessage(std::string msg);

  /**Cross-layer ideal BSR callback: (rnti, bufferBytes) → eNB MAC handler.
   **/
  typedef Callback<void, uint16_t, uint64_t> IdealBsrCallback;

  void SetIdealBsrCallback (IdealBsrCallback cb);

  // Access Stratum Release Assistance Indication (AS RAI, TS 36.321 5.4.5 for
  // NB-IoT; rai-Activation-r14 in MAC-MainConfig-NB). Fired when a buffer size
  // of zero bytes is triggered for the BSR with no subsequent UL/DL data -- the
  // eNB then releases immediately instead of waiting on the data-inactivity
  // timer. Modeled as the AS RAI MAC CE (~1 B, carried on the UL like the BSR).
  typedef Callback<void, uint16_t> RaiCallback;
  void SetRaiCallback (RaiCallback cb);

  // Dedicated SR on the REAL NPRACH (TS 36.331 SchedulingRequestConfig-NB). On
  // connect the UE registers (rnti, srIndex) with the eNB (models the RRC config
  // exchange); thereafter, on its round-robin dedicated occasion, it transmits a
  // preamble on its reserved subcarrier carrying NO identity. The eNB resolves
  // the C-RNTI from its own resource->UE map and grants DCI N0.
  typedef Callback<void, uint16_t, uint32_t> SrConfigCallback;   // (rnti, srIndex)
  void SetSrConfigCallback (SrConfigCallback cb);
  void SetSrDedicated (uint32_t srIndex, uint32_t reservedSubcarriers,
                       uint32_t contentionOffset);
  void SetSrHybridContention (bool en);   ///< also contend on shared subcarriers while waiting for the reserved slot
  void SetSrEdtContention (bool en);      ///< contention-SR attempts use the EDT NPRACH partition (data rides Msg3)
  void SetOracleBsr (bool en);            ///< oracle / ideal BSR: eNB learns the buffer instantly (no SR delay/energy)
  uint64_t GetPendingUlBytes () { return GetBufferSizeComplete (); } ///< diagnostic: pending UL buffer (in-flight/backlog at sim end)
  /// diagnostic breakdown of the pending UL buffer: tx (never sent) / retx (sent, unACKed) / status (signalling residue)
  void GetPendingUlBreakdown (uint64_t &tx, uint64_t &retx, uint64_t &status) {
    tx = retx = status = 0;
    for (auto &kv : m_ulBsrReceived) {
      tx += kv.second.txQueueSize; retx += kv.second.retxQueueSize; status += kv.second.statusPduSize;
    }
  }
  /**
   * Enable/disable the persistent-grant ideal-BSR hook.
   **/
  void SetPersistentGrant (bool enable);

  // Dedicated-NPRACH Scheduling Request (Rel-15 SchedulingRequestConfig-NB):
  // period, in subframes, of this UE's dedicated SR opportunities. When the
  // UE has UL data it transmits an SR at the next occasion (contention-free,
  // energy-charged) instead of the removed zero-cost ideal-BSR oracle.
  void SetSrPeriod (uint32_t subframes);

  // Model 1 (deep-sleep + SR-resurrect): when enabled, a FUG UE with nothing in
  // flight sleeps in PSM (context kept, ~15 uW) instead of monitoring NPDCCH at
  // the connected floor (~3 mW). It wakes via the dedicated SR to monitor for
  // its grant, then sleeps again once the buffer drains.
  void SetDeepSleepFug (bool enable);

  // Connected-mode DRX (TS 36.321 5.7; DRX-Config-NB-r13 in MAC-MainConfig-NB,
  // with drx-Cycle-v1430 extending the long cycle to sf10240 = 10.24 s). A
  // connected FUG UE that is NOT deep-sleeping only opens NPDCCH search-space
  // occasions during the on-duration of each DRX cycle; the rest of the cycle it
  // drops to the connected idle floor instead of monitoring every search-space
  // occasion. Energy-only DL gating: uplink (data -> SR) is unaffected.
  // cycleSubframes: long DRX cycle in subframes; inactivityMs: drx-InactivityTimer.
  void SetCdrx (bool enable, uint32_t cycleSubframes, uint32_t inactivityMs);

  // Proactive Fast Uplink Grant (standalone 4th mode). The UE sends NO SR in
  // steady state -- the eNB predicts its period and pushes grants. Reuses the
  // deep-sleep/resume infra; the SR subsystem is cleanly disabled after a short
  // reactive bootstrap (see DoReportBufferStatus).
  void SetProactiveFug (bool enable);

  // C-RNTI MAC CE path (TS 36.321 5.1.5 / 6.1.3.2). Arm with the UE's existing
  // C-RNTI before a connected-UE Random Access: the UE then carries a C-RNTI MAC
  // CE in Msg3 and keeps its identity (instead of adopting the Temporary C-RNTI).
  // Mechanism only -- not wired into the FUG resume; armed explicitly for tests.
  void SetRaKeepCrnti (uint16_t crnti);

  // Contention-resolution failure (TS 36.321 5.1.5): a UE that lost the shared
  // Temporary C-RNTI (Msg4 not addressed to it) must back off before its next
  // Random Access. The RRC calls this when it discards a mismatched Msg4; the
  // current backoff parameter is carried into the upcoming re-RACH.
  void NotifyContentionResolutionFailedNb ();


private:
  // forwarded from MAC SAP
 /**
  * Transmit PDU function
  *
  * \param params LteMacSapProvider::TransmitPduParameters
  */
  void DoTransmitPdu (LteMacSapProvider::TransmitPduParameters params);
 /**
  * Report buffers status function
  *
  * \param params LteMacSapProvider::ReportBufferStatusParameters
  */
  void DoReportBufferStatus (LteMacSapProvider::ReportBufferStatusParameters params);
  void DoReportBufferStatusNb (LteMacSapProvider::ReportBufferStatusParameters params, NbIotRrcSap::NpdcchMessage::SearchSpaceType searchspace);

  // forwarded from UE CMAC SAP
 /**
  * Configure RACH function
  *
  * \param rc LteUeCmacSapProvider::RachConfig
  */
  void DoConfigureRach (LteUeCmacSapProvider::RachConfig rc);
 /**
  * Configure RACH function
  *
  * \param rc LteUeCmacSapProvider::RachConfig
  */
  void DoConfigureRadioResourceConfig (NbIotRrcSap::RadioResourceConfigCommonNb rc);
 /**
  * Start contention based random access procedure function
  */
  void DoStartContentionBasedRandomAccessProcedure ();
 /**
  * Start contention based random access procedure function
  */
  void DoStartRandomAccessProcedureNb (bool edt);
 /**
  * Set RNTI
  *
  * \param rnti the RNTI
  */
  void DoSetRnti (uint16_t rnti);
 /**
  * Start non contention based random access procedure function
  *
  * \param rnti the RNTI
  * \param rapId the RAPID
  * \param prachMask the PRACH mask
  */
  void DoStartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t rapId, uint8_t prachMask);
 /**
  * Add LC function
  *
  * \param lcId the LCID
  * \param lcConfig the logical channel config
  * \param msu the MSU
  */
  void DoAddLc (uint8_t lcId, LteUeCmacSapProvider::LogicalChannelConfig lcConfig, LteMacSapUser* msu);
 /**
  * Remove LC function
  *
  * \param lcId the LCID
  */
  void DoRemoveLc (uint8_t lcId);
  /**
   * \brief Reset function
   */
  void DoReset ();
  /**
   * \brief Notify MAC about the successful RRC connection
   * establishment.
   */
  void DoNotifyConnectionSuccessful ();

  /**
   * Set IMSI
   *
   * \param imsi the IMSI of the UE
   */
  void DoSetImsi (uint64_t imsi);

  // forwarded from PHY SAP
 /**
  * Receive Phy PDU function
  *
  * \param p the packet
  */
  void DoReceivePhyPdu (Ptr<Packet> p);
 /**
  * Receive LTE control message function
  *
  * \param msg the LTE control message
  */
  void DoReceiveLteControlMessage (Ptr<LteControlMessage> msg);

  void DoNotifyAboutHarqOpportunity (std::vector<std::pair<uint64_t,std::vector<uint64_t>>> subframes);
  
  // internal methods
  /// Randomly select and send RA preamble function
  void RandomlySelectAndSendRaPreamble ();

  // internal methods
  /// Randomly select and send RA preamble function
  void RandomlySelectAndSendRaPreambleNb ();
 /**
  * Send RA preamble function
  *
  * \param contention if true randomly select and send the RA preamble
  */
  void SendRaPreamble (bool contention);
  void SendRaPreambleNb (bool contention);
  /// Start waiting for RA response function
  void StartWaitingForRaResponse ();
/// Start waiting for RA response function
  void StartWaitingForRaResponseNb ();
 /**
  * Receive the RA response function
  *
  * \param raResponse RA response received
  */
  void RecvRaResponse (BuildRarListElement_s raResponse);
 /**
  * Receive the RA response function
  *
  * \param raResponse RA response received
  */
  void RecvRaResponseNb (NbIotRrcSap::RarPayload raResponse);
 /**
  * RA response timeout function
  *
  * \param contention if true randomly select and send the RA preamble
  */
  void RaResponseTimeout (bool contention);
 /**
  * RA response timeout function
  *
  * \param contention if true randomly select and send the RA preamble
  */
  void RaResponseTimeoutNb (bool contention);
  /// Send report buffer status
  void SendReportBufferStatus (void);
  /// Refresh HARQ processes packet buffer function
  void RefreshHarqProcessesPacketBuffer (void);

  uint64_t GetBufferSize();
  uint64_t GetBufferSizeComplete();

  void DoSetTransmissionScheduled(bool scheduled);

  void DoNotifyEdrx();
  void DoNotifyPsm();
  void DoSetMsg5Buffer(uint32_t buffersize);

  

  NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel DoGetCoverageEnhancementLevel();
  /// component carrier Id --> used to address sap
  uint8_t m_componentCarrierId;

private:

  /// LcInfo structure
  struct LcInfo
  {
    LteUeCmacSapProvider::LogicalChannelConfig lcConfig; ///< logical channel config
    LteMacSapUser* macSapUser; ///< MAC SAP user
  };

  std::map <uint8_t, LcInfo> m_lcInfoMap; ///< logical channel info map

  LteMacSapProvider* m_macSapProvider; ///< MAC SAP provider

  LteUeCmacSapUser* m_cmacSapUser; ///< CMAC SAP user
  LteUeCmacSapProvider* m_cmacSapProvider; ///< CMAC SAP provider

  LteUePhySapProvider* m_uePhySapProvider; ///< UE Phy SAP provider
  LteUePhySapUser* m_uePhySapUser; ///< UE Phy SAP user
  
  std::map <uint8_t, LteMacSapProvider::ReportBufferStatusParameters> m_ulBsrReceived; ///< BSR received from RLC (the last one)
  
  
  Time m_bsrPeriodicity; ///< BSR periodicity
  Time m_bsrLast; ///< BSR last
  
  bool m_freshUlBsr; ///< true when a BSR has been received in the last TTI

  uint8_t m_harqProcessId; ///< HARQ process ID
  std::vector < Ptr<PacketBurst> > m_miUlHarqProcessesPacket; ///< Packets under transmission of the UL HARQ processes
  std::vector < uint8_t > m_miUlHarqProcessesPacketTimer; ///< timer for packet life in the buffer

  uint16_t m_rnti; ///< RNTI
  uint16_t m_imsi; ///< IMSI

  bool m_rachConfigured; ///< is RACH configured?
  bool m_nprachConfigured; ///< is RACH configured?
  LteUeCmacSapProvider::RachConfig m_rachConfig; ///< RACH configuration
  NbIotRrcSap::RachInfo m_rachConfigCe; ///< RACH configuration
  NbIotRrcSap::RadioResourceConfigCommonNb m_radioResourceConfig; ///< RACH configuration
  uint8_t m_raPreambleId; ///< RA preamble ID
  uint8_t m_preambleTransmissionCounter; ///< preamble tranamission counter
  uint8_t m_preambleTransmissionCounterCe; ///< preamble tranamission counter per CE level
  uint32_t m_backoffParameter; ///< backoff parameter (ms, TS 36.321 Table 7.2-2)
  uint32_t m_pendingReRachBackoffMs {0}; ///< backoff carried into the next RA after a contention-resolution failure (TS 36.321 5.1.5); survives MAC Reset
  EventId m_noRaResponseReceivedEvent; ///< no RA response received event ID
  Ptr<UniformRandomVariable> m_raPreambleUniformVariable; ///< RA preamble random variable

  uint32_t m_frameNo; ///< frame number
  uint32_t m_subframeNo; ///< subframe number
  uint8_t m_raRnti; ///< RA RNTI
  bool m_waitingForRaResponse; ///< waiting for RA response

  NbIotRrcSap::NprachParametersNb m_CeLevel; // CE Level based on RSRP
  NbIotRrcSap::NprachParametersNb m_CeLevelRapRetries; // CE Level based on RSRP, but might be increased due to RA failures
  std::vector<std::pair<uint64_t, std::vector<uint64_t>>> m_nextPossibleHarqOpportunity;  // Subframes to send NPUSCH F2 if meessage received 
  bool m_simplifiedNprach;
  bool m_inSearchSpace;
  bool m_transmissionScheduled;
  uint64_t m_lastUlGrantNpuschSf {0}; ///< NPUSCH start subframe of the last UL grant acted on; de-dups repeated NPDCCH DCI deliveries (repetitions) so the UE transmits a grant once
  bool m_listenToSearchSpaces;
  bool m_edrx;
  bool m_psm;
  bool m_nextIsMsg5;
  bool m_edt;
  NbIotRrcSap::EdtTbsNb DoGetEdtTbsInfo(); // return EdtTbsInfo based on RSRP (Coverage level)
  uint32_t m_subframesInSearchSpace;
  std::vector<uint32_t> m_logging;
  bool m_mac_logging;
  std::string m_logdir;
  uint32_t m_msg5Buffer;

  IdealBsrCallback m_idealBsrCb;
  RaiCallback m_raiCb;                    ///< AS RAI signal to the eNB (release-now)
  bool m_raiActivation {true};           ///< rai-Activation-r14: AS RAI enabled for this UE
  bool m_persistentGrant {false};

  // Dedicated-NPRACH SR state (replaces the ideal-BSR oracle). The UE
  // requests a DCI N0 grant by transmitting an SR at its next dedicated SR
  // occasion; m_idealBsrCb is now fired only AFTER that real, period-delayed,
  // energy-charged SR — no longer a zero-delay oracle.
  uint32_t m_srPeriodSubframes {10240};  ///< SR opportunity period (subframes=ms) = effSrPeriod (round-robin)
  // Hybrid reservation+contention SR (reservation-ALOHA). The dedicated
  // round-robin slot (m_srPeriodSubframes) is the GUARANTEED floor; on top of it
  // the UE opportunistically contends on m_srContentionSubcarriers shared
  // subcarriers at every base NPRACH occasion -- a REAL contention: the UE picks
  // a random subcarrier and transmits, the eNB tallies per subcarrier and a
  // singleton WINS (gets the grant) while a collision LOSES (the eNB sees >1 on
  // that subcarrier and drops it). It clears at the FIRST of {contention win,
  // dedicated turn}; a loss needs no backoff -- the dedicated slot is the net.
  bool m_srPreamble {false};             ///< use the real-preamble dedicated SR (else legacy idealBsr SR)
  uint32_t m_srIndex {0};                ///< this UE's dedicated SR index i (resource assignment)
  uint32_t m_srReservedSubcarriers {0};  ///< N_res reserved for dedicated SR
  uint32_t m_srContentionOffset {0};     ///< first reserved subcarrier index (= N_cont)
  bool m_srConfigRegistered {false};     ///< RRC dedicated-SR config sent to eNB once
  SrConfigCallback m_srConfigCb;         ///< register (rnti, srIndex) at the eNB on connect
  uint64_t MsToNextDedicatedOccasion (void) const; ///< time to this UE's next round-robin SR occasion
  void SendDedicatedSrPreamble (void);   ///< transmit the dedicated SR preamble on the reserved subcarrier
  // Hybrid reservation+contention SR: while waiting for its guaranteed reserved
  // occasion, an unscheduled UE also contends on the shared (non-reserved)
  // subcarriers every base NPRACH occasion. Whichever resolves first wins.
  bool m_srHybridContention {false};     ///< opportunistic contention SR enabled
  bool m_oracleBsr {false};              ///< oracle / ideal BSR: instant zero-cost buffer report to the eNB
  uint64_t MsToNextBaseOccasion (void) const;  ///< time to the next NPRACH occasion (any phase)
  void SendContentionSrPreamble (void);  ///< contend on a random shared subcarrier
  void RestoreSrCeLevelAfterEdt (void);  ///< undo the EDT-partition m_CeLevel swap (all contention exits)
  EventId m_srContentionEvent;           ///< pending contention SR transmission event
  bool m_srPending {false};              ///< an SR is already scheduled/awaiting tx
  bool m_srContentionRa {false};         ///< a faithful hybrid-contention RA is in flight (keeps C-RNTI; retry on loss)
  uint32_t m_srContentionFailures {0};   ///< contention attempts (RAR-timeout/Msg4-loss) for the CURRENT packet; inflates the RA-cost estimate so a UE stops dogpiling the shared pool and falls back to its guaranteed dedicated slot
  Time m_lastDedSrTx {Seconds (-1.0)};   ///< last dedicated-SR preamble TX: its blind grant is inbound; contention holds off (grant-grace)
  uint16_t m_contentionTempRnti {0};     ///< RAR Temp C-RNTI for a contention Msg3: tag the Msg3 tx with it so the eNB receives it on the allocated Msg3 resource
  EventId m_srEvent;                     ///< pending SR transmission event
  Time m_srProhibitUntil {Seconds (0)};  ///< sr-ProhibitTimer: no new SR before this time
  uint32_t m_srProhibitPeriods {1};      ///< prohibit duration, in SR periods
  bool m_grantSessionActive {false};     ///< eNB is actively granting (post-SR); the real BSR
                                         ///< conveys the buffer and new SRs are suppressed
  bool m_drbDataSentThisSession {false}; ///< a DRB (data, lcid>2) PDU has been transmitted since the
                                         ///< last resume; gates RAI so it can't fire on the transient
                                         ///< empty-BSR window during resume signalling (which would
                                         ///< release the UE BEFORE its data -> a stale release that
                                         ///< strands the next epoch's packet)
  uint64_t m_prevBsrTotal {0};           ///< diagnostic: last total UL buffer seen in
                                         ///< DoReportBufferStatus, to detect the 0->n arrival edge
  uint32_t m_srBootstrapBytes {50};      ///< NOT transmitted: the SR on air is a 1-bit NPRACH
                                         ///< preamble (TS 36.321, carries no buffer size). This
                                         ///< token trips the eNB grant gate (rlcUlBuffer>0) and
                                         ///< sizes the BLIND first grant. The eNB grants a
                                         ///< service-appropriate default TBS (~one ambient-IoT
                                         ///< packet, ~49 B + headers; cf. configured-grant/SPS
                                         ///< sized for the known traffic), so a typical packet
                                         ///< fits in ONE grant; any excess uses the BSR tag on
                                         ///< that NPUSCH. (Was 1 -> 11 B TBS -> many fragile
                                         ///< grant rounds per 49 B packet -> high duty + loss.)
  bool m_deepSleepFug {false};           ///< Model 1: deep-sleep (PSM) between packets, resurrect
                                         ///< via SR, instead of the connected NPDCCH-monitoring floor
  bool m_suspended {false};              ///< RRC-suspended (PSM/eDRX): the MAC must NOT fire SRs --
                                         ///< data wakes the UE via the RRC resume path. Prevents the
                                         ///< stray-SR-after-suspend wake loop.
  // Connected-mode DRX (TS 36.321 5.7 / DRX-Config-NB-r13). Gates NPDCCH
  // search-space monitoring to the on-duration of each long DRX cycle; outside
  // it the UE rests at the connected idle floor (RRC_CONNECTED_IDLE). The cycle
  // length therefore sets how often the ~80 mW on-duration monitoring recurs.
  bool m_cdrxEnabled {false};            ///< connected-mode DRX active
  uint32_t m_cdrxCycleSubframes {10240}; ///< long DRX cycle (sf); 10240 = sf10240 = 10.24 s
  uint32_t m_cdrxOnDurationSubframes {64}; ///< on-duration window (>= one search-space period)
  uint32_t m_cdrxInactivityMs {500};     ///< drx-InactivityTimer length (extends Active Time)
  Time m_cdrxInactivityUntil {Seconds (0)}; ///< Active Time deadline after the last grant
  // Proactive FUG (4th mode): SR cleanly disabled in steady state.
  bool m_proactiveFug {false};           ///< this UE is served by proactive FUG (no SR)
  // C-RNTI MAC CE (connected-UE RA keeping its identity).
  uint16_t m_raKeepCrnti {0};            ///< if !=0, this RA keeps this C-RNTI (signalled in Msg3)
  bool m_msg3HasCrntiMacCe {false};      ///< attach the C-RNTI MAC CE to the next Msg3
  bool m_edtMsg3DataSent {false};        ///< EDT contention Msg3 carried the DATA (loser re-arms RLC txed->retx)
  // EDT (Rel-15) on the hybrid-contention SR route: the contention preamble is sent
  // on the SEPARATE EDT NPRACH partition (that IS the EDT request); m_CeLevel is
  // swapped to the partition params for the transaction and restored at every exit.
  bool m_srEdtContention {false};        ///< contention-SR attempts use the EDT partition
  bool m_srCeLevelSwapped {false};       ///< m_CeLevel currently holds the EDT partition params
  NbIotRrcSap::NprachParametersNb m_srCeLevelSaved {}; ///< legacy CE params to restore
  bool m_awaitingContentionResolution {false}; ///< Msg3 sent with C-RNTI MAC CE, waiting for Msg4
  EventId m_contentionResolutionTimer;   ///< mac-ContentionResolutionTimer (fails CR if no Msg4)
  uint16_t m_crniTempForCr {0};          ///< the Temporary C-RNTI to discard on successful CR
  void ContentionResolutionTimeout (void); ///< CR failed: no Msg4 within the timer
  uint32_t m_fugBootstrapEpochs {0};     ///< reactive-SR epochs done; >=k -> SR off, await predicted grant
  static constexpr uint32_t kProactiveBootstrapEpochs = 2; ///< reactive SRs to seed the eNB predictor
  void SendSchedulingRequest (void);     ///< tx a dedicated-NPRACH SR, then signal eNB
  /**
   * \brief The `RaResponseTimeout` trace source. Fired RA response timeout.
   * Exporting IMSI, contention flag, preamble transmission counter
   * and the max limit of preamble transmission.
   */
  TracedCallback<uint64_t, bool, uint8_t, uint8_t> m_raResponseTimeoutTrace;
};

} // namespace ns3

#endif // LTE_UE_MAC_ENTITY

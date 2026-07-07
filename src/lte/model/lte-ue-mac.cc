/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2022 Communication Networks Institute at TU Dortmund University
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
 */

#include <ns3/log.h>
#include <ns3/pointer.h>
#include <ns3/packet.h>
#include <ns3/packet-burst.h>
#include <ns3/random-variable-stream.h>
#include <ns3/build-profile.h>

#include "lte-ue-mac.h"
#include "lte-ue-net-device.h"
#include "lte-radio-bearer-tag.h"
#include "nb-iot-data-volume-and-power-headroom-tag.h"
#include "nb-iot-buffer-status-report-tag.h"
#include "nb-iot-crnti-mac-ce-tag.h"
#include <ns3/ff-mac-common.h>
#include <ns3/lte-control-messages.h>
#include <ns3/simulator.h>
#include <ns3/lte-common.h>
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LteUeMac");

NS_OBJECT_ENSURE_REGISTERED (LteUeMac);

// TS 36.321 Table 7.2-2: NB-IoT Backoff Parameter values (ms) by BI index.
static uint32_t NbiotBackoffMs (uint8_t bi)
{
  static const uint32_t kTable[16] = {
    0, 256, 512, 1024, 2048, 4096, 8192, 16384,
    32768, 65536, 131072, 262144, 524288, 524288, 524288, 524288 };
  return kTable[bi & 0x0F];
}

///////////////////////////////////////////////////////////
// SAP forwarders
///////////////////////////////////////////////////////////

/// UeMemberLteUeCmacSapProvider class
class UeMemberLteUeCmacSapProvider : public LteUeCmacSapProvider
{
public:
  /**
   * Constructor
   *
   * \param mac the UE MAC
   */
  UeMemberLteUeCmacSapProvider (LteUeMac *mac);

  // inherited from LteUeCmacSapProvider
  virtual void ConfigureRach (RachConfig rc);
  virtual void ConfigureRadioResourceConfig (NbIotRrcSap::RadioResourceConfigCommonNb rc);
  virtual void StartContentionBasedRandomAccessProcedure ();
  virtual void StartRandomAccessProcedureNb (bool edt);
  virtual void StartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t preambleId,
                                                             uint8_t prachMask);
  virtual void SetRnti (uint16_t rnti);
  virtual void AddLc (uint8_t lcId, LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
                      LteMacSapUser *msu);
  virtual void RemoveLc (uint8_t lcId);
  virtual void Reset ();
  virtual void NotifyConnectionSuccessful ();
  virtual void SetImsi (uint64_t imsi);
  virtual void NotifyEdrx();
  virtual void NotifyPsm();
  virtual void SetMsg5Buffer(uint32_t buffersize);
  virtual void SetRaKeepCrnti(uint16_t crnti);
  virtual void NotifyContentionResolutionFailedNb();
  virtual NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel GetCoverageEnhancementLevel();
  virtual uint32_t GetUlBufferSize();

private:
  LteUeMac *m_mac; ///< the UE MAC
};

UeMemberLteUeCmacSapProvider::UeMemberLteUeCmacSapProvider (LteUeMac *mac) : m_mac (mac)
{
}

void
UeMemberLteUeCmacSapProvider::ConfigureRach (RachConfig rc)
{
  m_mac->DoConfigureRach (rc);
}
void
UeMemberLteUeCmacSapProvider::ConfigureRadioResourceConfig (
    NbIotRrcSap::RadioResourceConfigCommonNb rc)
{
  m_mac->DoConfigureRadioResourceConfig (rc);
}
void
UeMemberLteUeCmacSapProvider::StartContentionBasedRandomAccessProcedure ()
{
  m_mac->DoStartContentionBasedRandomAccessProcedure ();
}
void
UeMemberLteUeCmacSapProvider::StartRandomAccessProcedureNb (bool edt)
{
  m_mac->DoStartRandomAccessProcedureNb (edt);
}
void
UeMemberLteUeCmacSapProvider::StartNonContentionBasedRandomAccessProcedure (uint16_t rnti,
                                                                            uint8_t preambleId,
                                                                            uint8_t prachMask)
{
  m_mac->DoStartNonContentionBasedRandomAccessProcedure (rnti, preambleId, prachMask);
}

void
UeMemberLteUeCmacSapProvider::SetRnti (uint16_t rnti)
{
  m_mac->DoSetRnti (rnti);
}

void
UeMemberLteUeCmacSapProvider::AddLc (uint8_t lcId, LogicalChannelConfig lcConfig,
                                     LteMacSapUser *msu)
{
  m_mac->DoAddLc (lcId, lcConfig, msu);
}

void
UeMemberLteUeCmacSapProvider::RemoveLc (uint8_t lcid)
{
  m_mac->DoRemoveLc (lcid);
}

void
UeMemberLteUeCmacSapProvider::Reset ()
{
  m_mac->DoReset ();
}

void
UeMemberLteUeCmacSapProvider::NotifyConnectionSuccessful ()
{
  m_mac->DoNotifyConnectionSuccessful ();
}

void
UeMemberLteUeCmacSapProvider::SetImsi (uint64_t imsi)
{
  m_mac->DoSetImsi (imsi);
}

void
UeMemberLteUeCmacSapProvider::NotifyEdrx()
{
  m_mac->DoNotifyEdrx();
}
void
UeMemberLteUeCmacSapProvider::NotifyPsm()
{
  m_mac->DoNotifyPsm();
}


void
UeMemberLteUeCmacSapProvider::SetMsg5Buffer(uint32_t buffersize){
  m_mac->DoSetMsg5Buffer(buffersize);
}
void
UeMemberLteUeCmacSapProvider::SetRaKeepCrnti(uint16_t crnti){
  m_mac->SetRaKeepCrnti(crnti);
}
void
UeMemberLteUeCmacSapProvider::NotifyContentionResolutionFailedNb(){
  m_mac->NotifyContentionResolutionFailedNb();
}

NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel UeMemberLteUeCmacSapProvider::GetCoverageEnhancementLevel(){
  return m_mac->DoGetCoverageEnhancementLevel();
}

uint32_t UeMemberLteUeCmacSapProvider::GetUlBufferSize(){
  // DATA-only buffer (LCID>2). NB-IoT RLC-UM leaves a small status-PDU residue on
  // the signalling LCs that never drains; using the full buffer here would keep a
  // proactive UE awake (RRC suspend guard) forever, never letting it sleep.
  return (uint32_t) m_mac->GetBufferSize();
}

/// UeMemberLteMacSapProvider class
class UeMemberLteMacSapProvider : public LteMacSapProvider
{
public:
  /**
   * Constructor
   *
   * \param mac the UE MAC
   */
  UeMemberLteMacSapProvider (LteUeMac *mac);

  // inherited from LteMacSapProvider
  virtual void TransmitPdu (TransmitPduParameters params);
  virtual void ReportBufferStatus (ReportBufferStatusParameters params);
  virtual void ReportBufferStatusNb (ReportBufferStatusParameters params,
                                     NbIotRrcSap::NpdcchMessage::SearchSpaceType searchspace);
  virtual void ReportNoTransmissionNb(uint16_t rnti, uint8_t lcid);

private:
  LteUeMac *m_mac; ///< the UE MAC
};

UeMemberLteMacSapProvider::UeMemberLteMacSapProvider (LteUeMac *mac) : m_mac (mac)
{
}

void
UeMemberLteMacSapProvider::TransmitPdu (TransmitPduParameters params)
{
  m_mac->DoTransmitPdu (params);
}

void
UeMemberLteMacSapProvider::ReportBufferStatus (ReportBufferStatusParameters params)
{
  m_mac->DoReportBufferStatus (params);
}

void
UeMemberLteMacSapProvider::ReportBufferStatusNb (
    ReportBufferStatusParameters params, NbIotRrcSap::NpdcchMessage::SearchSpaceType searchspace)
{
  m_mac->DoReportBufferStatus (params);
}

void
UeMemberLteMacSapProvider::ReportNoTransmissionNb(uint16_t rnti, uint8_t lcid){

}

/**
 * UeMemberLteUePhySapUser
 */
class UeMemberLteUePhySapUser : public LteUePhySapUser
{
public:
  /**
   * Constructor
   *
   * \param mac the UE MAC
   */
  UeMemberLteUePhySapUser (LteUeMac *mac);

  // inherited from LtePhySapUser
  virtual void ReceivePhyPdu (Ptr<Packet> p);
  virtual void SubframeIndication (uint32_t frameNo, uint32_t subframeNo);
  virtual void ReceiveLteControlMessage (Ptr<LteControlMessage> msg);
  virtual void NotifyAboutHarqOpportunity (std::vector<std::pair<uint64_t, std::vector<uint64_t>>> subframes);

private:
  LteUeMac *m_mac; ///< the UE MAC
};

UeMemberLteUePhySapUser::UeMemberLteUePhySapUser (LteUeMac *mac) : m_mac (mac)
{
}

void
UeMemberLteUePhySapUser::ReceivePhyPdu (Ptr<Packet> p)
{
  m_mac->DoReceivePhyPdu (p);
}

void
UeMemberLteUePhySapUser::SubframeIndication (uint32_t frameNo, uint32_t subframeNo)
{
  m_mac->DoSubframeIndication (frameNo, subframeNo);
}

void
UeMemberLteUePhySapUser::ReceiveLteControlMessage (Ptr<LteControlMessage> msg)
{
  m_mac->DoReceiveLteControlMessage (msg);
}

void
UeMemberLteUePhySapUser::NotifyAboutHarqOpportunity (
    std::vector<std::pair<uint64_t, std::vector<uint64_t>>> subframes)
{
  m_mac->DoNotifyAboutHarqOpportunity (subframes);
}

//////////////////////////////////////////////////////////
// LteUeMac methods
///////////////////////////////////////////////////////////

TypeId
LteUeMac::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::LteUeMac")
          .SetParent<Object> ()
          .SetGroupName ("Lte")
          .AddConstructor<LteUeMac> ()
          .AddTraceSource ("RaResponseTimeout", "trace fired upon RA response timeout",
                           MakeTraceSourceAccessor (&LteUeMac::m_raResponseTimeoutTrace),
                           "ns3::LteUeMac::RaResponseTimeoutTracedCallback")

      ;
  return tid;
}

LteUeMac::LteUeMac ()
    : m_bsrPeriodicity (MilliSeconds (1)), // ideal behavior
      m_bsrLast (MilliSeconds (0)),
      m_freshUlBsr (false),
      m_harqProcessId (0),
      m_rnti (0),
      m_imsi (0),
      m_rachConfigured (false),
      m_waitingForRaResponse (false),
      m_transmissionScheduled(false),
      m_listenToSearchSpaces(false)

{
  NS_LOG_FUNCTION (this);
  m_miUlHarqProcessesPacket.resize (HARQ_PERIOD);
  for (uint8_t i = 0; i < m_miUlHarqProcessesPacket.size (); i++)
    {
      Ptr<PacketBurst> pb = CreateObject<PacketBurst> ();
      m_miUlHarqProcessesPacket.at (i) = pb;
    }
  m_miUlHarqProcessesPacketTimer.resize (HARQ_PERIOD, 0);

  m_macSapProvider = new UeMemberLteMacSapProvider (this);
  m_cmacSapProvider = new UeMemberLteUeCmacSapProvider (this);
  m_uePhySapUser = new UeMemberLteUePhySapUser (this);
  m_raPreambleUniformVariable = CreateObject<UniformRandomVariable> ();
  m_componentCarrierId = 0;
  m_nextIsMsg5 = false;
  m_mac_logging = false;
}

LteUeMac::~LteUeMac ()
{
  NS_LOG_FUNCTION (this);
}

void
LteUeMac::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_miUlHarqProcessesPacket.clear ();
  m_srEvent.Cancel ();
  m_srContentionEvent.Cancel ();
  delete m_macSapProvider;
  delete m_cmacSapProvider;
  delete m_uePhySapUser;
  Object::DoDispose ();
}

LteUePhySapUser *
LteUeMac::GetLteUePhySapUser (void)
{
  return m_uePhySapUser;
}

void
LteUeMac::SetIdealBsrCallback (IdealBsrCallback cb) { m_idealBsrCb = cb; }

void
LteUeMac::SetRaiCallback (RaiCallback cb) { m_raiCb = cb; }

void
LteUeMac::SetSrConfigCallback (SrConfigCallback cb) { m_srConfigCb = cb; }

void
LteUeMac::SetSrDedicated (uint32_t srIndex, uint32_t reservedSubcarriers, uint32_t contentionOffset)
{
  m_srPreamble = (reservedSubcarriers > 0);
  m_srIndex = srIndex;
  m_srReservedSubcarriers = reservedSubcarriers;
  m_srContentionOffset = contentionOffset;
}

void
LteUeMac::SetSrHybridContention (bool en) { m_srHybridContention = en; }

void
LteUeMac::SetOracleBsr (bool en) { m_oracleBsr = en; }

void
LteUeMac::SetPersistentGrant (bool enable) { m_persistentGrant = enable; }

void
LteUeMac::SetSrPeriod (uint32_t subframes) { m_srPeriodSubframes = (subframes == 0 ? 1 : subframes); }

void
LteUeMac::SetDeepSleepFug (bool enable) { m_deepSleepFug = enable; }

void
LteUeMac::SetCdrx (bool enable, uint32_t cycleSubframes, uint32_t inactivityMs)
{
  m_cdrxEnabled = enable;
  if (cycleSubframes > 0) m_cdrxCycleSubframes = cycleSubframes;
  m_cdrxInactivityMs = inactivityMs;
  // cDRX keeps the UE RRC-CONNECTED and radio-sleeps it; it must NOT release via
  // AS RAI. Leaving RAI on makes every drained packet fire ReleaseOnRai ->
  // SwitchToResumeNb (eNB park + NAS suspend + re-wake), churning NPDCCH and
  // defeating the connected-DRX floor. Disable RAI for the cDRX UE.
  if (enable) m_raiActivation = false;
}

void
LteUeMac::SetProactiveFug (bool enable)
{
  m_proactiveFug = enable;
}

void
LteUeMac::SetRaKeepCrnti (uint16_t crnti) { m_raKeepCrnti = crnti; }

void
LteUeMac::ContentionResolutionTimeout (void)
{
  // mac-ContentionResolutionTimer expired with no Msg4 (no C-RNTI-addressed
  // PDCCH): contention resolution failed (TS 36.321 5.1.5).
  if (m_awaitingContentionResolution)
    {
      NS_LOG_WARN ("Contention resolution FAILED (no Msg4) for C-RNTI=" << m_rnti);
      m_awaitingContentionResolution = false;
      m_crniTempForCr = 0;
      RestoreSrCeLevelAfterEdt ();
      // EDT Msg3 carried the DATA and we lost the captured collision: the RLC
      // already emitted the PDU (txon -> txed, unACKed). Re-arm it (txed -> retx)
      // so the retry Msg3 / reserved slot can carry it again -- otherwise it
      // strands until t-PollRetransmit (25 s).
      if (m_edtMsg3DataSent)
        {
          m_edtMsg3DataSent = false;
          for (auto &kv : m_lcInfoMap)
            {
              if (kv.first > 2 && kv.second.macSapUser != nullptr)
                {
                  kv.second.macSapUser->NotifyHarqDeliveryFailure ();
                }
            }
        }
      // Faithful hybrid-contention SR: lost the captured collision (no Msg4 for our
      // C-RNTI). The UE kept its real C-RNTI, so just keep contending at the next
      // base occasion; the dedicated reserved slot is the guaranteed floor.
      if (m_srContentionRa && m_srPending)
        {
          m_srContentionRa = false;
          ++m_srContentionFailures;   // this packet's RA route just got costlier -> gate may peel it to dedicated
          // TS 36.321 5.1.4: back off (BACKOFF from the contention RAR's BI) before
          // re-contending so colliders that lost spread across occasions instead of
          // re-colliding in lockstep at the same next base occasion.
          uint32_t backoffMs = (m_backoffParameter > 0)
              ? m_raPreambleUniformVariable->GetInteger (0, m_backoffParameter) : 0;
          m_srContentionEvent = Simulator::Schedule (
              MilliSeconds (MsToNextBaseOccasion () + backoffMs),
              &LteUeMac::SendContentionSrPreamble, this);
        }
      // In a full model the UE would back off and retry RA; left as a hook.
    }
}

// Time (ms) to this UE's NEXT dedicated SR occasion: the next real NPRACH
// occasion k (k*period + startTime) with k % cycle == phase, where phase =
// srIndex / N_res and cycle = effSrPeriod/period. Computed identically on the
// eNB side so the resolved phase matches.
uint64_t
LteUeMac::MsToNextDedicatedOccasion (void) const
{
  uint32_t now    = Simulator::Now ().GetMilliSeconds ();
  uint32_t period = NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel);
  uint32_t start  = NbIotRrcSap::ConvertNprachStartTime2int (m_CeLevel);
  if (period == 0) period = 1;
  uint32_t cycle  = (m_srPeriodSubframes > period) ? (m_srPeriodSubframes / period) : 1;
  uint32_t phase  = (m_srReservedSubcarriers > 0) ? (m_srIndex / m_srReservedSubcarriers) % cycle : 0;
  // index of the first occasion at/after now
  uint32_t kMin = (now <= start) ? 0 : ((now - start + period - 1) / period);
  uint32_t r = kMin % cycle;
  uint32_t add = (phase + cycle - r) % cycle;
  uint64_t k = (uint64_t) kMin + add;
  uint64_t occTime = k * period + start;
  return (occTime > now) ? (occTime - now) : 0;
}

void
LteUeMac::SendDedicatedSrPreamble (void)
{
  NS_LOG_FUNCTION (this);
  if (!m_srPending) { return; }                       // already granted
  if (m_rnti == 0 || m_suspended) { m_srPending = false; return; }
  uint64_t total = 0;
  for (auto & kv : m_ulBsrReceived)
    total += kv.second.txQueueSize + kv.second.retxQueueSize + kv.second.statusPduSize;
  if (total == 0) { m_srPending = false; return; }    // buffer drained

  // Dedicated SR takes PRECEDENCE over contention (the reserved, collision-free slot
  // is the guaranteed path). If a contention attempt for this same packet is in
  // flight, ABANDON it and serve the UE only via the dedicated grant: the UE drops out
  // of the contention (ignores any RAR/Msg4 for it -> the eNB picks a different winner
  // for that occasion). Without this the UE would be granted by BOTH paths and
  // transmit the same PDU twice -> the eNB RLC-AM reassembles the duplicate into a
  // malformed SDU (PDCP D/C-bit assert) and drops data.
  if (m_srContentionRa || m_waitingForRaResponse || m_awaitingContentionResolution)
    {
      m_srContentionRa = false;
      m_waitingForRaResponse = false;
      m_noRaResponseReceivedEvent.Cancel ();
      m_awaitingContentionResolution = false;
      m_contentionResolutionTimer.Cancel ();
      m_srContentionEvent.Cancel ();
      m_msg3HasCrntiMacCe = false;
      m_contentionTempRnti = 0;
      m_raKeepCrnti = 0;
      m_msg5Buffer = 0;
      // Restore the legacy partition FIRST: the reserved-subcarrier computation
      // below reads m_CeLevel, which the abandoned EDT contention had swapped.
      RestoreSrCeLevelAfterEdt ();
      // Abandoning an EDT contention whose Msg3 already carried the DATA: re-arm
      // the RLC (txed -> retx) so the reserved-slot grant below can carry it.
      if (m_edtMsg3DataSent)
        {
          m_edtMsg3DataSent = false;
          for (auto &kv : m_lcInfoMap)
            {
              if (kv.first > 2 && kv.second.macSapUser != nullptr)
                {
                  kv.second.macSapUser->NotifyHarqDeliveryFailure ();
                }
            }
        }
    }

  // Transmit a real NPRACH preamble on this UE's reserved subcarrier. The
  // preamble carries NO identity; the eNB resolves the C-RNTI from the
  // resource->UE map it holds (registered at connect). Reserved subcarrier =
  // contentionOffset + (srIndex % N_res). Preamble duration mirrors the RA path.
  uint32_t reservedSub = m_srContentionOffset + (m_srIndex % m_srReservedSubcarriers);
  m_lastDedSrTx = Simulator::Now ();  // blind grant inbound: contention holds off (grant-grace)
  if (NbIotDebugTrace ())
    std::cout << "[UE-DEDSR-TX] t=" << Simulator::Now ().GetSeconds ()
              << " rnti=" << m_rnti << " srIdx=" << m_srIndex
              << " reservedSub=" << reservedSub << " total=" << total << std::endl;
  uint8_t  ceOffset = NbIotRrcSap::ConvertNprachSubcarrierOffset2int (m_CeLevel);
  double ts = 1000.0 / (15000.0 * 2048.0);
  double preambleGroupTime = NbIotRrcSap::ConvertNprachCpLenght2double (
        m_radioResourceConfig.nprachConfig) + 5.0 * 8192.0 * ts;
  double time = NbIotRrcSap::ConvertNumRepetitionsPerPreambleAttempt2int (m_CeLevel)
                * 4.0 * preambleGroupTime;
  m_cmacSapUser->NotifyEnergyState (NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPRACH);
  Simulator::Schedule (MilliSeconds (time + 1), &LteUeCmacSapUser::NotifyEnergyState,
                       m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);
  Simulator::Schedule (MilliSeconds (time), &LteUePhySapProvider::SendNprachPreamble,
                       m_uePhySapProvider, (uint32_t) reservedSub, (uint32_t) m_rnti, ceOffset);
  // Retry at this UE's OWN next dedicated occasion if no grant arrives (the DCI N0 handler
  // clears m_srPending and cancels m_srEvent on success). Right after firing at our slot,
  // MsToNextDedicatedOccasion() returns 0 -- the fallback must then be a FULL round-robin
  // cycle (m_srPeriodSubframes ~1600 ms, our next real slot), NOT one NPRACH period (80 ms).
  // The 80 ms fallback re-fired on the very next occasion (a DIFFERENT phase/UE) every 80 ms,
  // so the "collision-free" dedicated SR collided across the whole round-robin -- the root of
  // the dedicated-SR collisions and the delay. The hybrid contention leg (scheduled in the SR
  // branch, self-rescheduling) runs in PARALLEL and provides the fast "keep trying" path,
  // contending whenever the next dedicated slot is farther than a full RA.
  uint64_t toNext = MsToNextDedicatedOccasion ();
  if (toNext == 0) toNext = m_srPeriodSubframes;   // our next real slot, not one 80 ms occasion
  m_srEvent = Simulator::Schedule (MilliSeconds (toNext), &LteUeMac::SendDedicatedSrPreamble, this);
}

uint64_t
LteUeMac::MsToNextBaseOccasion (void) const
{
  // Time to the next NPRACH occasion of ANY phase (the contention pool opens
  // every base period, unlike the per-UE reserved occasion every effSrPeriod).
  uint32_t now    = Simulator::Now ().GetMilliSeconds ();
  uint32_t period = NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel);
  uint32_t start  = NbIotRrcSap::ConvertNprachStartTime2int (m_CeLevel);
  if (period == 0) period = 1;
  uint32_t kMin = (now <= start) ? 0 : ((now - start + period - 1) / period);
  uint64_t occTime = (uint64_t) kMin * period + start;
  if (occTime <= now) occTime += period;     // strictly the NEXT occasion
  return occTime - now;
}

void
LteUeMac::SendContentionSrPreamble (void)
{
  NS_LOG_FUNCTION (this);
  if (!m_srPending) { return; }                        // already granted
  if (m_rnti == 0 || m_suspended) { m_srPending = false; return; }
  if (m_srContentionOffset == 0) { return; }           // no shared pool configured
  if (GetBufferSizeComplete () == 0) { m_srPending = false; return; } // buffer drained
  // A faithful contention RA is already in flight (awaiting RAR or Msg4): do not
  // launch a second preamble on top of it.
  if (m_srContentionRa || m_waitingForRaResponse || m_awaitingContentionResolution)
    { return; }

  // A registered/resumed UE (it holds a C-RNTI on this path) only contends on the shared
  // pool when its guaranteed dedicated SR is FARTHER off than a full RACH (preamble ->
  // Msg4). Within that window the reserved, collision-free slot serves it sooner, so
  // contending would only add collision risk and energy for no latency gain -- skip this
  // round but keep the cadence for the gaps between reserved occasions. Estimate
  // preamble->Msg4 from the same CE-level RA constants as the RA window (TS 36.321 5.1.4)
  // plus a representative Msg3+Msg4 (~64 ms typical at CE0; CR-timer worst case is 512 ms).
  // Faithful full-RA latency (TS 36.321 5.1): sum ALL FIVE messages at this CE level,
  // not just preamble->RAR + a flat 64. Each NPDCCH grant search costs ~one NPDCCH
  // period; each NPUSCH transmission ~one more (Msg3/Msg5 are single-block at CE0).
  //   Msg1 preamble tx | Msg2 RAR window | Msg3 (UL grant + tx) | Msg4 (contention
  //   resolution PDCCH, well under the pp32 CR-timer CAP) | Msg5 (data grant + tx).
  double npdcchPeriodRa = NbIotRrcSap::ConvertNpdcchNumRepetitionsRa2int (m_CeLevel) *
                          NbIotRrcSap::ConvertNpdcchStartSfCssRa2double (m_CeLevel);
  uint32_t msg1PreambleMs = (NbIotRrcSap::ConvertNumRepetitionsPerPreambleAttempt2int (m_CeLevel) >= 64 ? 41u : 4u);
  uint32_t msg2RarMs      = (uint32_t) (NbIotRrcSap::ConvertRaResponseWindowSize2int (m_rachConfigCe) * npdcchPeriodRa);
  uint32_t msg3Ms         = (uint32_t) (2.0 * npdcchPeriodRa);   // UL grant search + Msg3 NPUSCH tx
  uint32_t msg4Ms         = (uint32_t) (3.0 * npdcchPeriodRa);   // contention-resolution PDCCH
  uint32_t msg5Ms         = (uint32_t) (2.0 * npdcchPeriodRa);   // data grant search + Msg5 NPUSCH tx
  uint32_t fullRachLatencyMs = msg1PreambleMs + msg2RarMs + msg3Ms + msg4Ms + msg5Ms;
  // Load-aware: RA is only the fast path when it SUCCEEDS. Each failed contention for
  // this packet (RAR timeout or Msg4/CR loss) multiplies the real cost of the RA route.
  // Once the honest estimate exceeds the wait for this UE's GUARANTEED collision-free
  // dedicated slot, stop dogpiling the shared pool and let the (already-scheduled,
  // self-retrying) dedicated preamble serve it -- TS 36.321 5.1.5 reserved SR is the
  // floor. This is what keeps the 6-tone pool from cascading at high density: colliding
  // UEs peel off to their reserved slots instead of re-colliding indefinitely.
  uint64_t estimatedRaMs = (uint64_t) fullRachLatencyMs * (m_srContentionFailures + 1);
  uint64_t toNextDedMs = MsToNextDedicatedOccasion ();
  bool timeSaysContend = toNextDedMs > estimatedRaMs;
  // Grant-grace: the reserved preamble just fired and its blind grant is inbound
  // (typ. 100-300 ms under load). Re-evaluating 80 ms later, the wait has wrapped
  // to the NEXT cycle and looks huge -- contending then races the grant, and the
  // stray preamble resurrects the just-served session (ghost resume: UE dangles
  // awake past its PSM sleep and browns out at the next epoch wave).
  bool grantPending = (Simulator::Now () - m_lastDedSrTx) < MilliSeconds (500);
  // Energy-aware admission: a capacitor UE only gambles on the shared pool if its
  // remaining charge can fund a full LOSING round above the brown-out floor
  // (preamble + RAR listen + EDT-Msg3 data TX + CR listen, with margin). An
  // energy-poor UE keeps its guaranteed single-transmission reserved slot.
  bool energyOk = !timeSaysContend || m_cmacSapUser->HasEnergyForContention ();
  if (NbIotDebugTrace ())
    std::cout << "[SR-GATE] t=" << Simulator::Now ().GetSeconds () << " imsi=" << m_imsi
              << " waitMs=" << toNextDedMs << " estMs=" << estimatedRaMs
              << " fails=" << m_srContentionFailures
              << " decision=" << (!timeSaysContend ? "WAIT-SLOT"
                                  : grantPending    ? "WAIT-GRANT"
                                  : energyOk        ? "CONTEND"
                                                    : "WAIT-ENERGY")
              << std::endl;
  if (!timeSaysContend || grantPending || !energyOk)
    {
      m_srContentionEvent = Simulator::Schedule (
          MilliSeconds (MsToNextBaseOccasion ()),
          &LteUeMac::SendContentionSrPreamble, this);
      return;
    }

  // Faithful hybrid contention (TS 36.321 5.1.5): transmit a real, IDENTITY-LESS
  // NPRACH preamble on a random shared subcarrier and run the RA handshake while
  // KEEPING the C-RNTI. The RAR's Temporary C-RNTI is used only to send Msg3,
  // which carries the retained C-RNTI in a C-RNTI MAC CE plus a data-volume DPR;
  // the eNB resolves contention by C-RNTI at Msg4. A colliding occasion is
  // CAPTURED (one UE wins, DropPreambleCollision=false); the losers retry the
  // next occasion (ContentionResolutionTimeout). The dedicated reserved slot is
  // the guaranteed floor (skip above). m_raPreambleId is a contention-pool
  // subcarrier in [0, contentionOffset); SendRaPreambleNb sends it carrying the
  // RA-RNTI (not the C-RNTI) and arms the RAR-wait, so the eNB issues a real RAR.
  m_raKeepCrnti    = m_rnti;                    // -> Msg3 C-RNTI MAC CE
  m_msg5Buffer     = GetBufferSizeComplete ();  // -> Msg3 DPR = real UL volume
  m_srContentionRa = true;                      // RAR-timeout / CR-loss => retry
  m_preambleTransmissionCounter = 0;
  m_preambleTransmissionCounterCe = 0;
  // EDT (Rel-15) on the contention route: transmit the preamble on the SEPARATE EDT
  // NPRACH partition -- the partition itself IS the EDT request (TS 36.321), so the
  // eNB's EDT scan returns an isEdt RAR with an edt-TBS Msg3 grant and the DATA rides
  // Msg3 (with the C-RNTI MAC CE + DPR) instead of a control-only dummy. m_CeLevel is
  // swapped to the partition params for this transaction and restored at EVERY exit
  // (win / CR-loss / RAR-timeout / reserved-slot abandonment).
  if (m_srEdtContention && m_msg5Buffer > 0 && (m_msg5Buffer + 10) * 8 <= 1000)
    {
      NbIotRrcSap::NprachParametersNbR14 edtP =
          m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb0;
      if (!m_srCeLevelSwapped)
        {
          m_srCeLevelSaved = m_CeLevel;
          m_srCeLevelSwapped = true;
        }
      m_edt = true;
      m_CeLevel.coverageEnhancementLevel = edtP.coverageEnhancementLevel;
      m_CeLevel.nprachPeriodicity = edtP.nprachPeriodicity;
      m_CeLevel.nprachStartTime = edtP.nprachStartTime;
      m_CeLevel.nprachSubcarrierOffset = edtP.nprachSubcarrierOffset;
      m_CeLevel.nprachNumSubcarriers = edtP.nprachNumSubcarriers;
      m_CeLevel.nprachSubcarrierMsg3RangeStart = edtP.nprachSubcarrierMsg3RangeStart;
      m_CeLevel.npdcchNumRepetitionsRA = edtP.npdcchNumRepetitionsRA;
      m_CeLevel.npdcchStartSfCssRa = edtP.npdcchStartSfCssRa;
      m_CeLevel.npdcchOffsetRa = edtP.npdcchOffsetRa;
      m_raPreambleId = m_raPreambleUniformVariable->GetInteger (
          0, NbIotRrcSap::ConvertNprachNumSubcarriers2int (m_CeLevel) - 1);
      // Align to the EDT partition's OWN NPRACH occasions (period/start differ from
      // the legacy partition whose cadence scheduled this call).
      uint64_t nowMs = 10ull * (m_frameNo - 1) + (m_subframeNo - 1);
      uint64_t periodMs = NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel);
      uint64_t startMs = NbIotRrcSap::ConvertNprachStartTime2int (m_CeLevel);
      uint64_t phaseMs = (periodMs - ((nowMs + periodMs - startMs) % periodMs)) % periodMs;
      if (NbIotDebugTrace ())
        std::cout << "[SR-EDT-TX] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
                  << " preamble=" << (uint32_t) m_raPreambleId << " occInMs=" << phaseMs
                  << " vol=" << m_msg5Buffer << std::endl;
      Simulator::Schedule (MilliSeconds (phaseMs), &LteUeMac::SendRaPreambleNb, this, true);
      return;
    }
  m_raPreambleId = m_raPreambleUniformVariable->GetInteger (0, m_srContentionOffset - 1);
  SendRaPreambleNb (true);
}

void
LteUeMac::RestoreSrCeLevelAfterEdt (void)
{
  if (m_srCeLevelSwapped)
    {
      m_CeLevel = m_srCeLevelSaved;
      m_srCeLevelSwapped = false;
      m_edt = false;
    }
}

void
LteUeMac::SetSrEdtContention (bool en)
{
  m_srEdtContention = en;
}


void
LteUeMac::SendSchedulingRequest (void)
{
  NS_LOG_FUNCTION (this);
  if (m_rnti == 0 || m_idealBsrCb.IsNull ())
    {
      m_srPending = false;
      return;
    }
  // Suspend race guard: if the UE was suspended (PSM/eDRX) since this SR was
  // scheduled, do NOT fire. The SR's m_idealBsrCb path re-wakes the eNB
  // (NotifyDataActivity -> WakeFromPersistentGrant), which would resurrect a
  // just-suspended UE and spin the suspend<->wake loop. Data legitimately
  // arriving in PSM wakes the UE via the RRC resume path (DoSendData ->
  // IDLE_WAIT_MIB -> SR-resume), not via this direct SR.
  if (m_suspended)
    {
      m_srPending = false;
      return;
    }
  // Recompute the buffer at the SR instant (it may have grown or drained).
  uint64_t total = 0, txq = 0, rtxq = 0, statpdu = 0;
  for (auto & kv : m_ulBsrReceived)
    {
      txq     += kv.second.txQueueSize;
      rtxq    += kv.second.retxQueueSize;
      statpdu += kv.second.statusPduSize;
    }
  total = txq + rtxq + statpdu;
  NS_LOG_DEBUG ("SR fire RNTI=" << m_rnti << " total=" << total
                << " txQ=" << txq << " retxQ=" << rtxq << " statusPdu=" << statpdu);
  if (total == 0)
    {
      m_srPending = false; // buffer drained: the SR is no longer needed (cancelled)
      return;
    }
  // sr-ProhibitTimer (TS 36.331 SchedulingRequestConfig-NB): bar further SRs
  // for m_srProhibitPeriods SR periods after this transmission.
  uint64_t period = (m_srPeriodSubframes == 0 ? 1 : m_srPeriodSubframes);
  m_srProhibitUntil = Simulator::Now () + MilliSeconds (m_srProhibitPeriods * period);
  // The SR on air is a single, contention-free 1-bit NPRACH preamble on the
  // UE's dedicated SR resource (~5.6 ms Format-0) at NPRACH tx power -- the
  // real cost the oracle ignored, and faster than a 1-byte BSR (which would
  // need an NPUSCH). This preamble is the entire SR transmission.
  const double srMs = 5.6;   // single dedicated (contention-free) NPRACH tone
  m_cmacSapUser->NotifyEnergyState (NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPRACH);
  Simulator::Schedule (MilliSeconds (srMs + 1), &LteUeCmacSapUser::NotifyEnergyState,
                       m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);
  // TS 36.321 5.4.4: the SR carries NO buffer size. m_srBootstrapBytes is NOT
  // sent on air -- it is the eNB-side token that trips the grant gate so the
  // first, BLIND minimum-TBS grant issues (its +10 B overhead = BSR/header
  // room). The real buffer arrives only via the BSR tag on that first NPUSCH,
  // from which the eNB sizes the remaining grants.
  m_idealBsrCb (m_rnti, m_srBootstrapBytes);
  // The SR stays pending until cancelled by a grant (TS 36.321 5.4.4). If no
  // DCI N0 arrives within one prohibit window, re-send the SR (NB-IoT has no
  // dsr-TransMax cap; it retries on the dedicated NPRACH SR resource).
  m_srEvent = Simulator::Schedule (MilliSeconds (m_srProhibitPeriods * period),
                                   &LteUeMac::SendSchedulingRequest, this);
}


void
LteUeMac::SetLteUePhySapProvider (LteUePhySapProvider *s)
{
  m_uePhySapProvider = s;
}

LteMacSapProvider *
LteUeMac::GetLteMacSapProvider (void)
{
  return m_macSapProvider;
}

void
LteUeMac::SetLteUeCmacSapUser (LteUeCmacSapUser *s)
{
  m_cmacSapUser = s;
}

LteUeCmacSapProvider *
LteUeMac::GetLteUeCmacSapProvider (void)
{
  return m_cmacSapProvider;
}

void
LteUeMac::SetComponentCarrierId (uint8_t index)
{
  m_componentCarrierId = index;
}
uint64_t
LteUeMac::GetBufferSize(){
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
  uint64_t buffersize=0;
  for(it = m_ulBsrReceived.begin(); it != m_ulBsrReceived.end(); ++it){
      if((*it).second.lcid > 2){
        uint64_t data_per_lc =((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
        buffersize += data_per_lc;
      }
  }
  return buffersize;
}
uint64_t
LteUeMac::GetBufferSizeComplete(){
  uint64_t buffersize=0;
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
  for(it = m_ulBsrReceived.begin(); it != m_ulBsrReceived.end(); ++it){
        uint64_t data_per_lc =((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
        buffersize += data_per_lc;
  }
  return buffersize;
}
void
LteUeMac::DoTransmitPdu (LteMacSapProvider::TransmitPduParameters params)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (m_rnti == params.rnti, "RNTI mismatch between RLC and MAC");
  // Faithful hybrid-contention Msg3: transmit tagged with the RAR's Temp C-RNTI so
  // the eNB receives it on the Msg3 NPUSCH resource it allocated; the C-RNTI MAC CE
  // inside re-points identity to the real C-RNTI. All other PDUs use the real RNTI.
  uint16_t txRnti = (m_msg3HasCrntiMacCe && m_contentionTempRnti != 0)
                        ? m_contentionTempRnti : params.rnti;
  LteRadioBearerTag radioTag (txRnti, params.lcid, 0 /* UE works in SISO mode*/);
  if (params.lcid > 2)
    m_drbDataSentThisSession = true;   // real DRB data went out -> RAI may now signal on drain
  if (NbIotDebugTrace () && params.lcid > 2)
    std::cout << "[UE-TX] t=" << Simulator::Now ().GetSeconds () << " rnti=" << params.rnti
              << " txRnti=" << txRnti << " lcid=" << (uint32_t) params.lcid
              << " pduSize=" << (params.pdu ? params.pdu->GetSize () : 0) << std::endl;
  DataVolumeAndPowerHeadroomTag dprTag;
  BufferStatusReportTag bsrTag;
  uint64_t bsr =0;
  //DoSetTransmissionScheduled(false);
  // Also enter when a contention Msg3 is pending but the volume snapshot is EMPTY
  // (0 at RA trigger, or consumed by an interleaved transmission during the
  // RAR->Msg3 delay). Without this, that Msg3 went out with NO DPR and NO C-RNTI
  // MAC CE: the eNB could not consume it at the MAC (needs the CE) and fed the
  // 3-byte dummy to RRC as a CCCH message, whose ASN.1 deserializer decoded the
  // zeros as type 0 (Reestablishment, never sent in NB-IoT) and over-read the
  // buffer -> sim abort. It also wasted the whole RA round (identity unresolved).
  if(m_msg5Buffer > 0 || (m_msg3HasCrntiMacCe && m_raKeepCrnti != 0)){
    // We are just about to send MSG3, add DPR Element for MSG5 (potentially CIoT-Opt)
    //std::cout << " set payload" << std::endl;
    uint64_t msg3Volume = (m_msg5Buffer > 0) ? m_msg5Buffer : GetBufferSizeComplete ();
    uint8_t dataVolumeIndex = DataVolumeDPR::BufferSize2DVId(msg3Volume);
    m_msg5Buffer = 0;
    m_nextIsMsg5 = true;
    dprTag.SetDataVolumeValue(dataVolumeIndex);
    params.pdu->AddPacketTag(dprTag);
    // C-RNTI MAC CE on Msg3 (TS 36.321 5.1.5): a connected UE conveys its existing
    // C-RNTI so the eNB resolves contention by C-RNTI. The UE switches to its real
    // C-RNTI to MONITOR for Msg4 (the C-RNTI-addressed PDCCH), but contention
    // resolution is NOT complete yet -- it is confirmed only on receiving Msg4
    // (see the DCI handler) within mac-ContentionResolutionTimer.
    if (m_msg3HasCrntiMacCe && m_raKeepCrnti != 0)
      {
        CRntiMacCeTag crntiTag (m_raKeepCrnti);
        params.pdu->AddPacketTag (crntiTag);
        NS_LOG_INFO ("Msg3 carries C-RNTI MAC CE=" << m_raKeepCrnti
                     << "; awaiting Msg4 (Temp C-RNTI=" << m_rnti << ")");
        m_crniTempForCr = m_rnti;          // keep the Temp C-RNTI until Msg4 confirms
        m_rnti = m_raKeepCrnti;            // monitor Msg4 on the real C-RNTI
        m_cmacSapUser->SetTemporaryCellRnti (m_rnti);
        m_awaitingContentionResolution = true;
        m_contentionResolutionTimer.Cancel ();
        // mac-ContentionResolutionTimer (TS 36.321 5.1.5). NB-IoT expresses this in
        // PDCCH periods (pp1..pp64); this cell broadcasts pp32 for all CE levels. We
        // run CE0 only, where pp32 = 32 x NPDCCH period (~16 ms at CE0: npdcch-
        // NumRepetitionsRA r8 x npdcchStartSfCssRa v2) ~= 512 ms. The eNB's grant to
        // the resolved C-RNTI routinely lands after 64 ms under load, so the flat
        // 64 ms timed out before resolution. Hardcoded for CE0; TODO: derive from the
        // SIB macContentionResolutionTimer x NPDCCH period per CE level.
        m_contentionResolutionTimer = Simulator::Schedule (
            MilliSeconds (512), &LteUeMac::ContentionResolutionTimeout, this);
        m_msg3HasCrntiMacCe = false;
        m_raKeepCrnti = 0;
        m_contentionTempRnti = 0;
        // This Msg3 is a contention-resolution CONTROL PDU, not an EDT Msg5: the
        // user data must flow on the FIRST post-resolution grant. m_nextIsMsg5 (set
        // with the DPR above) would make that grant skip the DRB (LCID>2) and strand
        // the data -- clear it so the data is sent.
        m_nextIsMsg5 = false;
      }
  }
  else{

    bsr = GetBufferSizeComplete();
    if(bsr > 0){

      bsrTag.SetBufferStatusReportIndex(BufferSizeLevelBsr::BufferSize2BsrId (bsr));
      params.pdu->AddPacketTag(bsrTag);
    }
    // Normal PDU just add BSR for next Packet

    // Oracle / ideal-BSR: close the grant session once the granted data has
    // drained the buffer. NB-IoT RLC-UM never re-reports an emptied queue, so
    // the total==0 reset in DoReportBufferStatus never runs; and a connected
    // (non-deep-sleep) oracle UE never hits the PSM/eDRX reset either. Without
    // this, m_grantSessionActive stays true forever and the oracle's
    // !m_grantSessionActive guard blocks every packet after the first.
    if (m_oracleBsr && bsr == 0)
      {
        m_grantSessionActive = false;
      }

    // Proactive FUG: a UE with UL data pending stays CONNECTED-monitoring (it
    // never suspends/resumes), so neither the PSM/eDRX reset nor the resume
    // reset of m_grantSessionActive ever runs. Once the granted DATA has drained
    // the buffer, close the grant session here so the NEXT packet can again
    // trigger the predicted push / fallback SR. NB-IoT RLC-UM leaves a small
    // status-PDU residue on the signalling LCs that never drains, so test the
    // DATA-only buffer (GetBufferSize, LCID>2): keying off the full buffer would
    // leave the session active forever and strand every packet after the first.
    if (m_proactiveFug && GetBufferSize () == 0)
      {
        m_grantSessionActive = false;
      }

    NS_LOG_INFO("LteUeMac::DoTransmitPdu RNTI: " << m_rnti << ", Id: " << params.pdu->GetUid () << ", Size: " << params.pdu->GetSize() << " bytes, " << params.pdu->GetSerializedSize() << " bytes");

  }

  params.pdu->AddPacketTag (radioTag);
  // store pdu in HARQ buffer
  //m_miUlHarqProcessesPacket.at (m_harqProcessId)->AddPacket (params.pdu);
  //m_miUlHarqProcessesPacketTimer.at (m_harqProcessId) = HARQ_PERIOD;
  m_uePhySapProvider->SendMacPdu (params.pdu);
}

void
LteUeMac::DoReportBufferStatus (LteMacSapProvider::ReportBufferStatusParameters params)
{
  NS_LOG_FUNCTION (this << (uint32_t) params.lcid);

  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;

  it = m_ulBsrReceived.find (params.lcid);
  if (it != m_ulBsrReceived.end ())
    {
      // update entry
      (*it).second = params;
    }
  else
    {
      m_ulBsrReceived.insert (std::pair<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> (
          params.lcid, params));
    }
  m_freshUlBsr = true;

  // Dedicated-NPRACH Scheduling Request (replaces the ideal-BSR oracle).
  // State machine (TS 36.321 5.4.4 + 5.4.5):
  //  - new data, no active grant session -> transmit an SR (carries no buffer)
  //    at the next dedicated-SR occasion: contention-free, real tx cost, up to
  //    one SR period of latency, instead of a zero-delay/zero-cost oracle.
  //  - once the SR yields a grant (m_grantSessionActive, set in the DCI N0
  //    handler) the buffer flows via the BSR MAC CE, NOT further SRs.
  //  - buffer drained -> close the session; the next packet starts a new SR.
  if (m_persistentGrant && m_rnti != 0 && !m_idealBsrCb.IsNull ())
  {
    uint64_t total = 0;
    for (auto & kv : m_ulBsrReceived)
    {
      total += kv.second.txQueueSize + kv.second.retxQueueSize + kv.second.statusPduSize;
    }
    // 0->n rising edge == a genuinely new packet reaching the MAC. 
    bool freshArrival = (m_prevBsrTotal == 0 && total > 0);
    // Fire the oracle on ANY growth of the UL buffer, not only the 0->n edge. 
    bool newUngrantedData = (total > m_prevBsrTotal);
    if (NbIotDebugTrace () && newUngrantedData)
    {
      std::cout << "[ARRIVAL] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
                << " total=" << total << " prev=" << m_prevBsrTotal
                << " fresh=" << freshArrival << " sessionActiveAtArrival=" << m_grantSessionActive
                << " srPending=" << m_srPending << " suspended=" << m_suspended
                << " oracle=" << (m_idealBsrCb.IsNull()?0:1) << std::endl;
    }
    m_prevBsrTotal = total;
    if (total == 0)
    {
      // AS RAI (TS 36.321 5.4.5, NB-IoT): a zero-byte BSR has been triggered at
      // the end of an active grant session and no further UL/DL data is expected
      // (single-packet-per-epoch). Signal the AS RAI MAC CE so the eNB releases
      // immediately, instead of waiting for the data-inactivity timer.
      // Only signal RAI once the UE has actually sent its DRB DATA this session. During a
      // resume the signalling PDUs (Msg3/resume-complete) drain first, so the BSR hits 0
      // BEFORE the data is reported -- firing RAI here would release the UE pre-data (a
      // stale release that later strands the next packet). m_grantSessionActive is already
      // true from the signalling grant, so it can't distinguish this; m_drbDataSentThisSession does.
      if (m_grantSessionActive && m_drbDataSentThisSession && m_raiActivation && m_rnti != 0 && !m_raiCb.IsNull ())
      {
        m_raiCb (m_rnti);
      }
      // Buffer drained: grant session over; next packet re-requests via SR.
      m_grantSessionActive = false;
    }
    else if (m_oracleBsr && !m_suspended && newUngrantedData)
    {
      if (NbIotDebugTrace ())
        std::cout << "[ORACLE-FIRE] t=" << Simulator::Now ().GetSeconds ()
                  << " rnti=" << m_rnti << " total=" << total
                  << " readvertise=" << (m_grantSessionActive ? 1 : 0) << std::endl;
      m_grantSessionActive = true;
      if (!m_idealBsrCb.IsNull ())
        m_idealBsrCb (m_rnti, total);
    }
    else if (m_oracleBsr && m_grantSessionActive && !m_suspended)
    {
      // Session active, NOT a fresh arrival: a benign re-report of the same packet still
      // awaiting its granted NPUSCH transmission. No state change (diagnostic only).
      if (NbIotDebugTrace ())
        std::cout << "[ORACLE-STRANDED] t=" << Simulator::Now ().GetSeconds ()
                  << " rnti=" << m_rnti << " total=" << total << std::endl;
    }
    else if (!m_srPending && !m_suspended
             && (!m_grantSessionActive || newUngrantedData)
             && Simulator::Now () >= m_srProhibitUntil
             && !(m_proactiveFug && m_fugBootstrapEpochs >= kProactiveBootstrapEpochs))
    {
      // Fire the SR on genuinely NEW data (newUngrantedData: buffer grew) EVEN IF the grant
      // session is flagged active. m_grantSessionActive is set on any grant and cleared only
      // when the buffer drains to EXACTLY 0; a single strand (e.g. a UE suspended with a
      // trailing bootstrap PDU still pending) leaves the buffer > 0, so the latch sticks true
      // forever and the raw !m_grantSessionActive guard would block the SR for EVERY later
      // packet -> the UE accumulates and never transmits (observed: hybridsr backlog cascade,
      // buffer 87->399 over epochs, 7 s delay). Mirrors the oracle-branch fire-on-growth fix.
      if (m_proactiveFug)
        m_fugBootstrapEpochs++;
      // !m_suspended: while RRC-suspended (PSM/eDRX) the UE must NOT fire a
      // direct SR -- data wakes it via the RRC resume path (DoSendData ->
      // IDLE_WAIT_MIB -> SR-resume). Firing here would re-wake a just-suspended
      // UE and spin the suspend<->wake loop.
      m_srPending = true;
      m_srContentionFailures = 0;   // fresh packet: start the RA-cost estimate from a single clean attempt
      if (m_srPreamble)
        {
          // Faithful dedicated SR: transmit a real NPRACH preamble on this UE's
          // reserved subcarrier at its next round-robin occasion. The eNB resolves
          // identity from its resource->RNTI map (no identity on the air).
          uint64_t wait = MsToNextDedicatedOccasion ();
          m_srEvent = Simulator::Schedule (MilliSeconds (wait),
                                           &LteUeMac::SendDedicatedSrPreamble, this);
          NS_LOG_DEBUG ("UE MAC: dedicated-SR preamble RNTI=" << m_rnti
                        << " idx=" << m_srIndex << " in " << wait << " ms");
          // Hybrid: also start contending on the shared pool at the next base
          // occasion (the reserved slot above is the guaranteed floor).
          if (m_srHybridContention && m_srContentionOffset > 0)
            {
              m_srContentionEvent = Simulator::Schedule (
                  MilliSeconds (MsToNextBaseOccasion ()),
                  &LteUeMac::SendContentionSrPreamble, this);
            }
        }
      else
        {
          // Legacy idealBsr SR (non-preamble): fire at the next dedicated turn.
          uint64_t period = (m_srPeriodSubframes == 0 ? 1 : m_srPeriodSubframes);
          uint64_t dedicatedWait = period - (Simulator::Now ().GetMilliSeconds () % period);
          m_srEvent = Simulator::Schedule (MilliSeconds (dedicatedWait),
                                           &LteUeMac::SendSchedulingRequest, this);
        }
    }
    else if (newUngrantedData && total > 0 && !m_oracleBsr)
    {
      // Diagnostic: new UL data present but NO branch requested a grant. Print WHICH
      // guard blocked the SR (the hybridsr imsi=64 strand: UE CONNECTED, direct-sent
      // to the DRB, but never re-announced via SR -> parked eNB never re-grants).
      if (NbIotDebugTrace ())
        std::cout << "[SR-SKIPPED] t=" << Simulator::Now ().GetSeconds ()
                  << " rnti=" << m_rnti << " total=" << total
                  << " srPending=" << m_srPending << " suspended=" << m_suspended
                  << " grantSession=" << m_grantSessionActive
                  << " prohibitInMs=" << (Simulator::Now () >= m_srProhibitUntil ? 0.0
                         : (double)(m_srProhibitUntil - Simulator::Now ()).GetMilliSeconds ())
                  << std::endl;
    }
  }
}

void
LteUeMac::SendReportBufferStatus (void)
{
  NS_LOG_FUNCTION (this);

  if (m_rnti == 0)
    {
      NS_LOG_INFO ("MAC not initialized, BSR deferred");
      return;
    }

  if (m_ulBsrReceived.size () == 0)
    {
      NS_LOG_INFO ("No BSR report to transmit");
      return;
    }
  MacCeListElement_s bsr;
  bsr.m_rnti = m_rnti;
  bsr.m_macCeType = MacCeListElement_s::BSR;

  // BSR is reported for each LCG
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
  std::vector<uint32_t> queue (4, 0); // one value per each of the 4 LCGs, initialized to 0
  for (it = m_ulBsrReceived.begin (); it != m_ulBsrReceived.end (); it++)
    {
      uint8_t lcid = it->first;
      std::map<uint8_t, LcInfo>::iterator lcInfoMapIt;
      lcInfoMapIt = m_lcInfoMap.find (lcid);
      NS_ASSERT (lcInfoMapIt != m_lcInfoMap.end ());
      NS_ASSERT_MSG ((lcid != 0) ||
                         (((*it).second.txQueueSize == 0) && ((*it).second.retxQueueSize == 0) &&
                          ((*it).second.statusPduSize == 0)),
                     "BSR should not be used for LCID 0");
      uint8_t lcg = lcInfoMapIt->second.lcConfig.logicalChannelGroup;
      queue.at (lcg) +=
          ((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
    }

  // FF API says that all 4 LCGs are always present
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (0)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (1)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (2)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (3)));

  // create the feedback to eNB
  Ptr<BsrLteControlMessage> msg = Create<BsrLteControlMessage> ();
  msg->SetBsr (bsr);
  m_uePhySapProvider->SendLteControlMessage (msg);
}

void
LteUeMac::RandomlySelectAndSendRaPreamble ()
{
  NS_LOG_FUNCTION (this);
  // 3GPP 36.321 5.1.1
  NS_ASSERT_MSG (m_rachConfigured, "RACH not configured");
  // assume that there is no Random Access Preambles group B
  m_raPreambleId =
      m_raPreambleUniformVariable->GetInteger (0, m_rachConfig.numberOfRaPreambles - 1);
  bool contention = true;
  SendRaPreamble (contention);
}

void
LteUeMac::RandomlySelectAndSendRaPreambleNb ()
{
  NS_LOG_FUNCTION (this);
  // 3GPP 36.321 5.1.1
  NS_ASSERT_MSG (m_nprachConfigured, "NPRACH not configured");
  // assume that there is no Random Access Preambles group B
  m_raPreambleId = m_raPreambleUniformVariable->GetInteger (0, NbIotRrcSap::ConvertNprachNumSubcarriers2int (m_CeLevel) - 1);
  if (NbIotDebugTrace ())
    std::cout << "[UE-PREAMBLE] t=" << Simulator::Now ().GetSeconds () << " imsi=" << m_imsi
              << " rapId=" << m_raPreambleId << " macEdt=" << m_edt
              << " subOffset=" << (uint32_t) NbIotRrcSap::ConvertNprachSubcarrierOffset2int (m_CeLevel)
              << " numSub=" << (uint32_t) NbIotRrcSap::ConvertNprachNumSubcarriers2int (m_CeLevel) << std::endl;
  bool contention = true;

  // NPRACH WINDOW STARTS at framenumber mod (NPRACH_PERIOD/10) = 0 (A Tutorial on NB-IoT Physical Layer Design, Mathhieu Kanj, et al.)
  //uint32_t currentsubframe = (m_frameNo - 1)*10 +(m_subframeNo-1);
  uint32_t currentsubframe = Simulator::Now().GetMilliSeconds();
  uint16_t window_condition = ( currentsubframe/10) % (NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel) / 10);
  uint32_t lastPeriodStart = (currentsubframe/10) - window_condition;
  uint32_t startSubframeNprachOccasion = lastPeriodStart*10 + NbIotRrcSap::ConvertNprachStartTime2int(m_CeLevel);
  if (startSubframeNprachOccasion != currentsubframe)
    {
      uint16_t subframesToWait = 0;
      if(currentsubframe < startSubframeNprachOccasion){
        subframesToWait = startSubframeNprachOccasion-currentsubframe;
      }else{
        subframesToWait = (NbIotRrcSap::ConvertNprachPeriodicity2int(m_CeLevel) - (currentsubframe % NbIotRrcSap::ConvertNprachPeriodicity2int(m_CeLevel)))+NbIotRrcSap::ConvertNprachStartTime2int(m_CeLevel);
      }

      //uint16_t frames_to_wait = (NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel) - window_condition*10) + (10-(m_subframeNo-1))%10;
      //NS_BUILD_DEBUG(std::cout << m_frameNo*10+m_subframeNo << std::endl);
      m_logging.push_back(currentsubframe+subframesToWait);
      //NS_BUILD_DEBUG(std::cout  << "Frames to wait:" << subframesToWait << std::endl);
      Simulator::Schedule (MilliSeconds (subframesToWait), &LteUeMac::SendRaPreambleNb, this,
                           contention);
    }
  else{
    SendRaPreambleNb(contention);
  }

}
void
LteUeMac::SendRaPreamble (bool contention)
{
  NS_LOG_FUNCTION (this << (uint32_t) m_raPreambleId << contention);
  // Since regular UL LteControlMessages need m_ulConfigured = true in
  // order to be sent by the UE, the rach preamble needs to be sent
  // with a dedicated primitive (not
  // m_uePhySapProvider->SendLteControlMessage (msg)) so that it can
  // bypass the m_ulConfigured flag. This is reasonable, since In fact
  // the RACH preamble is sent on 6RB bandwidth so the uplink
  // bandwidth does not need to be configured.
  NS_ASSERT (m_subframeNo > 0); // sanity check for subframe starting at 1
  m_raRnti = m_subframeNo - 1;
  m_uePhySapProvider->SendRachPreamble (m_raPreambleId, m_raRnti);
  NS_LOG_INFO (this << " sent preamble id " << (uint32_t) m_raPreambleId << ", RA-RNTI "
                    << (uint32_t) m_raRnti);
  // 3GPP 36.321 5.1.4
  //Time raWindowBegin = MilliSeconds (3);
  //Time raWindowEnd = MilliSeconds (3 + m_rachConfig.raResponseWindowSize);
  //Simulator::Schedule (raWindowBegin, &LteUeMac::StartWaitingForRaResponse, this);
  //m_noRaResponseReceivedEvent = Simulator::Schedule (raWindowEnd, &LteUeMac::RaResponseTimeout, this, contention);
}
void
LteUeMac::SendRaPreambleNb (bool contention)
{
  NS_LOG_FUNCTION (this << (uint32_t) m_raPreambleId << contention);

  NS_ASSERT (m_frameNo > 0); // sanity check for subframe starting at 1

  // ETSI 36.321 5.1.4
  m_raRnti = 1 + floor (m_frameNo / 4);

  m_radioResourceConfig.nprachConfig.nprachCpLength =
      NbIotRrcSap::NprachConfig::NprachCpLength::us266dot7;
  double ts = 1000.0 / (15000.0 * 2048.0);
  double preambleSymbolTime = 8192.0 * ts;
  double preambleGroupTimeNoCP = 5.0 * preambleSymbolTime;
  double preambleGroupTime =
      NbIotRrcSap::ConvertNprachCpLenght2double (m_radioResourceConfig.nprachConfig) +
      preambleGroupTimeNoCP;
  double preambleRepetition = 4.0 * preambleGroupTime;
  double time = NbIotRrcSap::ConvertNumRepetitionsPerPreambleAttempt2int (m_CeLevel) *
                                  preambleRepetition;

  m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPRACH);
  //Schedule EnergyStateChange on the next subframe after transmission
  Simulator::Schedule (MilliSeconds (time+1), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);
  Simulator::Schedule (MilliSeconds (time), &LteUePhySapProvider::SendNprachPreamble,
                       m_uePhySapProvider, m_raPreambleId, m_raRnti,
                       NbIotRrcSap::ConvertNprachSubcarrierOffset2int (m_CeLevel));
  NS_LOG_INFO (this << " sent preamble id " << (uint32_t) m_raPreambleId << ", RA-RNTI "
                    << (uint32_t) m_raRnti);

  if (m_mac_logging)
  {
    LogMessage("SendRaPreambleNb");
  }

  // 3GPP 36.321 5.1.4
  Time raWindowBegin;
  Time raWindowEnd;
  uint32_t npdcchPeriod = NbIotRrcSap::ConvertNpdcchNumRepetitionsRa2int (m_CeLevel) *
                          NbIotRrcSap::ConvertNpdcchStartSfCssRa2double (m_CeLevel);

  if (NbIotRrcSap::ConvertNumRepetitionsPerPreambleAttempt2int (m_CeLevel) >= 64)
    {
      raWindowBegin = MilliSeconds (41);
      //NS_BUILD_DEBUG(std::cout << (m_frameNo - 1) * 10 + (m_subframeNo - 1) + time + 41 + NbIotRrcSap::ConvertRaResponseWindowSize2int (m_rachConfigCe) * npdcchPeriod << std::endl);
      raWindowEnd = MilliSeconds (
          time + 41 + NbIotRrcSap::ConvertRaResponseWindowSize2int (m_rachConfigCe) * npdcchPeriod);
    }
  else
    {
      raWindowBegin = MilliSeconds (4);
      //NS_BUILD_DEBUG(std::cout << (m_frameNo - 1) * 10 + (m_subframeNo - 1) + time + 4 + NbIotRrcSap::ConvertRaResponseWindowSize2int (m_rachConfigCe) * npdcchPeriod << std::endl);
      raWindowEnd = MilliSeconds (
          time + 4 + NbIotRrcSap::ConvertRaResponseWindowSize2int (m_rachConfigCe) * npdcchPeriod);
    }
  //Time raWindowEnd = MilliSeconds (4 + 8*10240);
  //Time raWindowEnd = MilliSeconds (4 + m_rachConfig.raResponseWindowSize);
  //NS_BUILD_DEBUG(std::cout << (m_frameNo - 1) * 10 + (m_subframeNo - 1) + time << std::endl);
  Simulator::Schedule (raWindowBegin, &LteUeMac::StartWaitingForRaResponse, this);
  m_listenToSearchSpaces = true;
  m_noRaResponseReceivedEvent =
      Simulator::Schedule (raWindowEnd, &LteUeMac::RaResponseTimeoutNb, this, contention);
}
void
LteUeMac::StartWaitingForRaResponse ()
{
  NS_LOG_FUNCTION (this);
  m_waitingForRaResponse = true;
}
void
LteUeMac::StartWaitingForRaResponseNb ()
{
  NS_LOG_FUNCTION (this);
  m_waitingForRaResponse = true;
}

void
LteUeMac::RecvRaResponse (BuildRarListElement_s raResponse)
{
  NS_LOG_FUNCTION (this);
  m_waitingForRaResponse = false;
  m_noRaResponseReceivedEvent.Cancel ();
  NS_LOG_INFO ("got RAR for RAPID " << (uint32_t) m_raPreambleId
                                    << ", setting T-C-RNTI = " << raResponse.m_rnti);
  m_rnti = raResponse.m_rnti;
  m_cmacSapUser->SetTemporaryCellRnti (m_rnti);
  // in principle we should wait for contention resolution,
  // but in the current LTE model when two or more identical
  // preambles are sent no one is received, so there is no need
  // for contention resolution
  m_cmacSapUser->NotifyRandomAccessSuccessful (false);
  // trigger tx opportunity for Message 3 over LC 0
  // this is needed since Message 3's UL GRANT is in the RAR, not in UL-DCIs
  const uint8_t lc0Lcid = 0;
  std::map<uint8_t, LcInfo>::iterator lc0InfoIt = m_lcInfoMap.find (lc0Lcid);
  NS_ASSERT (lc0InfoIt != m_lcInfoMap.end ());
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator lc0BsrIt =
      m_ulBsrReceived.find (lc0Lcid);
  if ((lc0BsrIt != m_ulBsrReceived.end ()) && (lc0BsrIt->second.txQueueSize > 0))
    {
      NS_ASSERT_MSG (raResponse.m_grant.m_tbSize > lc0BsrIt->second.txQueueSize,
                     "segmentation of Message 3 is not allowed");
      // this function can be called only from primary carrier
      if (m_componentCarrierId > 0)
        {
          NS_FATAL_ERROR ("Function called on wrong componentCarrier");
        }
      LteMacSapUser::TxOpportunityParameters txOpParams;
      txOpParams.bytes = raResponse.m_grant.m_tbSize;
      txOpParams.layer = 0;
      txOpParams.harqId = 0;
      txOpParams.componentCarrierId = m_componentCarrierId;
      txOpParams.rnti = m_rnti;
      txOpParams.lcid = lc0Lcid;
      lc0InfoIt->second.macSapUser->NotifyTxOpportunity (txOpParams);
      lc0BsrIt->second.txQueueSize = 0;
    }
}

void
LteUeMac::RecvRaResponseNb (NbIotRrcSap::RarPayload raResponse)
{
  NS_LOG_FUNCTION (this);
  m_waitingForRaResponse = false;
  m_noRaResponseReceivedEvent.Cancel ();
  NS_LOG_INFO ("got RAR for RAPID " << (uint32_t) m_raPreambleId
                                    << ", setting T-C-RNTI = " << raResponse.cellRnti);
  // Drop a stale/orphaned RAR for a still-connected UE -- UNLESS this is an
  // intentional connected-UE RA that keeps its C-RNTI (m_raKeepCrnti armed), in
  // which case the RAR's Temporary C-RNTI is used to send Msg3 with the C-RNTI
  // MAC CE and contention is resolved by C-RNTI at Msg4.
  if (m_persistentGrant && m_rnti != 0 && m_raKeepCrnti == 0){
    NS_LOG_WARN ("RecvRaResponseNb: dropping stale RAR under persistent grant"
                 << " (live RNTI=" << m_rnti
                 << ", RAR T-C-RNTI=" << raResponse.cellRnti << ")"
                 << " — orphaned RA timeout fired after suspension");
    return;
  }

  if (m_mac_logging)
  {
    std::string msg = "RecvRaResponseNb,cellRNTI," + std::to_string(raResponse.cellRnti) + ",";
    LogMessage(msg);
  }

  // C-RNTI MAC CE (TS 36.321 5.1.5): a connected UE doing contention RA signals its
  // existing C-RNTI in a C-RNTI MAC CE on Msg3; the eNB resolves identity by that
  // C-RNTI and grants the existing context. KEEP the real C-RNTI (do NOT adopt the
  // Temporary C-RNTI): Msg3 rides the RAR's resource-based grant, and keying it to
  // the real C-RNTI lets the buffered DRB data ride that grant without an RLC/MAC
  // RNTI mismatch (DoTransmitPdu, "RNTI mismatch" assert). Idle RA (m_raKeepCrnti==0)
  // still adopts the Temp C-RNTI to send its CCCH Msg3.
  if (m_raKeepCrnti != 0)
    {
      m_msg3HasCrntiMacCe = true;
      // KEEP the real C-RNTI for RLC keying (so the buffered DRB data rides Msg3
      // without an RLC/MAC mismatch), but remember the RAR's Temporary C-RNTI: the
      // Msg3 must be TRANSMITTED tagged with it so the eNB receives it on the Msg3
      // NPUSCH resource it allocated for that Temp C-RNTI. The eNB then reads the
      // C-RNTI MAC CE (real) + DPR from the data section and allocates NPUSCH on the
      // real C-RNTI.
      m_contentionTempRnti = raResponse.cellRnti;
    }
  else
    {
      m_rnti = raResponse.cellRnti;
      m_cmacSapUser->SetTemporaryCellRnti (m_rnti);
    }
  // in principle we should wait for contention resolution,
  // but in the current LTE model when two or more identical
  // preambles are sent no one is received, so there is no need
  // for contention resolution

  // To be comented in
  if (NbIotDebugTrace ())
    std::cout << "[EDT-GATE] imsi=" << m_imsi << " tbs_size=" << raResponse.ulGrant.tbs_size
              << " ueEdt=" << m_edt << std::endl;
  bool edt;
  if(raResponse.ulGrant.tbs_size > 88){
    // We got a grant for EDT
    edt = true;
  }else{
    edt = false;
  }
  m_cmacSapUser->NotifyRandomAccessSuccessful (edt);

  // EDT (Rel-15) on the hybrid-contention SR route: an EDT-sized Msg3 grant
  // (tbs > 88) carries the DATA itself (plus the C-RNTI MAC CE + DPR attached in
  // DoTransmitPdu) -- fall through to the data-LC txop path below instead of the
  // 3-byte control dummy. The grant is large enough that nothing fragments (the
  // historical data-on-tiny-Msg3 deserializer crash does not apply). If this UE
  // loses the captured collision, the CR-timeout path re-arms the RLC via
  // NotifyHarqDeliveryFailure (txed->retx) so the data is not stranded.
  if (m_msg3HasCrntiMacCe && edt)
    {
      m_edtMsg3DataSent = true;
    }
  else if (m_msg3HasCrntiMacCe)
    {
      // Faithful hybrid-contention Msg3: a CONTROL-only Msg3. Build a minimal MAC PDU
      // on which DoTransmitPdu attaches the DPR (BSR) + C-RNTI MAC CE and tags the
      // radio with the Temp C-RNTI. The eNB receives it on the Msg3 resource, resolves
      // the real C-RNTI, and allocates NPUSCH from the DPR; the DATA flows afterward on
      // the real C-RNTI. (Carrying a DRB data fragment on the tiny Msg3 grant crashed
      // the eNB packet deserializer under heavy contention load.)
      LteMacSapProvider::TransmitPduParameters tp;
      tp.pdu = Create<Packet> (3);
      tp.rnti = m_rnti;
      tp.lcid = 0;
      tp.componentCarrierId = m_componentCarrierId;
      uint32_t subframesTillNpusch = raResponse.ulGrant.subframes.second.front ()
                                     - (10 * (m_frameNo - 1) + m_subframeNo - 1);
      m_transmissionScheduled = true;
      Simulator::Schedule (MilliSeconds (subframesTillNpusch),
                           &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser,
                           NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPUSCH);
      Simulator::Schedule (MilliSeconds (subframesTillNpusch),
                           &LteUeMac::DoTransmitPdu, this, tp);
      return;
    }

  // trigger tx opportunity for Message 3 over the RAR grant (Msg3's UL grant is in
  // the RAR, not in UL-DCIs). Normal RA: Msg3 is the CCCH ConnectionRequest on LC0.
  // Faithful hybrid-contention RA (connected UE, m_msg3HasCrntiMacCe): there is no
  // CCCH message -- Msg3 instead carries the C-RNTI MAC CE (the saved C-RNTI,
  // attached in DoTransmitPdu) on the UE's DATA LC, so the eNB resolves identity by
  // the saved C-RNTI and reuses the existing context instead of issuing a new RNTI.
  uint8_t lc0Lcid = 0;
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator lc0BsrIt =
      m_ulBsrReceived.find (lc0Lcid);
  if (m_msg3HasCrntiMacCe
      && (lc0BsrIt == m_ulBsrReceived.end () || lc0BsrIt->second.txQueueSize == 0))
    {
      for (auto & kv : m_ulBsrReceived)
        if (kv.first > 2 && kv.second.txQueueSize > 0) { lc0Lcid = kv.first; break; }
      lc0BsrIt = m_ulBsrReceived.find (lc0Lcid);
    }
  std::map<uint8_t, LcInfo>::iterator lc0InfoIt = m_lcInfoMap.find (lc0Lcid);
  if (lc0InfoIt != m_lcInfoMap.end ()
      && (lc0BsrIt != m_ulBsrReceived.end ()) && (lc0BsrIt->second.txQueueSize > 0))
    {
      // NS_ASSERT_MSG (raResponse.m_grant.m_tbSize > lc0BsrIt->second.txQueueSize,
      //               "segmentation of Message 3 is not allowed");
      // this function can be called only from primary carrier
      if (m_componentCarrierId > 0)
        {
          NS_FATAL_ERROR ("Function called on wrong componentCarrier");
        }
      LteMacSapUser::TxOpportunityParameters txOpParams;



      txOpParams.bytes = raResponse.ulGrant.tbs_size/8;
      txOpParams.layer = 0;
      txOpParams.harqId = 0;
      txOpParams.componentCarrierId = m_componentCarrierId;
      txOpParams.rnti = m_rnti;
      txOpParams.lcid = lc0Lcid;
      int subframes = raResponse.ulGrant.subframes.second.back() -
                      (10 * (m_frameNo - 1) + m_subframeNo - 1);

      uint32_t subframesTillNpusch = raResponse.ulGrant.subframes.second.front() - (10*(m_frameNo-1)+m_subframeNo-1);

      m_transmissionScheduled = true;
      Simulator::Schedule(MilliSeconds(subframesTillNpusch), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPUSCH);

      //EnergyStateChange on the next Subframe after Transmission Completed
      Simulator::Schedule(MilliSeconds(subframes+1), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);

      //Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
      //                     lc0InfoIt->second.macSapUser, txOpParams);
      lc0InfoIt->second.macSapUser->NotifyTxOpportunityNb(txOpParams,subframes);
      // CCCH Msg3 (LC0) is one-shot -> consume it. For the data LC (faithful
      // contention RA) the grant is small and the RLC segments: leave the queue so
      // the remainder is re-reported (BSR) and granted on the resolved C-RNTI.
      if (lc0Lcid == 0)
        lc0BsrIt->second.txQueueSize = 0;
    }
}

void
LteUeMac::RaResponseTimeout (bool contention)
{
  NS_LOG_FUNCTION (this << contention);
  m_waitingForRaResponse = false;
  // 3GPP 36.321 5.1.4
  ++m_preambleTransmissionCounter;
  //fire RA response timeout trace
  m_raResponseTimeoutTrace (m_imsi, contention, m_preambleTransmissionCounter,
                            m_rachConfig.preambleTransMax + 1);
  if (m_preambleTransmissionCounter == m_rachConfig.preambleTransMax + 1)
    {
      NS_LOG_INFO ("RAR timeout, preambleTransMax reached => giving up");
      m_cmacSapUser->NotifyRandomAccessFailed ();
    }
  else
    {
      NS_LOG_INFO ("RAR timeout, re-send preamble");
      if (contention)
        {
          RandomlySelectAndSendRaPreamble ();
        }
      else
        {
          SendRaPreamble (contention);
        }
    }
}

void
LteUeMac::RaResponseTimeoutNb (bool contention)
{
  // Based on ETSI TS 136 321 V13.9.0, 5.1.4: Random Access Response reception:
  // When RAP fails, and the MaxNumPreambleAttemptCE counter is reached, the UE resets MaxNumPreambleAttemptCE
  // and retries in the next CE level, until preambleTransMax is reached
  NS_LOG_FUNCTION (this << contention);
  m_waitingForRaResponse = false;
  // Faithful hybrid-contention SR: no RAR for our contention preamble this
  // occasion (nothing decoded). NOT a RA failure -- keep contending at the next
  // base occasion (the dedicated reserved slot is the guaranteed floor); do not
  // CE-escalate or count toward preambleTransMax.
  if (m_srContentionRa)
    {
      m_srContentionRa = false;
      m_raKeepCrnti = 0;
      m_msg5Buffer = 0;
      m_msg3HasCrntiMacCe = false;
      RestoreSrCeLevelAfterEdt ();   // no RAR: leave the EDT partition before retrying/peeling
      if (m_srPending)
        {
          ++m_srContentionFailures;   // no RAR this occasion -> RA route costlier; gate may peel this UE to dedicated
          // TS 36.321 5.1.4: back off (BACKOFF from the contention RAR's BI) before
          // re-contending so colliders that lost spread across occasions instead of
          // re-colliding in lockstep.
          uint32_t backoffMs = (m_backoffParameter > 0)
              ? m_raPreambleUniformVariable->GetInteger (0, m_backoffParameter) : 0;
          m_srContentionEvent = Simulator::Schedule (
              MilliSeconds (MsToNextBaseOccasion () + backoffMs),
              &LteUeMac::SendContentionSrPreamble, this);
        }
      return;
    }
  //NS_BUILD_DEBUG(std::cout << "Window End" << std::endl);
  // 3GPP 36.321 5.1.4
  ++m_preambleTransmissionCounter;
  ++m_preambleTransmissionCounterCe;
  //fire RA response timeout trace
  m_raResponseTimeoutTrace (m_imsi, contention, m_preambleTransmissionCounter,
                            m_rachConfig.preambleTransMax);
  if (m_preambleTransmissionCounter == m_radioResourceConfig.rachConfigCommon.preambleTransMaxCE)
    {
      NS_LOG_INFO ("RAR timeout, preambleTransMax reached => giving up");
      if (m_mac_logging) LogMessage("LteUeMac::RaResponseTimeoutNb,RAR timeout: preambleTransMax reached");

      m_cmacSapUser->NotifyRandomAccessFailed ();
    }
  else
    {
      if (m_preambleTransmissionCounterCe ==
      NbIotRrcSap::ConvertMaxNumPreambleAttemptCE2int (m_CeLevel)) // Max. number of retries in this CE level reached
        {
          m_preambleTransmissionCounterCe = 0;
          NbIotRrcSap::NprachParametersNbR14 tmp = {}; // needed if EDT is enabled

          if (m_CeLevel.coverageEnhancementLevel == m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb0.coverageEnhancementLevel) // CE0
            {
              // Increase to CE1
              NS_LOG_INFO ("RAR timeout, MaxNumPreambleAttemptCE reached => increasing CE level to CE1");
              if (m_mac_logging) LogMessage("LteUeMac::RaResponseTimeoutNb,RAR timeout, MaxNumPreambleAttemptCE reached => increasing CE level to CE1");
              m_CeLevel = m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb1;
              m_rachConfigCe = m_radioResourceConfig.rachConfigCommon.rachInfoList.rachInfo2;
              if(m_edt){
                tmp = m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb1;
              }
            }
          else if (m_CeLevel.coverageEnhancementLevel == m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb1.coverageEnhancementLevel) // CE1
            {
              // Increase to CE2
              NS_LOG_INFO ("RAR timeout, MaxNumPreambleAttemptCE reached => increasing CE level to CE2");
              if (m_mac_logging) LogMessage("LteUeMac::RaResponseTimeoutNb,RAR timeout, MaxNumPreambleAttemptCE reached => increasing CE level to CE2");
              m_CeLevel = m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb2;
              m_rachConfigCe = m_radioResourceConfig.rachConfigCommon.rachInfoList.rachInfo3;
              if(m_edt){
                tmp = m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb2;
              }
            }
          else if (m_CeLevel.coverageEnhancementLevel == m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb2.coverageEnhancementLevel) // CE2
            {
              // Can't increase further
              NS_LOG_INFO ("RAR timeout, MaxNumPreambleAttemptCE in CE2 reached => giving up");
              if (m_mac_logging) LogMessage("LteUeMac::RaResponseTimeoutNb,RAR timeout, MaxNumPreambleAttemptCE in CE2 reached => giving up");
              m_cmacSapUser->NotifyRandomAccessFailed ();
              return;
            }

          if(m_edt){
            // Overwrite R13 config with values for R15 EDT provided
            // easiest way to access data not include in NprachParameterNBR14
            m_CeLevel.coverageEnhancementLevel= tmp.coverageEnhancementLevel;
            m_CeLevel.nprachPeriodicity = tmp.nprachPeriodicity;
            m_CeLevel.nprachStartTime = tmp.nprachStartTime;
            m_CeLevel.nprachSubcarrierOffset = tmp.nprachSubcarrierOffset;
            m_CeLevel.nprachNumSubcarriers = tmp.nprachNumSubcarriers;
            m_CeLevel.nprachSubcarrierMsg3RangeStart= tmp.nprachSubcarrierMsg3RangeStart;
            m_CeLevel.npdcchNumRepetitionsRA = tmp.npdcchNumRepetitionsRA;
            m_CeLevel.npdcchStartSfCssRa = tmp.npdcchStartSfCssRa;
            m_CeLevel.npdcchOffsetRa = tmp.npdcchOffsetRa;

          }
        }
      NS_LOG_INFO ("RAR timeout, re-send preamble");
      if (m_mac_logging) LogMessage("LteUeMac::RaResponseTimeoutNb,RAR timeout, re-send preamble");
      // TS 36.321 5.1.4: back off by a uniform random delay in [0, BACKOFF) before
      // the next preamble, where BACKOFF comes from the last received BI. Spreads
      // a thundering herd across many NPRACH occasions.
      uint32_t backoffMs = (m_backoffParameter > 0)
                               ? m_raPreambleUniformVariable->GetInteger (0, m_backoffParameter)
                               : 0;
      if (contention)
        {
          if (backoffMs > 0)
            Simulator::Schedule (MilliSeconds (backoffMs),
                                 &LteUeMac::RandomlySelectAndSendRaPreambleNb, this);
          else
            RandomlySelectAndSendRaPreambleNb ();
        }
      else
        {
          SendRaPreambleNb (contention);
        }
    }
}

void
LteUeMac::DoConfigureRach (LteUeCmacSapProvider::RachConfig rc)
{
  NS_LOG_FUNCTION (this);
  m_rachConfig = rc;
  m_rachConfigured = true;
}
void
LteUeMac::DoConfigureRadioResourceConfig (NbIotRrcSap::RadioResourceConfigCommonNb rc)
{
  NS_LOG_FUNCTION (this);
  m_radioResourceConfig = rc;
  m_nprachConfigured = true;
}
void
LteUeMac::DoStartContentionBasedRandomAccessProcedure ()
{
  NS_LOG_FUNCTION (this);

  // 3GPP 36.321 5.1.1
  NS_ASSERT_MSG (m_rachConfigured, "RACH not configured");
  m_preambleTransmissionCounter = 0;
  m_backoffParameter = 0;
  RandomlySelectAndSendRaPreamble ();
}
void
LteUeMac::DoStartRandomAccessProcedureNb (bool edt)
{
  NS_LOG_FUNCTION (this);

  // 3GPP 36.321 5.1.1
  NS_ASSERT_MSG (m_nprachConfigured, "RACH not configured");
  m_preambleTransmissionCounter = 0;
  m_preambleTransmissionCounterCe = 0;
  m_edt = edt;
  // Check CE Level
  double rsrp = m_uePhySapProvider->GetRSRP ();
  //NS_BUILD_DEBUG (std::cout << "RSRP: " << rsrp << "dBm" << std::endl);

  NbIotRrcSap::NprachParametersNbR14 tmp = {}; // needed if EDT is enabled

  if (rsrp <= m_radioResourceConfig.nprachConfig.rsrpThresholdsPrachInfoList.ce2_lowerbound)
    {
      // CE2
      m_CeLevel = m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb2;
      m_rachConfigCe = m_radioResourceConfig.rachConfigCommon.rachInfoList.rachInfo3;
      if(m_edt){
        tmp = m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb2;
      }
    }
  else if (rsrp <= m_radioResourceConfig.nprachConfig.rsrpThresholdsPrachInfoList.ce1_lowerbound)
    {
      // CE1
      m_CeLevel = m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb1;
      m_rachConfigCe = m_radioResourceConfig.rachConfigCommon.rachInfoList.rachInfo2;
      if(m_edt){
        tmp = m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb1;
      }
    }
  else if (rsrp > m_radioResourceConfig.nprachConfig.rsrpThresholdsPrachInfoList.ce1_lowerbound)
    {
      // CE0
      m_CeLevel = m_radioResourceConfig.nprachConfig.nprachParametersList.nprachParametersNb0;
      m_rachConfigCe = m_radioResourceConfig.rachConfigCommon.rachInfoList.rachInfo1;
      if(m_edt){
        tmp = m_radioResourceConfig.nprachConfigR15.nprachParameterListEdt.nprachParametersNb0;
      }
    }

  if(m_edt){
    // Overwrite R13 config with values for R15 EDT provided
    // easiest way to access data not include in NprachParameterNBR14
    m_CeLevel.coverageEnhancementLevel= tmp.coverageEnhancementLevel;
    m_CeLevel.nprachPeriodicity = tmp.nprachPeriodicity;
    m_CeLevel.nprachStartTime = tmp.nprachStartTime;
    m_CeLevel.nprachSubcarrierOffset = tmp.nprachSubcarrierOffset;
    m_CeLevel.nprachNumSubcarriers = tmp.nprachNumSubcarriers;
    m_CeLevel.nprachSubcarrierMsg3RangeStart= tmp.nprachSubcarrierMsg3RangeStart;
    m_CeLevel.npdcchNumRepetitionsRA = tmp.npdcchNumRepetitionsRA;
    m_CeLevel.npdcchStartSfCssRa = tmp.npdcchStartSfCssRa;
    m_CeLevel.npdcchOffsetRa = tmp.npdcchOffsetRa;

  }
  m_backoffParameter = 0;

  if (m_mac_logging)
  {
    LogMessage("StartRandomAccessProcedureNb");
  }

  // Backoff carried over from a contention-resolution failure (TS 36.321 5.1.5):
  // delay the first preamble of this re-RACH by a uniform draw in [0, backoff)
  // so the colliders that lost the previous round spread out instead of
  // re-colliding immediately. Cleared after use (one-shot per failure).
  if (m_pendingReRachBackoffMs > 0)
    {
      uint32_t delayMs = m_raPreambleUniformVariable->GetInteger (0, m_pendingReRachBackoffMs);
      m_pendingReRachBackoffMs = 0;
      if (delayMs > 0)
        {
          Simulator::Schedule (MilliSeconds (delayMs),
                               &LteUeMac::RandomlySelectAndSendRaPreambleNb, this);
          return;
        }
    }

  RandomlySelectAndSendRaPreambleNb ();
}

void
LteUeMac::NotifyContentionResolutionFailedNb ()
{
  NS_LOG_FUNCTION (this);
  // Carry the backoff parameter learned this attempt (from the RAR Backoff
  // Indicator) into the next RA. It survives the MAC Reset that the RRC performs
  // on T300 expiry, so the re-RACH applies it (see DoStartRandomAccessProcedureNb).
  m_pendingReRachBackoffMs = m_backoffParameter;
  if (m_mac_logging)
  {
    LogMessage ("NotifyContentionResolutionFailedNb,backoffMs=" + std::to_string (m_backoffParameter));
  }
}
void
LteUeMac::DoSetRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
  m_rnti = rnti;
}

void
LteUeMac::DoSetImsi (uint64_t imsi)
{
  NS_LOG_FUNCTION (this);
  m_imsi = imsi;
}

void
LteUeMac::DoStartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t preambleId,
                                                          uint8_t prachMask)
{
  NS_LOG_FUNCTION (this << rnti << (uint16_t) preambleId << (uint16_t) prachMask);
  NS_ASSERT_MSG (prachMask == 0,
                 "requested PRACH MASK = " << (uint32_t) prachMask
                                           << ", but only PRACH MASK = 0 is supported");
  m_rnti = rnti;
  m_raPreambleId = preambleId;
  m_preambleTransmissionCounter = 0;
  m_preambleTransmissionCounterCe = 0;
  bool contention = false;
  SendRaPreamble (contention);
}

void
LteUeMac::DoAddLc (uint8_t lcId, LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
                   LteMacSapUser *msu)
{
  NS_LOG_FUNCTION (this << " lcId" << (uint32_t) lcId);
  NS_ASSERT_MSG (m_lcInfoMap.find (lcId) == m_lcInfoMap.end (),
                 "cannot add channel because LCID " << (uint16_t) lcId << " is already present");

  LcInfo lcInfo;
  lcInfo.lcConfig = lcConfig;
  lcInfo.macSapUser = msu;
  m_lcInfoMap[lcId] = lcInfo;
}

void
LteUeMac::DoRemoveLc (uint8_t lcId)
{
  NS_LOG_FUNCTION (this << " lcId" << lcId);
  NS_ASSERT_MSG (m_lcInfoMap.find (lcId) != m_lcInfoMap.end (), "could not find LCID " << lcId);
  m_lcInfoMap.erase (lcId);
}

void
LteUeMac::DoReset ()
{
  NS_LOG_FUNCTION (this);
  std::map<uint8_t, LcInfo>::iterator it = m_lcInfoMap.begin ();
  while (it != m_lcInfoMap.end ())
    {
      // don't delete CCCH)
      if (it->first == 0)
        {
          ++it;
        }
      else
        {
          // note: use of postfix operator preserves validity of iterator
          m_lcInfoMap.erase (it++);
        }
    }
  // note: rnti will be assigned by the eNB using RA response message
  m_rnti = 0;
  m_noRaResponseReceivedEvent.Cancel ();
  m_rachConfigured = false;
  m_freshUlBsr = false;
  m_ulBsrReceived.clear ();
}

void
LteUeMac::DoNotifyConnectionSuccessful ()
{
  NS_LOG_FUNCTION (this);
  m_uePhySapProvider->NotifyConnectionSuccessful ();
  m_listenToSearchSpaces = true;
  m_suspended = false;   // resumed/connected: the MAC SR machine is live again
  m_drbDataSentThisSession = false;  // new session: RAI must wait until this session's DRB data is sent
  // A fresh resume/connection starts NO active grant session. m_grantSessionActive
  // latches true (it is only cleared by a total==0 BSR, which is unreliable), so a
  // stale value here would block the SR-schedule branch in DoReportBufferStatus --
  // leaving a resumed UE with data but no way to request a grant if the resume-path
  // grant misses. Clearing it makes the dedicated SR a reliable first-grant backstop.
  m_grantSessionActive = false;
  // RRC dedicated-SR configuration (TS 36.331 SchedulingRequestConfig-NB): once
  // connected with a real C-RNTI, register this UE's dedicated SR index at the
  // eNB so it can resolve the UE's reserved-subcarrier preambles by identity.
  if (m_srPreamble && !m_srConfigRegistered && m_rnti != 0 && !m_srConfigCb.IsNull ())
    {
      m_srConfigCb (m_rnti, m_srIndex);
      m_srConfigRegistered = true;
    }
}

void
LteUeMac::DoReceivePhyPdu (Ptr<Packet> p)
{
  LteRadioBearerTag tag;

  uint32_t frameSize = p->GetSize ();
  uint32_t serializedSize = p->GetSerializedSize();
  p->RemovePacketTag (tag);
  if (NbIotDebugTrace ())
    std::cout << "[UE-PHY-PDU] t=" << Simulator::Now ().GetSeconds ()
              << " ueRnti=" << m_rnti << " tagRnti=" << tag.GetRnti ()
              << " lcid=" << (uint32_t) tag.GetLcid ()
              << " lcKnown=" << (m_lcInfoMap.find (tag.GetLcid ()) != m_lcInfoMap.end ()) << std::endl;
  if (tag.GetRnti () == m_rnti)
  {
      NS_LOG_INFO("LteUeMac::DoReceivePhyPdu RNTI: " << m_rnti << ", Id: " << p->GetUid () << ", Size: " << frameSize << " bytes, " << serializedSize << " bytes");
      // packet is for the current user
      std::map<uint8_t, LcInfo>::const_iterator it = m_lcInfoMap.find (tag.GetLcid ());
      if (it != m_lcInfoMap.end ())
        {
          LteMacSapUser::ReceivePduParameters rxPduParams;
          rxPduParams.p = p;
          rxPduParams.rnti = m_rnti;
          rxPduParams.lcid = tag.GetLcid ();
          it->second.macSapUser->ReceivePdu (rxPduParams);
          // NB-IoT Specific: Send HARQ at advertised Subframe
          if (m_nextPossibleHarqOpportunity.size () > 0)
            {
              uint32_t currentsubframe = 10 * (m_frameNo - 1) + (m_subframeNo - 1);
              uint32_t subframestillHarqF2 = m_nextPossibleHarqOpportunity[0].second.front()- currentsubframe;
              uint32_t subframestowait = m_nextPossibleHarqOpportunity[0].second.back() - currentsubframe;
              //NS_BUILD_DEBUG (std::cout << "Sending HARQ Response at " << currentsubframe + subframestowait << std::endl);

              Simulator::Schedule (MilliSeconds (subframestowait),
                                   &LteUePhySapProvider::SendHarqAckResponse, m_uePhySapProvider,
                                   true);
              Simulator::Schedule(MilliSeconds(subframestillHarqF2),&LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPUSCH_F2);
              Simulator::Schedule(MilliSeconds(subframestowait+1),&LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);
              m_nextPossibleHarqOpportunity.clear();
              //NS_BUILD_DEBUG (std::cout << m_rnti << " Got to MSG4-HARQ \n");
            }
        }
      else
        {
          NS_LOG_WARN ("received packet with unknown lcid " << (uint32_t) tag.GetLcid ());
        }
    }
}
void
LteUeMac::DoSetTransmissionScheduled(bool scheduled){
  m_transmissionScheduled = scheduled;
}

void
LteUeMac::DoReceiveLteControlMessage (Ptr<LteControlMessage> msg)
{
  NS_LOG_FUNCTION (this);
  if (msg->GetMessageType () == LteControlMessage::UL_DCI)
    {
      Ptr<UlDciLteControlMessage> msg2 = DynamicCast<UlDciLteControlMessage> (msg);
      UlDciListElement_s dci = msg2->GetDci ();
      if (dci.m_ndi == 1)
        {
          // New transmission -> empty pkt buffer queue (for deleting eventual pkts not acked )
          Ptr<PacketBurst> pb = CreateObject<PacketBurst> ();
          m_miUlHarqProcessesPacket.at (m_harqProcessId) = pb;
          // Retrieve data from RLC
          std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator itBsr;
          uint16_t activeLcs = 0;
          uint32_t statusPduMinSize = 0;
          for (itBsr = m_ulBsrReceived.begin (); itBsr != m_ulBsrReceived.end (); itBsr++)
            {
              if (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) ||
                  ((*itBsr).second.txQueueSize > 0))
                {
                  activeLcs++;
                  if (((*itBsr).second.statusPduSize != 0) &&
                      ((*itBsr).second.statusPduSize < statusPduMinSize))
                    {
                      statusPduMinSize = (*itBsr).second.statusPduSize;
                    }
                  if (((*itBsr).second.statusPduSize != 0) && (statusPduMinSize == 0))
                    {
                      statusPduMinSize = (*itBsr).second.statusPduSize;
                    }
                }
            }
          if (activeLcs == 0)
            {
              NS_LOG_ERROR (this << " No active flows for this UL-DCI");
              return;
            }
          std::map<uint8_t, LcInfo>::iterator it;
          uint32_t bytesPerActiveLc = dci.m_tbSize / activeLcs;
          bool statusPduPriority = false;
          if ((statusPduMinSize != 0) && (bytesPerActiveLc < statusPduMinSize))
            {
              // send only the status PDU which has highest priority
              statusPduPriority = true;
              NS_LOG_DEBUG (this << " Reduced resource -> send only Status, b ytes "
                                 << statusPduMinSize);
              if (dci.m_tbSize < statusPduMinSize)
                {
                  NS_FATAL_ERROR ("Insufficient Tx Opportunity for sending a status message");
                }
            }
          NS_LOG_LOGIC (this << " UE " << m_rnti << ": UL-CQI notified TxOpportunity of "
                             << dci.m_tbSize << " => " << bytesPerActiveLc << " bytes per active LC"
                             << " statusPduMinSize " << statusPduMinSize);

          LteMacSapUser::TxOpportunityParameters txOpParams;

          for (it = m_lcInfoMap.begin (); it != m_lcInfoMap.end (); it++)
            {
              itBsr = m_ulBsrReceived.find ((*it).first);
              NS_LOG_DEBUG (this << " Processing LC " << (uint32_t) (*it).first
                                 << " bytesPerActiveLc " << bytesPerActiveLc);
              if ((itBsr != m_ulBsrReceived.end ()) &&
                  (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) ||
                   ((*itBsr).second.txQueueSize > 0)))
                {
                  if ((statusPduPriority) && ((*itBsr).second.statusPduSize == statusPduMinSize))
                    {
                      txOpParams.bytes = (*itBsr).second.statusPduSize;
                      txOpParams.layer = 0;
                      txOpParams.harqId = 0;
                      txOpParams.componentCarrierId = m_componentCarrierId;
                      txOpParams.rnti = m_rnti;
                      txOpParams.lcid = (*it).first;
                      (*it).second.macSapUser->NotifyTxOpportunity (txOpParams);
                      NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " send  "
                                         << (*itBsr).second.statusPduSize << " status bytes to LC "
                                         << (uint32_t) (*it).first << " statusQueue "
                                         << (*itBsr).second.statusPduSize << " retxQueue"
                                         << (*itBsr).second.retxQueueSize << " txQueue"
                                         << (*itBsr).second.txQueueSize);
                      (*itBsr).second.statusPduSize = 0;
                      break;
                    }
                  else
                    {
                      uint32_t bytesForThisLc = bytesPerActiveLc;
                      NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " bytes to LC "
                                         << (uint32_t) (*it).first << " statusQueue "
                                         << (*itBsr).second.statusPduSize << " retxQueue"
                                         << (*itBsr).second.retxQueueSize << " txQueue"
                                         << (*itBsr).second.txQueueSize);
                      if (((*itBsr).second.statusPduSize > 0) &&
                          (bytesForThisLc > (*itBsr).second.statusPduSize))
                        {
                          txOpParams.bytes = (*itBsr).second.statusPduSize;
                          txOpParams.layer = 0;
                          txOpParams.harqId = 0;
                          txOpParams.componentCarrierId = m_componentCarrierId;
                          txOpParams.rnti = m_rnti;
                          txOpParams.lcid = (*it).first;
                          (*it).second.macSapUser->NotifyTxOpportunity (txOpParams);
                          bytesForThisLc -= (*itBsr).second.statusPduSize;
                          NS_LOG_DEBUG (this << " serve STATUS " << (*itBsr).second.statusPduSize);
                          (*itBsr).second.statusPduSize = 0;
                        }
                      else
                        {
                          if ((*itBsr).second.statusPduSize > bytesForThisLc)
                            {
                              NS_FATAL_ERROR (
                                  "Insufficient Tx Opportunity for sending a status message");
                            }
                        }

                      if ((bytesForThisLc > 7) // 7 is the min TxOpportunity useful for Rlc
                          && (((*itBsr).second.retxQueueSize > 0) ||
                              ((*itBsr).second.txQueueSize > 0)))
                        {
                          if ((*itBsr).second.retxQueueSize > 0)
                            {
                              NS_LOG_DEBUG (this << " serve retx DATA, bytes " << bytesForThisLc);
                              txOpParams.bytes = bytesForThisLc;
                              txOpParams.layer = 0;
                              txOpParams.harqId = 0;
                              txOpParams.componentCarrierId = m_componentCarrierId;
                              txOpParams.rnti = m_rnti;
                              txOpParams.lcid = (*it).first;
                              (*it).second.macSapUser->NotifyTxOpportunity (txOpParams);
                              if ((*itBsr).second.retxQueueSize >= bytesForThisLc)
                                {
                                  (*itBsr).second.retxQueueSize -= bytesForThisLc;
                                }
                              else
                                {
                                  (*itBsr).second.retxQueueSize = 0;
                                }
                            }
                          else if ((*itBsr).second.txQueueSize > 0)
                            {
                              uint16_t lcid = (*it).first;
                              uint32_t rlcOverhead;
                              if (lcid == 1)
                                {
                                  // for SRB1 (using RLC AM) it's better to
                                  // overestimate RLC overhead rather than
                                  // underestimate it and risk unneeded
                                  // segmentation which increases delay
                                  rlcOverhead = 4;
                                }
                              else
                                {
                                  // minimum RLC overhead due to header
                                  rlcOverhead = 2;
                                }
                              NS_LOG_DEBUG (this << " serve tx DATA, bytes " << bytesForThisLc
                                                 << ", RLC overhead " << rlcOverhead);
                              txOpParams.bytes = bytesForThisLc;
                              txOpParams.layer = 0;
                              txOpParams.harqId = 0;
                              txOpParams.componentCarrierId = m_componentCarrierId;
                              txOpParams.rnti = m_rnti;
                              txOpParams.lcid = (*it).first;
                              (*it).second.macSapUser->NotifyTxOpportunity (txOpParams);
                              if ((*itBsr).second.txQueueSize >= bytesForThisLc - rlcOverhead)
                                {
                                  (*itBsr).second.txQueueSize -= bytesForThisLc - rlcOverhead;
                                }
                              else
                                {
                                  (*itBsr).second.txQueueSize = 0;
                                }
                            }
                        }
                      else
                        {
                          if (((*itBsr).second.retxQueueSize > 0) ||
                              ((*itBsr).second.txQueueSize > 0))
                            {
                              // resend BSR info for updating eNB peer MAC
                              m_freshUlBsr = true;
                            }
                        }
                      NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << "\t new queues "
                                         << (uint32_t) (*it).first << " statusQueue "
                                         << (*itBsr).second.statusPduSize << " retxQueue"
                                         << (*itBsr).second.retxQueueSize << " txQueue"
                                         << (*itBsr).second.txQueueSize);
                    }
                }
            }
        }
      else
        {
          // HARQ retransmission -> retrieve data from HARQ buffer
          NS_LOG_DEBUG (this << " UE MAC RETX HARQ " << (uint16_t) m_harqProcessId);
          Ptr<PacketBurst> pb = m_miUlHarqProcessesPacket.at (m_harqProcessId);
          for (std::list<Ptr<Packet>>::const_iterator j = pb->Begin (); j != pb->End (); ++j)
            {
              Ptr<Packet> pkt = (*j)->Copy ();
              m_uePhySapProvider->SendMacPdu (pkt);
            }
          m_miUlHarqProcessesPacketTimer.at (m_harqProcessId) = HARQ_PERIOD;
        }
    }
  else if (msg->GetMessageType () == LteControlMessage::RAR)
    {
      if (m_waitingForRaResponse)
        {
          Ptr<RarLteControlMessage> rarMsg = DynamicCast<RarLteControlMessage> (msg);
          uint16_t raRnti = rarMsg->GetRaRnti ();
          NS_LOG_LOGIC (this << "got RAR with RA-RNTI " << (uint32_t) raRnti << ", expecting "
                             << (uint32_t) m_raRnti);
          if (raRnti == m_raRnti) // RAR corresponds to TX subframe of preamble
            {
              for (std::list<RarLteControlMessage::Rar>::const_iterator it =
                       rarMsg->RarListBegin ();
                   it != rarMsg->RarListEnd (); ++it)
                {
                  if (it->rapId == m_raPreambleId) // RAR is for me
                    {
                      RecvRaResponse (it->rarPayload);
                      /// \todo RRC generates the RecvRaResponse messaged
                      /// for avoiding holes in transmission at PHY layer
                      /// (which produce erroneous UL CQI evaluation)
                    }
                }
            }
        }
    }
  else if (msg->GetMessageType () == LteControlMessage::RAR_NB)
    {
      if (m_waitingForRaResponse)
        {
          Ptr<RarNbiotControlMessage> rarMsg = DynamicCast<RarNbiotControlMessage> (msg);
          uint16_t raRnti = rarMsg->GetRaRnti ();
          NS_LOG_LOGIC (this << "got RAR with RA-RNTI " << (uint32_t) raRnti << ", expecting "
                             << (uint32_t) m_raRnti);
          if (raRnti == m_raRnti) // RAR corresponds to TX subframe of preamble
            {
              // Backoff Indicator (TS 36.321 5.1.4): read from the RAR MAC PDU
              // even if our RAPID isn't matched (a colliding UE still backs off).
              m_backoffParameter = NbiotBackoffMs (rarMsg->GetBackoffIndicator ());
              for (std::list<NbIotRrcSap::Rar>::const_iterator it = rarMsg->RarListBegin ();
                   it != rarMsg->RarListEnd (); ++it)
                {
                  if (it->rapId == NbIotRrcSap::ConvertNprachSubcarrierOffset2int (m_CeLevel) +
                                       m_raPreambleId) // RAR is for me
                    {
                      RecvRaResponseNb (it->rarPayload);
                      /// \todo RRC generates the RecvRaResponse messaged
                      /// for avoiding holes in transmission at PHY layer
                      /// (which produce erroneous UL CQI evaluation)
                    }
                }
            }
        }
    }
  else if (msg->GetMessageType () == LteControlMessage::DL_DCI_NB){
      Ptr<DlDciN1NbiotControlMessage> msg2 = DynamicCast<DlDciN1NbiotControlMessage> (msg);
      NbIotRrcSap::DciN1 dci = msg2->GetDci ();
      if (NbIotDebugTrace ())
        std::cout << "[UE-DL-DCI] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti << std::endl;
      //Handle Energy State Dci Reception
      m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);

      uint32_t subframesTillNpdschBegin = dci.npdschOpportunity.front() - (10*(m_frameNo-1)+m_subframeNo-1);
      uint32_t subframesTillNpdschEnd = dci.npdschOpportunity.back() - (10 * (m_frameNo - 1) + m_subframeNo - 1);
      m_transmissionScheduled = true;
      Simulator::Schedule(MilliSeconds(subframesTillNpdschBegin), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_RECEIVING_NPDSCH);
      Simulator::Schedule(MilliSeconds(subframesTillNpdschEnd+1), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);

      // Liberg p. 286 "After the device completes its NPUSCH Format 2 transmission, it is not required to monitor NPDCCH search space for 3 ms."
      if (m_nextPossibleHarqOpportunity.size() > 0){
        uint32_t subframesTransmissionEnd = m_nextPossibleHarqOpportunity[0].second.back() - (10*(m_frameNo-1)+m_subframeNo-1);
        Simulator::Schedule(MilliSeconds(subframesTransmissionEnd+3),&LteUeMac::DoSetTransmissionScheduled, this,false); // Transmission done, ready to listen to new NPDCCH
      }else{
        Simulator::Schedule(MilliSeconds(subframesTillNpdschEnd+3), &LteUeMac::DoSetTransmissionScheduled, this, false);
      }


  }
  else if (msg->GetMessageType () == LteControlMessage::UL_DCI_NB)
    {
      Ptr<UlDciN0NbiotControlMessage> msg2 = DynamicCast<UlDciN0NbiotControlMessage> (msg);
      NbIotRrcSap::DciN0 dci = msg2->GetDci ();

      // De-duplicate NPDCCH DCI repetitions. NB-IoT transmits a grant over multiple
      // repetition subframes (coverage), and this model delivers EACH repetition as a
      // separate UL_DCI_NB message. A grant is ONE logical scheduling event -- the UE
      // soft-combines the repetitions and acts once. Fingerprint by the granted NPUSCH
      // start subframe (repetitions share it; distinct grants do not). Without this the
      // UE transmits the same TB N times -> the eNB receives N duplicate RLC PDUs ->
      // RLC-AM reassembly corruption (PDCP D/C-bit assert) and wasted airtime/loss.
      uint64_t npuschStart = dci.npuschOpportunity[0].second.front ();
      if (npuschStart == m_lastUlGrantNpuschSf)
        {
          return; // repeated DCI for a grant already acted on
        }
      m_lastUlGrantNpuschSf = npuschStart;

      // Msg4 (TS 36.321 5.1.5): this DCI is addressed to the UE's (real) C-RNTI.
      // If a C-RNTI MAC CE was sent on Msg3, receiving it completes contention
      // resolution -- the UE keeps its C-RNTI and discards the Temporary C-RNTI.
      if (m_awaitingContentionResolution)
        {
          NS_LOG_INFO ("Msg4 received: contention resolution SUCCESSFUL, kept C-RNTI=" << m_rnti
                       << " (discarded Temp C-RNTI=" << m_crniTempForCr << ")");
          m_awaitingContentionResolution = false;
          m_crniTempForCr = 0;
          m_contentionResolutionTimer.Cancel ();
        }

      // TS 36.321 5.4.4: a pending SR is cancelled, and sr-ProhibitTimer is
      // stopped, once the UE is granted UL-SCH resources (and will send its
      // BSR/data). Cancel the pending SR + re-send, and clear the prohibit
      // window so genuinely new data can request again immediately.
      m_srPending = false;
      m_srContentionRa = false;          // faithful contention RA (if any) is resolved
      m_srContentionFailures = 0;        // packet served: reset the RA-cost inflation for the next one
      m_edtMsg3DataSent = false;         // EDT Msg3 (if any) won: its data was delivered
      RestoreSrCeLevelAfterEdt ();       // back to the legacy partition for the SR machine
      // A grant addressed to our C-RNTI IS the contention resolution (TS 36.321
      // 5.1.5): stop the contention-resolution timer and clear the awaiting state so
      // the buffered data is sent on this grant rather than the UE retrying/timing out.
      m_awaitingContentionResolution = false;
      m_contentionResolutionTimer.Cancel ();
      m_srEvent.Cancel ();
      m_srContentionEvent.Cancel ();
      m_srProhibitUntil = Simulator::Now ();
      // First grant after the SR opens the grant session: from here the real
      // buffer flows via the BSR MAC CE (on this granted NPUSCH and the
      // periodic-BSR path), not via further SRs. Stays active until the buffer
      // drains (see DoReportBufferStatus).
      m_grantSessionActive = true;
      // If this UE is SR-resuming from PSM (it announced via its preamble and the RRC is
      // held in a resuming state), this grant is the eNB's confirmation that it resynced
      // -> tell the RRC to flip to CONNECTED now. No-op for a normal connected-mode grant.
      m_cmacSapUser->NotifySrResumeConnected ();
      // drx-InactivityTimer (TS 36.321 5.7): a grant extends Active Time, so the
      // cDRX gate keeps monitoring through the rest of this session.
      m_cdrxInactivityUntil = Simulator::Now () + MilliSeconds (m_cdrxInactivityMs);

      m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);

      uint32_t subframesTillNpusch = dci.npuschOpportunity[0].second.front() - (10*(m_frameNo-1)+m_subframeNo-1);
      uint32_t subframes = *(dci.npuschOpportunity[0].second.end () - 1) -
          (10 * (m_frameNo - 1) + m_subframeNo - 1);

      m_transmissionScheduled = true;
      Simulator::Schedule(MilliSeconds(subframesTillNpusch), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_SENDING_NPUSCH);
      Simulator::Schedule(MilliSeconds(subframes+1), &LteUeCmacSapUser::NotifyEnergyState, m_cmacSapUser, NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);


      // Liberg p. 283 "After the device completes its NPUSCH transmission, there is at least a 3-ms gap to allow the device to switch from transmission mode to reception mode and be ready for monitoring the next NPDCCH search space candidate."
      uint32_t subframesTransmissionEnd = dci.npuschOpportunity[0].second.back() - (10*(m_frameNo-1)+m_subframeNo-1);
      Simulator::Schedule(MilliSeconds(subframesTransmissionEnd+3),&LteUeMac::DoSetTransmissionScheduled, this,false); // Transmission done, ready to listen to new NPDCCH

      if (dci.NDI)
        {
          // New transmission -> empty pkt buffer queue (for deleting eventual pkts not acked )
          Ptr<PacketBurst> pb = CreateObject<PacketBurst> ();
          m_miUlHarqProcessesPacket.at (m_harqProcessId) = pb;
          // Retrieve data from RLC
          std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator itBsr;
          std::vector<uint8_t> activeLcs;
          for (itBsr = m_ulBsrReceived.begin (); itBsr != m_ulBsrReceived.end (); itBsr++)
          {
            if (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) ||
                ((*itBsr).second.txQueueSize > 0))
              {
                if(m_nextIsMsg5){
                  // We might get a bigger TxOp as the size of the MSG5, so we don't want user data transmitted there
                  if(itBsr->first >2){
                    continue;
                  }
                }
                activeLcs.push_back(itBsr->first);
              }
          }
          if(m_nextIsMsg5){
            m_nextIsMsg5 = false;
          }
          LteMacSapUser::TxOpportunityParameters txOpParams;
          // Prioritise SRBs over DataBs
          uint64_t bytesforallLc = dci.tbs/8;
          for(std::vector<uint8_t>::iterator lcit = activeLcs.begin(); lcit != activeLcs.end(); ++lcit){
            std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator bsr = m_ulBsrReceived.find((*lcit));
            std::map<uint8_t, LcInfo>::iterator lcidIt = m_lcInfoMap.find (bsr->second.lcid);
            if ((bsr->second.statusPduSize > 0) &&
                    (bytesforallLc > bsr->second.statusPduSize))
              {
                txOpParams.bytes = bsr->second.statusPduSize;
                txOpParams.layer = 0;
                txOpParams.harqId = 0;
                txOpParams.componentCarrierId = m_componentCarrierId;
                txOpParams.rnti = bsr->second.rnti;
                txOpParams.lcid = bsr->second.lcid;
                //Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
                //    (*lcidIt).second.macSapUser, txOpParams);
                (*lcidIt).second.macSapUser->NotifyTxOpportunityNb(txOpParams,subframes);
                bytesforallLc -= bsr->second.statusPduSize;
                bsr->second.statusPduSize = 0;
              }
            else
              {
                if (bsr->second.statusPduSize > bytesforallLc)
                  {
                    //NS_FATAL_ERROR (
                    //    "Insufficient Tx Opportunity for sending a status message");
                  }
              }

            if ((bytesforallLc> 7) // 7 is the min TxOpportunity useful for Rlc
                && ((bsr->second.retxQueueSize > 0) ||
                    (bsr->second.txQueueSize > 0)))
              {
                if (bsr->second.retxQueueSize > 0)
                  {
                    NS_LOG_DEBUG (this << " serve retx DATA, bytes " << bytesforallLc);
                    if(bsr->second.retxQueueSize > bytesforallLc){
                      txOpParams.bytes = bytesforallLc;
                      bsr->second.retxQueueSize -= bytesforallLc;
                      bytesforallLc = 0;
                    }else{
                      if(bsr->second.retxQueueSize +4 < 7){
                        txOpParams.bytes = 7;
                        bytesforallLc -= 7;
                      }else{
                        txOpParams.bytes = bsr->second.retxQueueSize+4;
                        bytesforallLc -= bsr->second.retxQueueSize+4;
                      }
                        bsr->second.retxQueueSize = 0;
                    }
                    txOpParams.layer = 0;
                    txOpParams.harqId = 0;
                    txOpParams.componentCarrierId = m_componentCarrierId;
                    txOpParams.rnti = bsr->second.rnti;
                    txOpParams.lcid = bsr->second.lcid;
                    //Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
                    //  (*lcidIt).second.macSapUser, txOpParams);
                    (*lcidIt).second.macSapUser->NotifyTxOpportunityNb(txOpParams,subframes);
                  }
                else if (bsr->second.txQueueSize > 0)
                  {
                    uint16_t lcid = bsr->second.lcid;
                    uint32_t rlcOverhead;
                    if (lcid == 1 || lcid == 3)
                      {
                        // for SRB1 (using RLC AM) it's better to
                        // overestimate RLC overhead rather than
                        // underestimate it and risk unneeded
                        // segmentation which increases delay
                        rlcOverhead = 4;
                      }
                    else
                      {
                        // minimum RLC overhead due to header
                        rlcOverhead = 2;
                      }
                    NS_LOG_DEBUG (this << " serve tx DATA, bytes " << bytesforallLc
                                        << ", RLC overhead " << rlcOverhead);
                    if(bsr->second.txQueueSize > bytesforallLc){
                      txOpParams.bytes = bytesforallLc;
                      bsr->second.txQueueSize -= bytesforallLc-rlcOverhead;
                      bytesforallLc = 0;
                    }else{
                      if(bsr->second.txQueueSize +4 < 7){
                        txOpParams.bytes = 7;
                        bytesforallLc -= 7;
                      }else{
                        txOpParams.bytes = bsr->second.txQueueSize+4;
                        bytesforallLc -= bsr->second.txQueueSize+4;
                      }

                      bsr->second.txQueueSize = 0;
                    }
                    txOpParams.layer = 0;
                    txOpParams.harqId = 0;
                    txOpParams.componentCarrierId = m_componentCarrierId;
                    txOpParams.rnti = bsr->second.rnti;
                    txOpParams.lcid = bsr->second.lcid;

                    //Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
                    //  (*lcidIt).second.macSapUser, txOpParams);
                    (*lcidIt).second.macSapUser->NotifyTxOpportunityNb(txOpParams,subframes);

                  }
                  }
                else
                  {
                    if ((bsr->second.retxQueueSize > 0) ||
                        (bsr->second.txQueueSize > 0))
                      {
                        //NS_BUILD_DEBUG(std::cout << "Not enough space" << std::endl);
                      }
                  }
          }
          //uint16_t activeLcs = 0;
          //uint32_t statusPduMinSize = 0;
          //for (itBsr = m_ulBsrReceived.begin (); itBsr != m_ulBsrReceived.end (); itBsr++)
          //  {
          //    if (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) ||
          //        ((*itBsr).second.txQueueSize > 0))
          //      {
          //        activeLcs++;
          //        if (((*itBsr).second.statusPduSize != 0) &&
          //            ((*itBsr).second.statusPduSize < statusPduMinSize))
          //          {
          //            statusPduMinSize = (*itBsr).second.statusPduSize;
          //          }
          //        if (((*itBsr).second.statusPduSize != 0) && (statusPduMinSize == 0))
          //          {
          //            statusPduMinSize = (*itBsr).second.statusPduSize;
          //          }
          //      }
          //  }
          //if (activeLcs == 0)
          //  {
          //    NS_LOG_ERROR (this << " No active flows for this UL-DCI");
          //    return;
          //  }
          //std::map<uint8_t, LcInfo>::iterator it;
          //uint32_t bytesPerActiveLc = (dci.tbs/8)/ activeLcs;
          //bool statusPduPriority = false;
          //if ((statusPduMinSize != 0) && (bytesPerActiveLc < statusPduMinSize))
          //  {
          //    // send only the status PDU which has highest priority
          //    statusPduPriority = true;
          //    NS_LOG_DEBUG (this << " Reduced resource -> send only Status, b ytes "
          //                       << statusPduMinSize);
          //    if (dci.tbs/8< statusPduMinSize)
          //      {
          //        NS_FATAL_ERROR ("Insufficient Tx Opportunity for sending a status message");
          //      }
          //  }
          //NS_LOG_LOGIC (this << " UE " << m_rnti << ": UL-CQI notified TxOpportunity of "
          //                   << dci.tbs << " => " << bytesPerActiveLc << " bytes per active LC"
          //                   << " statusPduMinSize " << statusPduMinSize);

          //LteMacSapUser::TxOpportunityParameters txOpParams;

          //for (it = m_lcInfoMap.begin (); it != m_lcInfoMap.end (); it++)
          //  {
          //    itBsr = m_ulBsrReceived.find ((*it).first);
          //    NS_LOG_DEBUG (this << " Processing LC " << (uint32_t) (*it).first
          //                       << " bytesPerActiveLc " << bytesPerActiveLc);
          //    if ((itBsr != m_ulBsrReceived.end ()) &&
          //        (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) ||
          //         ((*itBsr).second.txQueueSize > 0)))
          //      {
          //        if ((statusPduPriority) && ((*itBsr).second.statusPduSize == statusPduMinSize))
          //          {
          //            txOpParams.bytes = (*itBsr).second.statusPduSize;
          //            txOpParams.layer = 0;
          //            txOpParams.harqId = 0;
          //            txOpParams.componentCarrierId = m_componentCarrierId;
          //            txOpParams.rnti = m_rnti;
          //            txOpParams.lcid = (*it).first;
          //            Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
          //                    it->second.macSapUser, txOpParams);
          //            NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " send  "
          //                               << (*itBsr).second.statusPduSize << " status bytes to LC "
          //                               << (uint32_t) (*it).first << " statusQueue "
          //                               << (*itBsr).second.statusPduSize << " retxQueue"
          //                               << (*itBsr).second.retxQueueSize << " txQueue"
          //                               << (*itBsr).second.txQueueSize);
          //            (*itBsr).second.statusPduSize = 0;
          //            break;
          //          }
          //        else
          //          {
          //            uint32_t bytesForThisLc = bytesPerActiveLc;
          //            NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " bytes to LC "
          //                               << (uint32_t) (*it).first << " statusQueue "
          //                               << (*itBsr).second.statusPduSize << " retxQueue"
          //                               << (*itBsr).second.retxQueueSize << " txQueue"
          //                               << (*itBsr).second.txQueueSize);
          //            if (((*itBsr).second.statusPduSize > 0) &&
          //                (bytesForThisLc > (*itBsr).second.statusPduSize))
          //              {
          //                txOpParams.bytes = (*itBsr).second.statusPduSize;
          //                txOpParams.layer = 0;
          //                txOpParams.harqId = 0;
          //                txOpParams.componentCarrierId = m_componentCarrierId;
          //                txOpParams.rnti = m_rnti;
          //                txOpParams.lcid = (*it).first;
          //                Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
          //                    it->second.macSapUser, txOpParams);
          //                bytesForThisLc -= (*itBsr).second.statusPduSize;
          //                NS_LOG_DEBUG (this << " serve STATUS " << (*itBsr).second.statusPduSize);
          //                (*itBsr).second.statusPduSize = 0;
          //              }
          //            else
          //              {
          //                if ((*itBsr).second.statusPduSize > bytesForThisLc)
          //                  {
          //                    NS_FATAL_ERROR (
          //                        "Insufficient Tx Opportunity for sending a status message");
          //                  }
          //              }

          //            if ((bytesForThisLc > 7) // 7 is the min TxOpportunity useful for Rlc
          //                && (((*itBsr).second.retxQueueSize > 0) ||
          //                    ((*itBsr).second.txQueueSize > 0)))
          //              {
          //                if ((*itBsr).second.retxQueueSize > 0)
          //                  {
          //                    NS_LOG_DEBUG (this << " serve retx DATA, bytes " << bytesForThisLc);
          //                    txOpParams.bytes = bytesForThisLc;
          //                    txOpParams.layer = 0;
          //                    txOpParams.harqId = 0;
          //                    txOpParams.componentCarrierId = m_componentCarrierId;
          //                    txOpParams.rnti = m_rnti;
          //                    txOpParams.lcid = (*it).first;
          //                    Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
          //                      it->second.macSapUser, txOpParams);
          //                    if ((*itBsr).second.retxQueueSize >= bytesForThisLc)
          //                      {
          //                        (*itBsr).second.retxQueueSize -= bytesForThisLc;
          //                      }
          //                    else
          //                      {
          //                        (*itBsr).second.retxQueueSize = 0;
          //                      }
          //                  }
          //                else if ((*itBsr).second.txQueueSize > 0)
          //                  {
          //                    uint16_t lcid = (*it).first;
          //                    uint32_t rlcOverhead;
          //                    if (lcid == 1)
          //                      {
          //                        // for SRB1 (using RLC AM) it's better to
          //                        // overestimate RLC overhead rather than
          //                        // underestimate it and risk unneeded
          //                        // segmentation which increases delay
          //                        rlcOverhead = 4;
          //                      }
          //                    else
          //                      {
          //                        // minimum RLC overhead due to header
          //                        rlcOverhead = 2;
          //                      }
          //                    NS_LOG_DEBUG (this << " serve tx DATA, bytes " << bytesForThisLc
          //                                       << ", RLC overhead " << rlcOverhead);
          //                    txOpParams.bytes = bytesForThisLc;
          //                    txOpParams.layer = 0;
          //                    txOpParams.harqId = 0;
          //                    txOpParams.componentCarrierId = m_componentCarrierId;
          //                    txOpParams.rnti = m_rnti;
          //                    txOpParams.lcid = (*it).first;

          //                    Simulator::Schedule (MilliSeconds (subframes), &LteMacSapUser::NotifyTxOpportunity,
          //                      it->second.macSapUser, txOpParams);
          //                    if ((*itBsr).second.txQueueSize >= bytesForThisLc - rlcOverhead)
          //                      {
          //                        (*itBsr).second.txQueueSize -= bytesForThisLc - rlcOverhead;
          //                      }
          //                    else
          //                      {
          //                        (*itBsr).second.txQueueSize = 0;
          //                      }
          //                  }
          //              }
          //            else
          //              {
          //                if (((*itBsr).second.retxQueueSize > 0) ||
          //                    ((*itBsr).second.txQueueSize > 0))
          //                  {
          //                    // resend BSR info for updating eNB peer MAC
          //                    m_freshUlBsr = true;
          //                  }
          //              }
          //            NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << "\t new queues "
          //                               << (uint32_t) (*it).first << " statusQueue "
          //                               << (*itBsr).second.statusPduSize << " retxQueue"
          //                               << (*itBsr).second.retxQueueSize << " txQueue"
          //                               << (*itBsr).second.txQueueSize);
          //          }
          //      }
          //  }
        }
      else
        {
          // HARQ retransmission -> retrieve data from HARQ buffer
          NS_LOG_DEBUG (this << " UE MAC RETX HARQ " << (uint16_t) m_harqProcessId);
          Ptr<PacketBurst> pb = m_miUlHarqProcessesPacket.at (m_harqProcessId);
          for (std::list<Ptr<Packet>>::const_iterator j = pb->Begin (); j != pb->End (); ++j)
            {
              Ptr<Packet> pkt = (*j)->Copy ();
              Simulator::Schedule (MilliSeconds (subframes), &LteUePhySapProvider::SendMacPdu ,
                m_uePhySapProvider, pkt);
              //m_uePhySapProvider->SendMacPdu (pkt);
            }
          m_miUlHarqProcessesPacketTimer.at (m_harqProcessId) = HARQ_PERIOD;
        }
    }
  else
    {
      NS_LOG_WARN (this << " LteControlMessage not recognized");
    }
}
void
LteUeMac::DoNotifyAboutHarqOpportunity (std::vector<std::pair<uint64_t, std::vector<uint64_t>>> subframes)
{
  m_nextPossibleHarqOpportunity = subframes;
}

void
LteUeMac::RefreshHarqProcessesPacketBuffer (void)
{
  NS_LOG_FUNCTION (this);

  for (uint16_t i = 0; i < m_miUlHarqProcessesPacketTimer.size (); i++)
    {
      if (m_miUlHarqProcessesPacketTimer.at (i) == 0)
        {
          if (m_miUlHarqProcessesPacket.at (i)->GetSize () > 0)
            {
              // timer expired: drop packets in buffer for this process
              NS_LOG_INFO (this << " HARQ Proc Id " << i << " packets buffer expired");
              Ptr<PacketBurst> emptyPb = CreateObject<PacketBurst> ();
              m_miUlHarqProcessesPacket.at (i) = emptyPb;
            }
        }
      else
        {
          m_miUlHarqProcessesPacketTimer.at (i)--;
        }
    }
}

void
LteUeMac::DoSubframeIndication (uint32_t frameNo, uint32_t subframeNo)
{
  NS_LOG_FUNCTION (this);
  m_frameNo = frameNo;
  m_subframeNo = subframeNo;
  //RefreshHarqProcessesPacketBuffer ();
  //
  if(m_edrx && GetBufferSizeComplete() == 0 && !m_transmissionScheduled){
    m_listenToSearchSpaces = false;
    m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_SUSPENDED_EDRX);
    m_edrx = false;
    // TODO Activate Paging Occasion listening
  }
  if(m_psm && GetBufferSizeComplete() == 0 && !m_transmissionScheduled){
    m_listenToSearchSpaces = false;
    m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_SUSPENDED_PSM);
    // TODO Activate Paging Occasion listening
    m_psm = false;
  }
  // Connected-mode DRX (TS 36.321 5.7 / DRX-Config-NB-r13): a connected UE only
  // opens NPDCCH search-space occasions during the on-duration of each long DRX
  // cycle. Outside the on-duration -- and with no active grant session, pending
  // UL data, or running drx-InactivityTimer -- it rests at the connected idle
  // floor (RRC_CONNECTED_IDLE) instead of firing an ~80 mW NPDCCH occasion every
  // search-space period. Uplink is unaffected: data sets the buffer (Active
  // Time) and the SR path runs independently, so only DL monitoring is gated.
  bool cdrxSleep = false;
  if (m_cdrxEnabled && m_listenToSearchSpaces)
    {
      uint64_t absSf = 10ull * (m_frameNo - 1) + (m_subframeNo - 1);
      bool inOnDuration = (absSf % m_cdrxCycleSubframes) < m_cdrxOnDurationSubframes;
      // Active Time (TS 36.321 5.7): driven by pending UL data and the running
      // drx-InactivityTimer (re-armed on every grant), NOT the m_grantSessionActive
      // flag -- that flag is latched on the first DCI and only released by a
      // total==0 BSR, which for the FUG-off UE may never recur, so it would pin
      // the UE permanently awake. Buffer + inactivity is the standard mechanism.
      // Active Time (TS 36.321 5.7) ALSO includes an in-progress random-access /
      // connection procedure: while the UE is waiting for a RA response or its Contention
      // Resolution Timer is running (waiting for Msg4 -- e.g. the RrcConnectionResume of a
      // capture loser re-RACHing deep in the sim), it MUST keep monitoring NPDCCH. Without
      // these, a UE resuming while cDRX is engaged DRX-sleeps through its Msg4, never
      // confirms, and the resume times out/re-parks forever (observed: one UE stuck).
      // RLC-AM ARQ gate: outstanding sent-but-unACKed PDUs hold Active Time -- the
      // UE keeps listening for the eNB's status PDU (ACK) before it may DRX-sleep.
      // Sleeping on pending ARQ state loses the ACK, and t-PollRetransmit later
      // forces a duplicate retransmission the eNB drops below its receive window.
      bool amUnacked = false;
      for (auto &kv : m_ulBsrReceived)
        {
          if (kv.second.unackedSize > 0) { amUnacked = true; break; }
        }
      bool activeTime = m_transmissionScheduled
                        || GetBufferSizeComplete () > 0
                        || Simulator::Now () < m_cdrxInactivityUntil
                        || m_waitingForRaResponse
                        || m_awaitingContentionResolution
                        || amUnacked;
      cdrxSleep = !inOnDuration && !activeTime;
      if (cdrxSleep)
        {
          m_inSearchSpace = false;
          m_subframesInSearchSpace = 0;
          if (m_cmacSapUser->GetEnergyState () != NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE)
            m_cmacSapUser->NotifyEnergyState (NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE);
        }
    }
  if(m_listenToSearchSpaces && !cdrxSleep){
    // Energy Model Start Receiving on my SearchSpaceBegin
    uint32_t searchSpacePeriodicity = NbIotRrcSap::ConvertNpdcchNumRepetitionsRa2int (m_CeLevel) *
                                      NbIotRrcSap::ConvertNpdcchStartSfCssRa2double (m_CeLevel);
    uint32_t searchSpaceConditionLeftSide =
        (10 * (m_frameNo - 1) + (m_subframeNo - 1)) % searchSpacePeriodicity;
    uint32_t searchSpaceConditionRightSide =
        NbIotRrcSap::ConvertNpdcchOffsetRa2double (m_CeLevel) * searchSpacePeriodicity;

    if (searchSpaceConditionLeftSide == searchSpaceConditionRightSide)
      {
        m_inSearchSpace=true;
        m_subframesInSearchSpace = 0;
      }
    if (m_inSearchSpace){
      if(!m_transmissionScheduled && m_cmacSapUser->GetEnergyState() == NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE){
        // We just moved from another state into IDLE
        // According to Liberg p.286 we still have to monitor the rest of the NPDCCH
        // Offset like the 3ms after NPUSCH F2 schould be handled by the m_transmissionScheduled flag
        m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_CONNECTED_RECEIVING_NPDCCH);
      }
      if (((m_subframeNo-1) != 0) && ((m_subframeNo-1) != 5) && !((m_subframeNo-1) == 9 && ((m_frameNo-1) % 2) == 1)) // Current Subframe is not NPBCH, NPSS and NSSS, and SI #TODO add SI
        {
          m_subframesInSearchSpace++;
        }
      if(m_subframesInSearchSpace == NbIotRrcSap::ConvertNpdcchNumRepetitionsRa2int (m_CeLevel)){
        m_inSearchSpace = false;
        m_subframesInSearchSpace = 0;
        if(m_cmacSapUser->GetEnergyState() == NbiotEnergyModel::PowerState::RRC_CONNECTED_RECEIVING_NPDCCH){
          m_cmacSapUser->NotifyEnergyState(NbiotEnergyModel::PowerState::RRC_CONNECTED_IDLE); // Device listened to the whole SearchSpace without Dci scheduled
        }
      }
    }
  }
  // CONSISTENT across ALL modes: the UE issues NO idealized BSR control message.
  // This free MAC-CE-over-control-channel reaches the eNB instantly and triggers
  // a grant (ReceiveBsrMessage), a second idealized buffer-report path on top of
  // the removed oracle. With it gone, the FIRST grant of an epoch comes only from
  // the realistic request mechanism -- the dedicated NPRACH SR (reactive FUG /
  // cDRX), the eNB predictor (proactive FUG), or Msg3's data-volume report (RA) --
  // and any remaining buffer within a granted epoch is conveyed by the realistic
  // BSR tag on the NPUSCH (eNB DoReceivePhyPdu -> ScheduleUlRlcBufferReq). So no
  // mode gets a free/instant buffer report; the comparison is apples-to-apples.
  bool bsrReady = false;
  if ((Simulator::Now () >= m_bsrLast + m_bsrPeriodicity) && (m_freshUlBsr == true) && bsrReady)
    {
      if (m_componentCarrierId == 0)
        {
          //Send BSR through primary carrier
          SendReportBufferStatus ();
        }
      m_bsrLast = Simulator::Now ();
      m_freshUlBsr = false;
    }
  m_harqProcessId = (m_harqProcessId + 1) % HARQ_PERIOD;
}

int64_t
LteUeMac::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_raPreambleUniformVariable->SetStream (stream);
  return 1;
}
void
LteUeMac::DoNotifyEdrx(){
  m_edrx = true;
  // Suspended: stop the MAC SR machine so no stray SR re-wakes the UE.
  m_suspended = true;
  m_srPending = false;
  m_grantSessionActive = false;
  m_srEvent.Cancel ();
  m_srContentionEvent.Cancel ();
  // Kill any IN-FLIGHT contention RA too (preamble already on the air): the event
  // cancellations above stop FUTURE attempts, but a RAR arriving after this
  // suspend would still be processed (m_waitingForRaResponse) and re-animate the
  // just-released session -- a ghost resume with nothing to send that dangles
  // until the next inactivity release and misses its PSM sleep (observed: ~20
  // UEs/run at duty 0.72 browning out at the next epoch wave).
  m_waitingForRaResponse = false;
  m_noRaResponseReceivedEvent.Cancel ();
  m_awaitingContentionResolution = false;
  m_contentionResolutionTimer.Cancel ();
  m_srContentionRa = false;
  m_msg3HasCrntiMacCe = false;
  m_contentionTempRnti = 0;
  m_raKeepCrnti = 0;
  m_msg5Buffer = 0;
  RestoreSrCeLevelAfterEdt ();
  if (m_edtMsg3DataSent)
    {
      m_edtMsg3DataSent = false;
      for (auto &kv : m_lcInfoMap)
        {
          if (kv.first > 2 && kv.second.macSapUser != nullptr)
            {
              kv.second.macSapUser->NotifyHarqDeliveryFailure ();
            }
        }
    }
  // NOTE: do NOT clear m_ulBsrReceived here. The spurious-SR-on-suspend loop is
  // already prevented by the m_suspended guard in SendSchedulingRequest/the SR
  // gate. Clearing the buffer view orphans data that is still in the RLC when the
  // UE suspends (RLC-UM won't re-report an unchanged queue on resume), causing
  // packet loss.
  m_freshUlBsr = false;
}

void
LteUeMac::DoNotifyPsm(){
  m_psm = true;
  m_suspended = true;
  m_srPending = false;
  m_grantSessionActive = false;
  m_srEvent.Cancel ();
  m_srContentionEvent.Cancel ();
  // See DoNotifyEdrx: kill any IN-FLIGHT contention RA (ghost-resume guard).
  m_waitingForRaResponse = false;
  m_noRaResponseReceivedEvent.Cancel ();
  m_awaitingContentionResolution = false;
  m_contentionResolutionTimer.Cancel ();
  m_srContentionRa = false;
  m_msg3HasCrntiMacCe = false;
  m_contentionTempRnti = 0;
  m_raKeepCrnti = 0;
  m_msg5Buffer = 0;
  RestoreSrCeLevelAfterEdt ();
  if (m_edtMsg3DataSent)
    {
      m_edtMsg3DataSent = false;
      for (auto &kv : m_lcInfoMap)
        {
          if (kv.first > 2 && kv.second.macSapUser != nullptr)
            {
              kv.second.macSapUser->NotifyHarqDeliveryFailure ();
            }
        }
    }
  // See DoNotifyEdrx: do NOT clear m_ulBsrReceived (orphans RLC-pending data).
  m_freshUlBsr = false;
}

NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel
LteUeMac::DoGetCoverageEnhancementLevel(){
  return m_CeLevel.coverageEnhancementLevel;
}

void LteUeMac::DoSetMsg5Buffer(uint32_t buffersize){
  m_msg5Buffer = buffersize;
}

void LteUeMac::SetLogDir(std::string dirname){
  m_logdir = dirname;
  m_mac_logging = !dirname.empty();
}

void LteUeMac::LogMessage(std::string msg){
  std::string logfile_path = m_logdir+"ueMAC.log";
  std::ofstream logfile;
  logfile.open(logfile_path, std::ios_base::app);
  logfile <<  m_imsi << "," << msg << "," << Simulator::Now().GetMilliSeconds() << "\n";
  logfile.close();
}

} // namespace ns3

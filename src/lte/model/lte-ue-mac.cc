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

  // Transmit a real NPRACH preamble on this UE's reserved subcarrier. The
  // preamble carries NO identity; the eNB resolves the C-RNTI from the
  // resource->UE map it holds (registered at connect). Reserved subcarrier =
  // contentionOffset + (srIndex % N_res). Preamble duration mirrors the RA path.
  uint32_t reservedSub = m_srContentionOffset + (m_srIndex % m_srReservedSubcarriers);
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
  // Retry at the next dedicated occasion if no grant arrives (the DCI N0 handler
  // clears m_srPending and cancels m_srEvent on success).
  uint64_t toNext = MsToNextDedicatedOccasion ();
  if (toNext == 0) toNext = NbIotRrcSap::ConvertNprachPeriodicity2int (m_CeLevel);
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
  uint64_t total = 0;
  for (auto & kv : m_ulBsrReceived)
    total += kv.second.txQueueSize + kv.second.retxQueueSize + kv.second.statusPduSize;
  if (total == 0) { m_srPending = false; return; }     // buffer drained

  // Opportunistic contention SR: pick a random subcarrier from the shared pool
  // [0, contentionOffset) and transmit a real NPRACH preamble there. Unlike the
  // reserved path the eNB cannot map this resource to a UE; on a singleton it
  // resolves identity as it would from the winner's subsequent NPUSCH C-RNTI
  // (abstracted here, mirroring the dedicated-SR map). A collision yields no
  // grant, so the UE retries next occasion or falls back to its reserved slot.
  uint32_t sub = m_raPreambleUniformVariable->GetInteger (0, m_srContentionOffset - 1);
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
                       m_uePhySapProvider, (uint32_t) sub, (uint32_t) m_rnti, ceOffset);
  // Retry at the next base occasion until a grant arrives (the DCI N0 handler
  // clears m_srPending and cancels this event on success).
  uint64_t toNext = MsToNextBaseOccasion ();
  m_srContentionEvent = Simulator::Schedule (MilliSeconds (toNext),
                                             &LteUeMac::SendContentionSrPreamble, this);
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
  LteRadioBearerTag radioTag (params.rnti, params.lcid, 0 /* UE works in SISO mode*/);
  DataVolumeAndPowerHeadroomTag dprTag;
  BufferStatusReportTag bsrTag;
  uint64_t bsr =0;
  //DoSetTransmissionScheduled(false);
  if(m_msg5Buffer > 0){
    // We are just about to send MSG3, add DPR Element for MSG5 (potentially CIoT-Opt)
    //std::cout << " set payload" << std::endl;
    uint8_t dataVolumeIndex = DataVolumeDPR::BufferSize2DVId(m_msg5Buffer);
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
        m_contentionResolutionTimer = Simulator::Schedule (
            MilliSeconds (64), &LteUeMac::ContentionResolutionTimeout, this);
        m_msg3HasCrntiMacCe = false;
        m_raKeepCrnti = 0;
      }
  }
  else{

    bsr = GetBufferSizeComplete();
    if(bsr > 0){

      bsrTag.SetBufferStatusReportIndex(BufferSizeLevelBsr::BufferSize2BsrId (bsr));
      params.pdu->AddPacketTag(bsrTag);
    }
    // Normal PDU just add BSR for next Packet

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
    if (total == 0)
    {
      // AS RAI (TS 36.321 5.4.5, NB-IoT): a zero-byte BSR has been triggered at
      // the end of an active grant session and no further UL/DL data is expected
      // (single-packet-per-epoch). Signal the AS RAI MAC CE so the eNB releases
      // immediately, instead of waiting for the data-inactivity timer.
      if (m_grantSessionActive && m_raiActivation && m_rnti != 0 && !m_raiCb.IsNull ())
      {
        m_raiCb (m_rnti);
      }
      // Buffer drained: grant session over; next packet re-requests via SR.
      m_grantSessionActive = false;
    }
    else if (m_oracleBsr && !m_grantSessionActive && !m_suspended)
    {
      // Oracle / ideal BSR (upper bound): the eNB learns the buffer the instant
      // data arrives -- no SR period, no contention, no preamble energy. Fires
      // the already-wired m_idealBsrCb -> NotifyIdealUlBuffer immediately. The
      // UE still transmits the data on the granted NPUSCH (that energy is still
      // charged); only the SIGNALLING overhead is removed. Off by default; this
      // is a separate comparison arm, NOT folded into the realistic FUG modes.
      m_grantSessionActive = true;
      if (!m_idealBsrCb.IsNull ())
        m_idealBsrCb (m_rnti, total);
    }
    else if (!m_srPending && !m_grantSessionActive && !m_suspended
             && Simulator::Now () >= m_srProhibitUntil
             && !(m_proactiveFug && m_fugBootstrapEpochs >= kProactiveBootstrapEpochs))
    {
      // Proactive FUG (4th mode): once the eNB has learned this UE's period it
      // pushes a predicted grant, so the UE sends NO scheduling request -- the
      // guard above disables the SR subsystem in steady state. Only the first
      // kProactiveBootstrapEpochs epochs still fire a reactive SR, to give the
      // eNB the >=2 arrivals it needs before it can predict.
      if (m_proactiveFug)
        m_fugBootstrapEpochs++;
      // !m_suspended: while RRC-suspended (PSM/eDRX) the UE must NOT fire a
      // direct SR -- data wakes it via the RRC resume path (DoSendData ->
      // IDLE_WAIT_MIB -> SR-resume). Firing here would re-wake a just-suspended
      // UE and spin the suspend<->wake loop.
      m_srPending = true;
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

  m_rnti = raResponse.cellRnti;
  m_cmacSapUser->SetTemporaryCellRnti (m_rnti);
  // C-RNTI MAC CE (TS 36.321 5.1.5): a connected UE doing RA uses the Temporary
  // C-RNTI only to receive the Msg3 grant, but signals its existing C-RNTI in a
  // C-RNTI MAC CE on Msg3 -- the eNB then resolves contention by C-RNTI and the
  // UE keeps its identity (the Temporary C-RNTI is discarded, see DoTransmitPdu).
  if (m_raKeepCrnti != 0)
    {
      m_msg3HasCrntiMacCe = true;
    }
  // in principle we should wait for contention resolution,
  // but in the current LTE model when two or more identical
  // preambles are sent no one is received, so there is no need
  // for contention resolution

  // To be comented in
  bool edt;
  if(raResponse.ulGrant.tbs_size > 88){
    // We got a grant for EDT
    edt = true;
  }else{
    edt = false;
  }
  m_cmacSapUser->NotifyRandomAccessSuccessful (edt);

  // trigger tx opportunity for Message 3 over LC 0
  // this is needed since Message 3's UL GRANT is in the RAR, not in UL-DCIs
  const uint8_t lc0Lcid = 0;
  std::map<uint8_t, LcInfo>::iterator lc0InfoIt = m_lcInfoMap.find (lc0Lcid);
  NS_ASSERT (lc0InfoIt != m_lcInfoMap.end ());
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator lc0BsrIt =
      m_ulBsrReceived.find (lc0Lcid);
  if ((lc0BsrIt != m_ulBsrReceived.end ()) && (lc0BsrIt->second.txQueueSize > 0))
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
      m_srEvent.Cancel ();
      m_srContentionEvent.Cancel ();
      m_srProhibitUntil = Simulator::Now ();
      // First grant after the SR opens the grant session: from here the real
      // buffer flows via the BSR MAC CE (on this granted NPUSCH and the
      // periodic-BSR path), not via further SRs. Stays active until the buffer
      // drains (see DoReportBufferStatus).
      m_grantSessionActive = true;
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
      bool activeTime = m_transmissionScheduled
                        || GetBufferSizeComplete () > 0
                        || Simulator::Now () < m_cdrxInactivityUntil;
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

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2018 Fraunhofer ESK : RLF extensions
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
 * Authors: Nicola Baldo <nbaldo@cttc.es>
 *          Marco Miozzo <mmiozzo@cttc.es>
 *          Manuel Requena <manuel.requena@cttc.es>
 * Modified by:  Danilo Abrignani <danilo.abrignani@unibo.it> (Carrier Aggregation - GSoC 2015),
 *               Biljana Bojovic <biljana.bojovic@cttc.es> (Carrier Aggregation)
 *               Vignesh Babu <ns3-dev@esk.fraunhofer.de> (RLF extensions)
 * 				 Tim Gebauer <tim.gebauer@tu-dortmund.de> (NB-IoT Extension)
 */

#include "lte-enb-rrc.h"
#include "lte-common.h"

#include <ns3/fatal-error.h>
#include <ns3/log.h>
#include <ns3/abort.h>
#include <ns3/build-profile.h>

#include <ns3/pointer.h>
#include <ns3/object-map.h>
#include <ns3/object-factory.h>
#include <ns3/simulator.h>

#include <ns3/lte-radio-bearer-info.h>
#include <ns3/eps-bearer-tag.h>
#include <ns3/packet.h>

#include <fstream>
#include <ns3/lte-rlc.h>
#include <ns3/lte-rlc-tm.h>
#include <ns3/lte-rlc-um.h>
#include <ns3/lte-rlc-am.h>
#include <ns3/lte-pdcp.h>
#include <cmath>

#include <execinfo.h>
#include <iostream>
#include <cstdlib>
#include <stdexcept>



/**
 * Prints the current call stack to the console.
 *
 * This function uses the backtrace() and backtrace_symbols() functions
 * from the execinfo.h header to obtain the current call stack and
 * resolve the symbols to human-readable names. It then prints the
 * call stack to the console.
 *
 * This function is mostly useful for debugging purposes.
 * It was used in LteEnbRrc::GetUeManagerbyRnti()
 */
void print_stack() {
    const int max_frames = 64;
    void* buffer[max_frames];
    int nptrs = backtrace(buffer, max_frames);
    char** symbols = backtrace_symbols(buffer, nptrs);

    std::cout << "Call stack:\n";
    for (int i = 0; i < nptrs; ++i)
        std::cout << symbols[i] << '\n';

    free(symbols);
}

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LteEnbRrc");

///////////////////////////////////////////
// CMAC SAP forwarder
///////////////////////////////////////////

/**
 * \brief Class for forwarding CMAC SAP User functions.
 */
class EnbRrcMemberLteEnbCmacSapUser : public LteEnbCmacSapUser
{
public:
  /**
   * Constructor
   *
   * \param rrc ENB RRC
   * \param componentCarrierId
   */
  EnbRrcMemberLteEnbCmacSapUser (LteEnbRrc* rrc, uint8_t componentCarrierId);

  virtual uint16_t AllocateTemporaryCellRnti ();
  virtual void NotifyLcConfigResult (uint16_t rnti, uint8_t lcid, bool success);
  virtual void RrcConfigurationUpdateInd (UeConfig params);
  virtual bool IsRandomAccessCompleted (uint16_t rnti);
  virtual NbIotRrcSap::SystemInformationBlockType2Nb GetCurrentSystemInformationBlockType2Nb();
  virtual void NotifyDataInactivityNb(uint16_t rnti, uint8_t lcid);
  virtual void NotifyDataInactivitySchedulerNb(uint16_t rnti);
  virtual void NotifyReleaseAssistanceNb(uint16_t rnti);
  virtual void NotifyDataActivitySchedulerNb(uint16_t rnti);
  virtual void NotifyUlDataObservedNb(uint16_t rnti);
  virtual bool IsUeInReleaseGraceNb(uint16_t rnti);

private:
  LteEnbRrc* m_rrc; ///< the RRC
  uint8_t m_componentCarrierId; ///< Component carrier ID
};

EnbRrcMemberLteEnbCmacSapUser::EnbRrcMemberLteEnbCmacSapUser (LteEnbRrc* rrc, uint8_t componentCarrierId)
  : m_rrc (rrc)
  , m_componentCarrierId {componentCarrierId}
{
}

uint16_t
EnbRrcMemberLteEnbCmacSapUser::AllocateTemporaryCellRnti ()
{
  return m_rrc->DoAllocateTemporaryCellRnti (m_componentCarrierId);
}

void
EnbRrcMemberLteEnbCmacSapUser::NotifyLcConfigResult (uint16_t rnti, uint8_t lcid, bool success)
{
  m_rrc->DoNotifyLcConfigResult (rnti, lcid, success);
}

void
EnbRrcMemberLteEnbCmacSapUser::RrcConfigurationUpdateInd (UeConfig params)
{
  m_rrc->DoRrcConfigurationUpdateInd (params);
}

bool
EnbRrcMemberLteEnbCmacSapUser::IsRandomAccessCompleted (uint16_t rnti)
{
  return m_rrc->IsRandomAccessCompleted (rnti);
}

NbIotRrcSap::SystemInformationBlockType2Nb
EnbRrcMemberLteEnbCmacSapUser::GetCurrentSystemInformationBlockType2Nb(){
  return m_rrc->DoGetCurrentSystemInformationBlockType2Nb();
}
void
EnbRrcMemberLteEnbCmacSapUser::NotifyDataInactivityNb(uint16_t rnti, uint8_t lcid)
{
  m_rrc->DoNotifyDataInactivityNb(rnti, lcid);
}
void
EnbRrcMemberLteEnbCmacSapUser::NotifyDataInactivitySchedulerNb(uint16_t rnti)
{
  m_rrc->DoNotifyDataInactivitySchedulerNb(rnti);
}
void
EnbRrcMemberLteEnbCmacSapUser::NotifyReleaseAssistanceNb(uint16_t rnti)
{
  m_rrc->DoNotifyReleaseAssistanceNb(rnti);
}
void
EnbRrcMemberLteEnbCmacSapUser::NotifyDataActivitySchedulerNb(uint16_t rnti)
{
  m_rrc->DoNotifyDataActivitySchedulerNb(rnti);
}
void
EnbRrcMemberLteEnbCmacSapUser::NotifyUlDataObservedNb(uint16_t rnti)
{
  m_rrc->DoNotifyUlDataObservedNb(rnti);
}
bool
EnbRrcMemberLteEnbCmacSapUser::IsUeInReleaseGraceNb(uint16_t rnti)
{
  return m_rrc->DoIsUeInReleaseGraceNb(rnti);
}
///////////////////////////////////////////
// UeManager
///////////////////////////////////////////


/// Map each of UE Manager states to its string representation.
static const std::string g_ueManagerStateName[UeManager::NUM_STATES] =
{
  "INITIAL_RANDOM_ACCESS",
  "CONNECTION_SETUP",
  "CONNECTION_REJECTED",
  "CONNECTION_RESUME",// New NBIOT
  "ATTACH_REQUEST",
  "CONNECTED_NORMALLY",
  "CONNECTION_RECONFIGURATION",
  "CONNECTION_REESTABLISHMENT",
  "HANDOVER_PREPARATION",
  "HANDOVER_JOINING",
  "HANDOVER_PATH_SWITCH",
  "HANDOVER_LEAVING",
  "IDLE_SUSPEND_EDRX", // New NBIOT
  "IDLE_SUSPEND_PSM", // New NBIOT
  "CONNECTED_TAU"// New NBIOT
  "IDLE_EARLY_DATA_TRANSMISSION" // New NBIOT
};

/**
 * \param s The UE manager state.
 * \return The string representation of the given state.
 */
static const std::string & ToString (UeManager::State s)
{
  return g_ueManagerStateName[s];
}


NS_OBJECT_ENSURE_REGISTERED (UeManager);


UeManager::UeManager ()
{
  NS_FATAL_ERROR ("this constructor is not expected to be used");
}


UeManager::UeManager (Ptr<LteEnbRrc> rrc, uint16_t rnti, State s, uint8_t componentCarrierId)
  : m_lastAllocatedDrbid (0),
    m_rnti (rnti),
    m_imsi (0),
    m_componentCarrierId (componentCarrierId),
    m_lastRrcTransactionIdentifier (0),
    m_rrc (rrc),
    m_state (s),
    m_pendingRrcConnectionReconfiguration (false),
    m_sourceX2apId (0),
    m_sourceCellId (0),
    m_needPhyMacConfiguration (false),
    m_caSupportConfigured (false),
    m_pendingStartDataRadioBearers (false),
    // 3GPP / GSMA NB-IoT mass-IoT deployment-guide values:
    //   T3324      = 20 s   (active timer)
    //   T3412      = 1 h    (periodic TAU)
    //   eDRX cycle = 20.48 s
    //   RRC release inactivity = 5 s
    m_t3412(Hours(1)),
    m_t3324(Seconds(20)),
    m_dataInactivityInterval(5000),
    m_eDrxCycle(MilliSeconds(20480)),
    m_enablePSM(true),
    m_persistentGrant(false)
{
  NS_LOG_FUNCTION (this);
}

void
UeManager::DoInitialize ()
{
  NS_LOG_FUNCTION (this);
  m_drbPdcpSapUser = new LtePdcpSpecificLtePdcpSapUser<UeManager> (this);

  m_physicalConfigDedicated.haveAntennaInfoDedicated = true;
  m_physicalConfigDedicated.antennaInfo.transmissionMode = m_rrc->m_defaultTransmissionMode;
  m_physicalConfigDedicated.haveSoundingRsUlConfigDedicated = true;
  m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex = m_rrc->GetNewSrsConfigurationIndex ();
  m_physicalConfigDedicated.soundingRsUlConfigDedicated.type = LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
  m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsBandwidth = 0;
  m_physicalConfigDedicated.havePdschConfigDedicated = true;
  m_physicalConfigDedicated.pdschConfigDedicated.pa = LteRrcSap::PdschConfigDedicated::dB0;

  // Pull NB-IoT timer values from LteEnbRrc into this UeManager.
  m_dataInactivityInterval = m_rrc->m_dataInactivityInterval;
  // Proactive-FUG flag is cell-wide; propagate it so a speculatively pushed
  // DCI0 does not force-wake this UeManager from a suspended state.
  m_proactiveFug = m_rrc->IsProactiveFug();
  // Connected-DRX FUG flag is cell-wide; propagate it so the data-inactivity
  // release is suppressed and the UE stays RRC_CONNECTED (MAC-cDRX sleep).
  m_cdrxFug = m_rrc->IsCdrxFug();

  for (uint8_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
      m_rrc->m_cmacSapProvider.at (i)->AddUe (m_rnti);
      m_rrc->m_cphySapProvider.at (i)->AddUe (m_rnti);
    }

  // setup the eNB side of SRB0
  {
    uint8_t lcid = 0;

    Ptr<LteRlc> rlc = CreateObject<LteRlcTm> ()->GetObject<LteRlc> ();
    rlc->SetLteMacSapProvider (m_rrc->m_macSapProvider);
    rlc->SetRnti (m_rnti);
    rlc->SetLcId (lcid);

    m_srb0 = CreateObject<LteSignalingRadioBearerInfo> ();
    m_srb0->m_rlc = rlc;
    m_srb0->m_srbIdentity = 0;
    // no need to store logicalChannelConfig as SRB0 is pre-configured

    LteEnbCmacSapProvider::LcInfo lcinfo;
    lcinfo.rnti = m_rnti;
    lcinfo.lcId = lcid;
    // Initialise the rest of lcinfo structure even if CCCH (LCID 0) is pre-configured, and only m_rnti and lcid will be used from passed lcinfo structure.
    // See FF LTE MAC Scheduler Iinterface Specification v1.11, 4.3.4 logicalChannelConfigListElement
    lcinfo.lcGroup = 0;
    lcinfo.qci = 0;
    lcinfo.isGbr = false;
    lcinfo.mbrUl = 0;
    lcinfo.mbrDl = 0;
    lcinfo.gbrUl = 0;
    lcinfo.gbrDl = 0;

    // MacSapUserForRlc in the ComponentCarrierManager MacSapUser
    LteMacSapUser* lteMacSapUser = m_rrc->m_ccmRrcSapProvider->ConfigureSignalBearer(lcinfo, rlc->GetLteMacSapUser ());
    // Signal Channel are only on Primary Carrier
    m_rrc->m_cmacSapProvider.at (m_componentCarrierId)->AddLc (lcinfo, lteMacSapUser);
    m_rrc->m_ccmRrcSapProvider->AddLc (lcinfo, lteMacSapUser);
  }

  // setup the eNB side of SRB1; the UE side will be set up upon RRC connection establishment
  {
    uint8_t lcid = 1;

    Ptr<LteRlc> rlc = CreateObject<LteRlcAm> ()->GetObject<LteRlc> ();
    rlc->SetLteMacSapProvider (m_rrc->m_macSapProvider);
    rlc->SetRnti (m_rnti);
    rlc->SetLcId (lcid);

    Ptr<LtePdcp> pdcp = CreateObject<LtePdcp> ();
    pdcp->SetRnti (m_rnti);
    pdcp->SetLcId (lcid);
    pdcp->SetLtePdcpSapUser (m_drbPdcpSapUser);
    pdcp->SetLteRlcSapProvider (rlc->GetLteRlcSapProvider ());
    rlc->SetLteRlcSapUser (pdcp->GetLteRlcSapUser ());

    m_srb1 = CreateObject<LteSignalingRadioBearerInfo> ();
    m_srb1->m_rlc = rlc;
    m_srb1->m_pdcp = pdcp;
    m_srb1->m_srbIdentity = 1;
    m_srb1->m_logicalChannelConfig.priority = 1;
    m_srb1->m_logicalChannelConfig.prioritizedBitRateKbps = 100;
    m_srb1->m_logicalChannelConfig.bucketSizeDurationMs = 100;
    m_srb1->m_logicalChannelConfig.logicalChannelGroup = 0;

    LteEnbCmacSapProvider::LcInfo lcinfo;
    lcinfo.rnti = m_rnti;
    lcinfo.lcId = lcid;
    lcinfo.lcGroup = 0; // all SRBs always mapped to LCG 0
    lcinfo.qci = EpsBearer::GBR_CONV_VOICE; // not sure why the FF API requires a CQI even for SRBs...
    lcinfo.isGbr = true;
    lcinfo.mbrUl = 1e6;
    lcinfo.mbrDl = 1e6;
    lcinfo.gbrUl = 1e4;
    lcinfo.gbrDl = 1e4;
    // MacSapUserForRlc in the ComponentCarrierManager MacSapUser
    LteMacSapUser* MacSapUserForRlc = m_rrc->m_ccmRrcSapProvider->ConfigureSignalBearer(lcinfo, rlc->GetLteMacSapUser ());
    // Signal Channel are only on Primary Carrier
    m_rrc->m_cmacSapProvider.at (m_componentCarrierId)->AddLc (lcinfo, MacSapUserForRlc);
    m_rrc->m_ccmRrcSapProvider->AddLc (lcinfo, MacSapUserForRlc);
  }

  LteEnbRrcSapUser::SetupUeParameters ueParams;
  ueParams.srb0SapProvider = m_srb0->m_rlc->GetLteRlcSapProvider ();
  ueParams.srb1SapProvider = m_srb1->m_pdcp->GetLtePdcpSapProvider ();
  m_rrc->m_rrcSapUser->SetupUe (m_rnti, ueParams);

  // configure MAC (and scheduler)
  LteEnbCmacSapProvider::UeConfig req;
  req.m_rnti = m_rnti;
  req.m_transmissionMode = m_physicalConfigDedicated.antennaInfo.transmissionMode;

  // configure PHY
  for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
      m_rrc->m_cmacSapProvider.at (i)->UeUpdateConfigurationReq (req);
      m_rrc->m_cphySapProvider.at (i)->SetTransmissionMode (m_rnti, m_physicalConfigDedicated.antennaInfo.transmissionMode);
      m_rrc->m_cphySapProvider.at (i)->SetSrsConfigurationIndex (m_rnti, m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex);
    }
  // schedule this UeManager instance to be deleted if the UE does not give any sign of life within a reasonable time
  Time maxConnectionDelay;
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
      m_connectionRequestTimeout = Simulator::Schedule (m_rrc->m_connectionRequestTimeoutDuration,
                                                        &LteEnbRrc::ConnectionRequestTimeout,
                                                        m_rrc, m_rnti);
      break;

    case HANDOVER_JOINING:
      m_handoverJoiningTimeout = Simulator::Schedule (m_rrc->m_handoverJoiningTimeoutDuration,
                                                      &LteEnbRrc::HandoverJoiningTimeout,
                                                      m_rrc, m_rnti);
      break;
    case IDLE_EARLY_DATA_TRANSMISSION:
      break;
    default:
      NS_FATAL_ERROR ("unexpected state " << ToString (m_state));
      break;
    }
  m_caSupportConfigured =  false;
}


UeManager::~UeManager (void)
{
}

void
UeManager::DoDispose ()
{
  delete m_drbPdcpSapUser;
  // delete eventual X2-U TEIDs
  for (std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.begin ();
       it != m_drbMap.end ();
       ++it)
    {
      m_rrc->m_x2uTeidInfoMap.erase (it->second->m_gtpTeid);
    }

}

TypeId UeManager::GetTypeId (void)
{
  static TypeId  tid = TypeId ("ns3::UeManager")
    .SetParent<Object> ()
    .AddConstructor<UeManager> ()
    .AddAttribute ("DataRadioBearerMap", "List of UE DataRadioBearerInfo by DRBID.",
                   ObjectMapValue (),
                   MakeObjectMapAccessor (&UeManager::m_drbMap),
                   MakeObjectMapChecker<LteDataRadioBearerInfo> ())
    .AddAttribute ("Srb0", "SignalingRadioBearerInfo for SRB0",
                   PointerValue (),
                   MakePointerAccessor (&UeManager::m_srb0),
                   MakePointerChecker<LteSignalingRadioBearerInfo> ())
    .AddAttribute ("Srb1", "SignalingRadioBearerInfo for SRB1",
                   PointerValue (),
                   MakePointerAccessor (&UeManager::m_srb1),
                   MakePointerChecker<LteSignalingRadioBearerInfo> ())
    .AddAttribute ("C-RNTI",
                   "Cell Radio Network Temporary Identifier",
                   TypeId::ATTR_GET, // read-only attribute
                   UintegerValue (0), // unused, read-only attribute
                   MakeUintegerAccessor (&UeManager::m_rnti),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("StateTransition",
                     "fired upon every UE state transition seen by the "
                     "UeManager at the eNB RRC",
                     MakeTraceSourceAccessor (&UeManager::m_stateTransitionTrace),
                     "ns3::UeManager::StateTracedCallback")
    .AddTraceSource ("DrbCreated",
                     "trace fired after DRB is created",
                     MakeTraceSourceAccessor (&UeManager::m_drbCreatedTrace),
                     "ns3::UeManager::ImsiCidRntiLcIdTracedCallback")
  ;
  return tid;
}

void
UeManager::SetSource (uint16_t sourceCellId, uint16_t sourceX2apId)
{
  m_sourceX2apId = sourceX2apId;
  m_sourceCellId = sourceCellId;
}

void
UeManager::SetImsi (uint64_t imsi)
{

  m_imsi = imsi;
}

void
UeManager::SetRnti(uint16_t rnti)
{
  m_rnti= rnti;
}
void
UeManager::SetResumeId(uint64_t resumeId)
{
  m_resumeId = resumeId;
}
void
UeManager::InitialContextSetupRequest ()
{
  NS_LOG_FUNCTION (this << m_rnti);

  if (m_state == ATTACH_REQUEST)
    {
      SwitchToState (CONNECTED_NORMALLY);
    }
  else if (m_state == IDLE_SUSPEND_PSM){
    return;
  }
  else
    {
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
    }
}

void
UeManager::SetupDataRadioBearer (EpsBearer bearer, uint8_t bearerId, uint32_t gtpTeid, Ipv4Address transportLayerAddress)
{
  NS_LOG_FUNCTION (this << (uint32_t) m_rnti);

  Ptr<LteDataRadioBearerInfo> drbInfo = CreateObject<LteDataRadioBearerInfo> ();
  uint8_t drbid = AddDataRadioBearerInfo (drbInfo);
  uint8_t lcid = Drbid2Lcid (drbid);
  uint8_t bid = Drbid2Bid (drbid);
  NS_ASSERT_MSG ( bearerId == 0 || bid == bearerId, "bearer ID mismatch (" << (uint32_t) bid << " != " << (uint32_t) bearerId << ", the assumption that ID are allocated in the same way by MME and RRC is not valid any more");
  drbInfo->m_epsBearer = bearer;
  drbInfo->m_epsBearerIdentity = bid;
  drbInfo->m_drbIdentity = drbid;
  drbInfo->m_logicalChannelIdentity = lcid;
  drbInfo->m_gtpTeid = gtpTeid;
  drbInfo->m_transportLayerAddress = transportLayerAddress;

  if (m_state == HANDOVER_JOINING)
    {
      // setup TEIDs for receiving data eventually forwarded over X2-U
      LteEnbRrc::X2uTeidInfo x2uTeidInfo;
      x2uTeidInfo.rnti = m_rnti;
      x2uTeidInfo.drbid = drbid;
      std::pair<std::map<uint32_t, LteEnbRrc::X2uTeidInfo>::iterator, bool>
      ret = m_rrc->m_x2uTeidInfoMap.insert (std::pair<uint32_t, LteEnbRrc::X2uTeidInfo> (gtpTeid, x2uTeidInfo));
      NS_ASSERT_MSG (ret.second == true, "overwriting a pre-existing entry in m_x2uTeidInfoMap");
    }

  TypeId rlcTypeId = m_rrc->GetRlcType (bearer);

  ObjectFactory rlcObjectFactory;
  rlcObjectFactory.SetTypeId (rlcTypeId);
  Ptr<LteRlc> rlc = rlcObjectFactory.Create ()->GetObject<LteRlc> ();
  rlc->SetLteMacSapProvider (m_rrc->m_macSapProvider);
  rlc->SetRnti (m_rnti);

  drbInfo->m_rlc = rlc;

  rlc->SetLcId (lcid);

  // we need PDCP only for real RLC, i.e., RLC/UM or RLC/AM
  // if we are using RLC/SM we don't care of anything above RLC
  if (rlcTypeId != LteRlcSm::GetTypeId ())
    {
      Ptr<LtePdcp> pdcp = CreateObject<LtePdcp> ();
      pdcp->SetRnti (m_rnti);
      pdcp->SetLcId (lcid);
      pdcp->SetLtePdcpSapUser (m_drbPdcpSapUser);
      pdcp->SetLteRlcSapProvider (rlc->GetLteRlcSapProvider ());
      rlc->SetLteRlcSapUser (pdcp->GetLteRlcSapUser ());
      drbInfo->m_pdcp = pdcp;
    }

  m_drbCreatedTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti, lcid);

  std::vector<LteCcmRrcSapProvider::LcsConfig> lcOnCcMapping = m_rrc->m_ccmRrcSapProvider->SetupDataRadioBearer (bearer, bearerId, m_rnti, lcid, m_rrc->GetLogicalChannelGroup (bearer), rlc->GetLteMacSapUser ());
  // LteEnbCmacSapProvider::LcInfo lcinfo;
  // lcinfo.rnti = m_rnti;
  // lcinfo.lcId = lcid;
  // lcinfo.lcGroup = m_rrc->GetLogicalChannelGroup (bearer);
  // lcinfo.qci = bearer.qci;
  // lcinfo.isGbr = bearer.IsGbr ();
  // lcinfo.mbrUl = bearer.gbrQosInfo.mbrUl;
  // lcinfo.mbrDl = bearer.gbrQosInfo.mbrDl;
  // lcinfo.gbrUl = bearer.gbrQosInfo.gbrUl;
  // lcinfo.gbrDl = bearer.gbrQosInfo.gbrDl;
  // use a for cycle to send the AddLc to the appropriate Mac Sap
  // if the sap is not initialized the appropriated method has to be called
  std::vector<LteCcmRrcSapProvider::LcsConfig>::iterator itLcOnCcMapping = lcOnCcMapping.begin ();
  NS_ASSERT_MSG (itLcOnCcMapping != lcOnCcMapping.end (), "Problem");
  for (itLcOnCcMapping = lcOnCcMapping.begin (); itLcOnCcMapping != lcOnCcMapping.end (); ++itLcOnCcMapping)
    {
      NS_LOG_DEBUG (this << " RNTI " << itLcOnCcMapping->lc.rnti << "Lcid " << (uint16_t) itLcOnCcMapping->lc.lcId << " lcGroup " << (uint16_t) itLcOnCcMapping->lc.lcGroup << " ComponentCarrierId " << itLcOnCcMapping->componentCarrierId);
      uint8_t index = itLcOnCcMapping->componentCarrierId;
      LteEnbCmacSapProvider::LcInfo lcinfo = itLcOnCcMapping->lc;
      LteMacSapUser *msu = itLcOnCcMapping->msu;
      m_rrc->m_cmacSapProvider.at (index)->AddLc (lcinfo, msu);
      m_rrc->m_ccmRrcSapProvider->AddLc (lcinfo, msu);
    }

  if (rlcTypeId == LteRlcAm::GetTypeId ())
    {
      drbInfo->m_rlcConfig.choice =  LteRrcSap::RlcConfig::AM;
    }
  else
    {
      drbInfo->m_rlcConfig.choice =  LteRrcSap::RlcConfig::UM_BI_DIRECTIONAL;
    }

  drbInfo->m_logicalChannelIdentity = lcid;
  drbInfo->m_logicalChannelConfig.priority =  m_rrc->GetLogicalChannelPriority (bearer);
  drbInfo->m_logicalChannelConfig.logicalChannelGroup = m_rrc->GetLogicalChannelGroup (bearer);
  if (bearer.IsGbr ())
    {
      drbInfo->m_logicalChannelConfig.prioritizedBitRateKbps = bearer.gbrQosInfo.gbrUl;
    }
  else
    {
      drbInfo->m_logicalChannelConfig.prioritizedBitRateKbps = 0;
    }
  drbInfo->m_logicalChannelConfig.bucketSizeDurationMs = 1000;

  ScheduleRrcConnectionReconfiguration ();
}

void
UeManager::RecordDataRadioBearersToBeStarted ()
{
  NS_LOG_FUNCTION (this << (uint32_t) m_rnti);
  for (std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.begin ();
       it != m_drbMap.end ();
       ++it)
    {
      m_drbsToBeStarted.push_back (it->first);
    }
}

void
UeManager::StartDataRadioBearers ()
{
  NS_LOG_FUNCTION (this << (uint32_t) m_rnti);
  for (std::list <uint8_t>::iterator drbIdIt = m_drbsToBeStarted.begin ();
       drbIdIt != m_drbsToBeStarted.end ();
       ++drbIdIt)
    {
      std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator drbIt = m_drbMap.find (*drbIdIt);
      NS_ASSERT (drbIt != m_drbMap.end ());
      drbIt->second->m_rlc->Initialize ();
      if (drbIt->second->m_pdcp)
        {
          drbIt->second->m_pdcp->Initialize ();
        }
    }
  m_drbsToBeStarted.clear ();
}


void
UeManager::ReleaseDataRadioBearer (uint8_t drbid)
{
  NS_LOG_FUNCTION (this << (uint32_t) m_rnti << (uint32_t) drbid);
  uint8_t lcid = Drbid2Lcid (drbid);
  std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.find (drbid);
  NS_ASSERT_MSG (it != m_drbMap.end (), "request to remove radio bearer with unknown drbid " << drbid);

  // first delete eventual X2-U TEIDs
  m_rrc->m_x2uTeidInfoMap.erase (it->second->m_gtpTeid);

  m_drbMap.erase (it);
  std::vector<uint8_t> ccToRelease = m_rrc->m_ccmRrcSapProvider->ReleaseDataRadioBearer (m_rnti, lcid);
  std::vector<uint8_t>::iterator itCcToRelease = ccToRelease.begin ();
  NS_ASSERT_MSG (itCcToRelease != ccToRelease.end (), "request to remove radio bearer with unknown drbid (ComponentCarrierManager)");
  for (itCcToRelease = ccToRelease.begin (); itCcToRelease != ccToRelease.end (); ++itCcToRelease)
    {
      m_rrc->m_cmacSapProvider.at (*itCcToRelease)->ReleaseLc (m_rnti, lcid);
    }
  LteRrcSap::RadioResourceConfigDedicated rrcd;
  rrcd.havePhysicalConfigDedicated = false;
  rrcd.drbToReleaseList.push_back (drbid);
  //populating RadioResourceConfigDedicated information element as per 3GPP TS 36.331 version 9.2.0
  rrcd.havePhysicalConfigDedicated = true;
  rrcd.physicalConfigDedicated = m_physicalConfigDedicated;

  //populating RRCConnectionReconfiguration message as per 3GPP TS 36.331 version 9.2.0 Release 9
  LteRrcSap::RrcConnectionReconfiguration msg;
  msg.haveMeasConfig = false;
  msg.haveMobilityControlInfo = false;
  msg.radioResourceConfigDedicated = rrcd;
  msg.haveRadioResourceConfigDedicated = true;
  // ToDo: Resend in any case this configuration
  // needs to be initialized
  msg.haveNonCriticalExtension = false;
  //RRC Connection Reconfiguration towards UE
  m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration (m_rnti, msg);
}

void
LteEnbRrc::DoSendReleaseDataRadioBearer (uint64_t imsi, uint16_t rnti, uint8_t bearerId)
{
  NS_LOG_FUNCTION (this << imsi << rnti << (uint16_t) bearerId);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  // Bearer de-activation towards UE
  ueManager->ReleaseDataRadioBearer (bearerId);
  // Bearer de-activation indication towards epc-enb application
  m_s1SapProvider->DoSendReleaseIndication (imsi,rnti,bearerId);
}

void
UeManager::RecvIdealUeContextRemoveRequest (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << m_rnti);

  //release the bearer info for the UE at SGW/PGW
  if (m_rrc->m_s1SapProvider != 0) //if EPC is enabled
    {
      for (const auto &it:m_drbMap)
        {
          NS_LOG_DEBUG ("Sending release of bearer id : " << (uint16_t) (it.first)
                        << "LCID : "
                        << (uint16_t) (it.second->m_logicalChannelIdentity));
          // Bearer de-activation indication towards epc-enb application
          m_rrc->m_s1SapProvider->DoSendReleaseIndication (GetImsi (), rnti, it.first);
        }
    }
}

void
UeManager::ScheduleRrcConnectionReconfiguration ()
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
    case CONNECTION_SETUP:
    case ATTACH_REQUEST:
    case CONNECTION_RECONFIGURATION:
    case CONNECTION_REESTABLISHMENT:
    case HANDOVER_PREPARATION:
    case HANDOVER_JOINING:
    case HANDOVER_LEAVING:
      // a previous reconfiguration still ongoing, we need to wait for it to be finished
      m_pendingRrcConnectionReconfiguration = true;
      break;

    case CONNECTED_NORMALLY:
      {
        m_pendingRrcConnectionReconfiguration = false;
        LteRrcSap::RrcConnectionReconfiguration msg = BuildRrcConnectionReconfiguration ();
        m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration (m_rnti, msg);
        RecordDataRadioBearersToBeStarted ();
        SwitchToState (CONNECTION_RECONFIGURATION);
      }
      break;
    case IDLE_SUSPEND_PSM:
      break;
    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

void
UeManager::PrepareHandover (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);
  switch (m_state)
    {
    case CONNECTED_NORMALLY:
      {
        m_targetCellId = cellId;
        EpcX2SapProvider::HandoverRequestParams params;
        params.oldEnbUeX2apId = m_rnti;
        params.cause          = EpcX2SapProvider::HandoverDesirableForRadioReason;
        params.sourceCellId   = m_rrc->ComponentCarrierToCellId (m_componentCarrierId);
        params.targetCellId   = cellId;
        params.mmeUeS1apId    = m_imsi;
        params.ueAggregateMaxBitRateDownlink = 200 * 1000;
        params.ueAggregateMaxBitRateUplink = 100 * 1000;
        params.bearers = GetErabList ();

        LteRrcSap::HandoverPreparationInfo hpi;
        hpi.asConfig.sourceUeIdentity = m_rnti;
        hpi.asConfig.sourceDlCarrierFreq = m_rrc->m_dlEarfcn;
        hpi.asConfig.sourceMeasConfig = m_rrc->m_ueMeasConfig;
        hpi.asConfig.sourceRadioResourceConfig = GetRadioResourceConfigForHandoverPreparationInfo ();
        hpi.asConfig.sourceMasterInformationBlock.dlBandwidth = m_rrc->m_dlBandwidth;
        hpi.asConfig.sourceMasterInformationBlock.systemFrameNumber = 0;
        hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity = m_rrc->m_sib1.at (m_componentCarrierId).cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity;
        hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.cellIdentity = m_rrc->ComponentCarrierToCellId (m_componentCarrierId);
        hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIndication = m_rrc->m_sib1.at (m_componentCarrierId).cellAccessRelatedInfo.csgIndication;
        hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIdentity = m_rrc->m_sib1.at (m_componentCarrierId).cellAccessRelatedInfo.csgIdentity;
        LteEnbCmacSapProvider::RachConfig rc = m_rrc->m_cmacSapProvider.at (m_componentCarrierId)->GetRachConfig ();
        hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon.rachConfigCommon.preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
        hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
        hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;
        hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon.rachConfigCommon.txFailParam.connEstFailCount = rc.connEstFailCount;
        hpi.asConfig.sourceSystemInformationBlockType2.freqInfo.ulCarrierFreq = m_rrc->m_ulEarfcn;
        hpi.asConfig.sourceSystemInformationBlockType2.freqInfo.ulBandwidth = m_rrc->m_ulBandwidth;
        params.rrcContext = m_rrc->m_rrcSapUser->EncodeHandoverPreparationInformation (hpi);

        NS_LOG_LOGIC ("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
        NS_LOG_LOGIC ("sourceCellId = " << params.sourceCellId);
        NS_LOG_LOGIC ("targetCellId = " << params.targetCellId);
        NS_LOG_LOGIC ("mmeUeS1apId = " << params.mmeUeS1apId);
        NS_LOG_LOGIC ("rrcContext   = " << params.rrcContext);

        m_rrc->m_x2SapProvider->SendHandoverRequest (params);
        SwitchToState (HANDOVER_PREPARATION);
      }
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }

}

void
UeManager::RecvHandoverRequestAck (EpcX2SapUser::HandoverRequestAckParams params)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (params.notAdmittedBearers.empty (), "not admission of some bearers upon handover is not supported");
  NS_ASSERT_MSG (params.admittedBearers.size () == m_drbMap.size (), "not enough bearers in admittedBearers");

  // note: the Handover command from the target eNB to the source eNB
  // is expected to be sent transparently to the UE; however, here we
  // decode the message and eventually re-encode it. This way we can
  // support both a real RRC protocol implementation and an ideal one
  // without actual RRC protocol encoding.

  Ptr<Packet> encodedHandoverCommand = params.rrcContext;
  LteRrcSap::RrcConnectionReconfiguration handoverCommand = m_rrc->m_rrcSapUser->DecodeHandoverCommand (encodedHandoverCommand);
  if (handoverCommand.haveNonCriticalExtension)
    {
      //Total number of component carriers = handoverCommand.nonCriticalExtension.sCellsToAddModList.size() + 1 (Primary carrier)
      if (handoverCommand.nonCriticalExtension.sCellsToAddModList.size() + 1 != m_rrc->m_numberOfComponentCarriers)
        {
          //Currently handover is only possible if source and target eNBs have equal number of component carriers
          NS_FATAL_ERROR ("The source and target eNBs have unequal number of component carriers. Target eNB CCs = "
                           << handoverCommand.nonCriticalExtension.sCellsToAddModList.size() + 1
                           << " Source eNB CCs = " << m_rrc->m_numberOfComponentCarriers);
        }
    }
  m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration (m_rnti, handoverCommand);
  SwitchToState (HANDOVER_LEAVING);
  m_handoverLeavingTimeout = Simulator::Schedule (m_rrc->m_handoverLeavingTimeoutDuration,
                                                  &LteEnbRrc::HandoverLeavingTimeout,
                                                  m_rrc, m_rnti);
  NS_ASSERT (handoverCommand.haveMobilityControlInfo);
  m_rrc->m_handoverStartTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti, handoverCommand.mobilityControlInfo.targetPhysCellId);

  //Set the target cell ID and the RNTI so that handover cancel message can be sent if required
  m_targetX2apId = params.newEnbUeX2apId;
  m_targetCellId = params.targetCellId;

  EpcX2SapProvider::SnStatusTransferParams sst;
  sst.oldEnbUeX2apId = params.oldEnbUeX2apId;
  sst.newEnbUeX2apId = params.newEnbUeX2apId;
  sst.sourceCellId = params.sourceCellId;
  sst.targetCellId = params.targetCellId;
  for ( std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator drbIt = m_drbMap.begin ();
        drbIt != m_drbMap.end ();
        ++drbIt)
    {
      // SN status transfer is only for AM RLC
      if (0 != drbIt->second->m_rlc->GetObject<LteRlcAm> ())
        {
          LtePdcp::Status status = drbIt->second->m_pdcp->GetStatus ();
          EpcX2Sap::ErabsSubjectToStatusTransferItem i;
          i.dlPdcpSn = status.txSn;
          i.ulPdcpSn = status.rxSn;
          sst.erabsSubjectToStatusTransferList.push_back (i);
        }
    }
  m_rrc->m_x2SapProvider->SendSnStatusTransfer (sst);
}


LteRrcSap::RadioResourceConfigDedicated
UeManager::GetRadioResourceConfigForHandoverPreparationInfo ()
{
  NS_LOG_FUNCTION (this);
  return BuildRadioResourceConfigDedicated ();
}

LteRrcSap::RrcConnectionReconfiguration
UeManager::GetRrcConnectionReconfigurationForHandover ()
{
  NS_LOG_FUNCTION (this);
  return BuildRrcConnectionReconfiguration ();
}

void
UeManager::SendPacket (uint8_t bid, Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << (uint16_t) bid);
  LtePdcpSapProvider::TransmitPdcpSduParameters params;
  params.pdcpSdu = p;
  params.rnti = m_rnti;
  params.lcid = Bid2Lcid (bid);
  uint8_t drbid = Bid2Drbid (bid);
  // Transmit PDCP sdu only if DRB ID found in drbMap
  std::map<uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.find (drbid);
  if (it != m_drbMap.end ())
    {
      Ptr<LteDataRadioBearerInfo> bearerInfo = GetDataRadioBearerInfo (drbid);
      if (bearerInfo != NULL)
        {
          LtePdcpSapProvider* pdcpSapProvider = bearerInfo->m_pdcp->GetLtePdcpSapProvider ();
          pdcpSapProvider->TransmitPdcpSdu (params);
        }
    }
}

void
UeManager::SendData (uint8_t bid, Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << (uint16_t) bid);
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
    case CONNECTION_SETUP:
      NS_LOG_WARN ("not connected, discarding packet");
      return;
      break;

    case CONNECTED_NORMALLY:

    case CONNECTION_RECONFIGURATION:
    case CONNECTION_REESTABLISHMENT:
    case HANDOVER_PREPARATION:
    case HANDOVER_PATH_SWITCH:
    case IDLE_SUSPEND_EDRX:
    case CONNECTION_RESUME:
    case IDLE_SUSPEND_PSM: // This is not correct, but will stay until paging is implemented
      {
        NS_LOG_LOGIC ("queueing data on PDCP for transmission over the air");
        SendPacket (bid, p);
      }
      break;
    case IDLE_EARLY_DATA_TRANSMISSION:
      {
        // Answer message for the early data transmission

        // send RRC Early Data Transmission Complete to UE
        // For now, we assume that the meesage will definitely fit into dedicated nas => TBS < 680
        NbIotRrcSap::RrcEarlyDataCompleteNb msg;
        msg.dedicatedInfoNas = p;
        // Contention resolution (EDT, TS 36.321 5.1.5): echo the winner's IMSI. Several UEs
        // can share a captured Temp C-RNTI and all send data in Msg3; only the one whose
        // IMSI matches accepts this completion, the others re-RACH (their data was NOT
        // forwarded). m_imsi here is the winner (set in RecvRrcEarlyDataRequestNb).
        msg.contentionResolutionId = m_imsi;

        m_rrc->m_rrcSapUser->SendRrcEarlyDataCompleteNb (m_rnti, msg);

      }
    case HANDOVER_JOINING:
      {
        // Buffer data until RRC Connection Reconfiguration Complete message is received
        NS_LOG_LOGIC ("buffering data");
        m_packetBuffer.push_back (std::make_pair (bid, p));
      }
      break;

    case HANDOVER_LEAVING:
      {
        NS_LOG_LOGIC ("forwarding data to target eNB over X2-U");
        uint8_t drbid = Bid2Drbid (bid);
        EpcX2Sap::UeDataParams params;
        params.sourceCellId = m_rrc->ComponentCarrierToCellId (m_componentCarrierId);
        params.targetCellId = m_targetCellId;
        params.gtpTeid = GetDataRadioBearerInfo (drbid)->m_gtpTeid;
        params.ueData = p;
        m_rrc->m_x2SapProvider->SendUeData (params);
      }
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

std::vector<EpcX2Sap::ErabToBeSetupItem>
UeManager::GetErabList ()
{
  NS_LOG_FUNCTION (this);
  std::vector<EpcX2Sap::ErabToBeSetupItem> ret;
  for (std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it =  m_drbMap.begin ();
       it != m_drbMap.end ();
       ++it)
    {
      EpcX2Sap::ErabToBeSetupItem etbsi;
      etbsi.erabId = it->second->m_epsBearerIdentity;
      etbsi.erabLevelQosParameters = it->second->m_epsBearer;
      etbsi.dlForwarding = false;
      etbsi.transportLayerAddress = it->second->m_transportLayerAddress;
      etbsi.gtpTeid = it->second->m_gtpTeid;
      ret.push_back (etbsi);
    }
  return ret;
}

void
UeManager::SendUeContextRelease ()
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case HANDOVER_PATH_SWITCH:
      NS_LOG_INFO ("Send UE CONTEXT RELEASE from target eNB to source eNB");
      EpcX2SapProvider::UeContextReleaseParams ueCtxReleaseParams;
      ueCtxReleaseParams.oldEnbUeX2apId = m_sourceX2apId;
      ueCtxReleaseParams.newEnbUeX2apId = m_rnti;
      ueCtxReleaseParams.sourceCellId = m_sourceCellId;
      ueCtxReleaseParams.targetCellId = m_targetCellId;
      m_rrc->m_x2SapProvider->SendUeContextRelease (ueCtxReleaseParams);
      SwitchToState (CONNECTED_NORMALLY);
      m_rrc->m_handoverEndOkTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti);
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

void
UeManager::RecvHandoverPreparationFailure (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);
  switch (m_state)
    {
    case HANDOVER_PREPARATION:
      NS_ASSERT (cellId == m_targetCellId);
      NS_LOG_INFO ("target eNB sent HO preparation failure, aborting HO");
      SwitchToState (CONNECTED_NORMALLY);
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

void
UeManager::RecvSnStatusTransfer (EpcX2SapUser::SnStatusTransferParams params)
{
  NS_LOG_FUNCTION (this);
  for (std::vector<EpcX2Sap::ErabsSubjectToStatusTransferItem>::iterator erabIt
         = params.erabsSubjectToStatusTransferList.begin ();
       erabIt != params.erabsSubjectToStatusTransferList.end ();
       ++erabIt)
    {
      // LtePdcp::Status status;
      // status.txSn = erabIt->dlPdcpSn;
      // status.rxSn = erabIt->ulPdcpSn;
      // uint8_t drbId = Bid2Drbid (erabIt->erabId);
      // std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator drbIt = m_drbMap.find (drbId);
      // NS_ASSERT_MSG (drbIt != m_drbMap.end (), "could not find DRBID " << (uint32_t) drbId);
      // drbIt->second->m_pdcp->SetStatus (status);
    }
}

void
UeManager::RecvUeContextRelease (EpcX2SapUser::UeContextReleaseParams params)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (m_state == HANDOVER_LEAVING, "method unexpected in state " << ToString (m_state));
  m_handoverLeavingTimeout.Cancel ();
}


// methods forwarded from RRC SAP

void
UeManager::CompleteSetupUe (LteEnbRrcSapProvider::CompleteSetupUeParameters params)
{
  NS_LOG_FUNCTION (this);
  m_srb0->m_rlc->SetLteRlcSapUser (params.srb0SapUser);
  m_srb1->m_pdcp->SetLtePdcpSapUser (params.srb1SapUser);
}

void
UeManager::RecvRrcConnectionRequest (LteRrcSap::RrcConnectionRequest msg)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
      {
        m_connectionRequestTimeout.Cancel ();

        if (m_rrc->m_admitRrcConnectionRequest == true)
          {
            m_imsi = msg.ueIdentity;
            // send RRC CONNECTION SETUP to UE
            LteRrcSap::RrcConnectionSetup msg2;
            msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
            msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();
            // Contention resolution (TS 36.321 5.1.5): echo the IMSI of THIS (the
            // first, i.e. winning) Msg3. If several UEs shared the Temporary
            // C-RNTI, only the one whose IMSI matches accepts Msg4; the others
            // discard it and re-RACH on T300. Duplicate requests land in the
            // CONNECTION_SETUP case below and are ignored.
            msg2.contentionResolutionId = m_imsi;

            m_rrc->m_rrcSapUser->SendRrcConnectionSetup (m_rnti, msg2);

            RecordDataRadioBearersToBeStarted ();
            m_connectionSetupTimeout = Simulator::Schedule (
                m_rrc->m_connectionSetupTimeoutDuration,
                &LteEnbRrc::ConnectionSetupTimeout, m_rrc, m_rnti);
            SwitchToState (CONNECTION_SETUP);
          }
        else
          {
            NS_LOG_INFO ("rejecting connection request for RNTI " << m_rnti);

            // send RRC CONNECTION REJECT to UE
            LteRrcSap::RrcConnectionReject rejectMsg;
            rejectMsg.waitTime = 3;
            m_rrc->m_rrcSapUser->SendRrcConnectionReject (m_rnti, rejectMsg);

            m_connectionRejectedTimeout = Simulator::Schedule (
                m_rrc->m_connectionRejectedTimeoutDuration,
                &LteEnbRrc::ConnectionRejectedTimeout, m_rrc, m_rnti);
            SwitchToState (CONNECTION_REJECTED);
          }
      }
      break;
    case CONNECTION_RESUME:
    case CONNECTION_SETUP:
      break;
    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

uint64_t
UeManager::AttachSuspendedNb(uint32_t imsi){

  // Fast forward Connection Setup StateMachine

  // Receiving MSG 3 and Sending MSG4

  //LteRrcSap::RrcConnectionSetup msg2;
  //msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
  //msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();

  //m_rrc->m_rrcSapUser->SendRrcConnectionSetup (m_rnti, msg2);

  m_imsi = imsi;
  NS_LOG_DEBUG ("UeManager::AttachSuspendedNb IMSI=" << m_imsi
                << " RNTI=" << m_rnti
                << " PG=" << m_persistentGrant);
  RecordDataRadioBearersToBeStarted ();
  if (m_rrc->m_s1SapProvider != 0)
      {
        Simulator::Schedule(MicroSeconds(500), &EpcEnbS1SapProvider::InitialUeMessage,m_rrc->m_s1SapProvider,m_imsi,m_rnti);
      }
  m_pendingRrcConnectionReconfiguration = false;
  StartDataRadioBearers();
  m_resumeId = m_rrc->DoAllocateTemporaryResumeId();
  SwitchToState(IDLE_SUSPEND_PSM);
  NS_LOG_DEBUG ("UeManager::AttachSuspendedNb IMSI=" << m_imsi
                << " RNTI=" << m_rnti
                << " resumeId=" << m_resumeId
                << " -> MoveUeToResumed in 10ms");
  Simulator::Schedule(MilliSeconds(10), &LteEnbRrc::MoveUeToResumed, m_rrc, m_rnti,m_resumeId);
  return m_resumeId;
}

void
UeManager::RecvRrcConnectionResumeRequestNb (NbIotRrcSap::RrcConnectionResumeRequestNb msg)
{
  NS_LOG_FUNCTION (this);
  if (NbIotDebugTrace ())
    std::cout << "[ENB-RESUME-REQ] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
              << " imsi=" << m_imsi << " state=" << ToString (m_state)
              << " reqResumeId=" << msg.resumeIdentity << std::endl;
  //NS_BUILD_DEBUG(std::cout << "\n"<< m_rnti << "GOT THROUGH" << std::endl);
  switch (m_state)
    {
    case IDLE_SUSPEND_PSM:
    case IDLE_SUSPEND_EDRX:
      {
        m_connectionRequestTimeout.Cancel ();

        if (m_rrc->m_admitRrcConnectionResumeRequest)
          {
              NbIotRrcSap::RrcConnectionResumeNb msg2;
              msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
              // Contention resolution (resume, TS 36.321 5.1.5): echo THIS (winning) Msg3's
              // resumeId. Several UEs can share a captured Temp C-RNTI; the reject guard in
              // DoRecvRrcConnectionResumeRequestNb ensures only this first resumer is served,
              // and only the UE whose resumeId matches accepts Msg4 -- the others re-RACH.
              msg2.resumeIdentity = msg.resumeIdentity;
              m_srb0->m_rlc->SetRnti(m_rnti);
              m_srb1->m_pdcp->SetRnti(m_rnti);
              m_srb1->m_rlc->SetRnti(m_rnti);
              for(std::map<uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it =   m_drbMap.begin(); it != m_drbMap.end(); ++it){
                it->second->m_pdcp->SetRnti(m_rnti);
                it->second->m_rlc->SetRnti(m_rnti);
              }
              // UP-EDT: the UE folded its early uplink data into this resume Msg3. The eNB has
              // it now, so deliver it upstream immediately (the UE will NOT repeat it in Msg5).
              // Empty for a normal (non-EDT) resume -> legacy flow untouched.
              if (msg.dedicatedInfoNas != nullptr && msg.dedicatedInfoNas->GetSize () > 0
                  && m_rrc->m_edtDataForwarded.find (msg.resumeIdentity) == m_rrc->m_edtDataForwarded.end ())
                {
                  // Forward the folded UP-EDT data upstream ONCE per resumeId. A capture loser
                  // whose resume stalled re-RACHes and re-sends the SAME data folded into the
                  // retried Msg3; without this dedupe the eNB delivers it again -> the server
                  // counts it twice (recv>sent, corrupting the loss metric). resumeId is unique
                  // per packet (a new one is allocated at each release), so this dedupes per
                  // packet, not per UE. Cleared on resume-complete and on id reallocation.
                  m_rrc->m_edtDataForwarded.insert (msg.resumeIdentity);
                  if (NbIotDebugTrace ())
                    {
                      uint8_t b0 = 0; msg.dedicatedInfoNas->CopyData (&b0, 1);
                      std::cout << "[ENB-UPEDT-DATA] rnti=" << m_rnti << " imsi=" << m_imsi
                                << " size=" << msg.dedicatedInfoNas->GetSize ()
                                << " firstByteHi=" << (uint32_t)(b0 >> 4) << std::endl;
                    }
                  EpsBearerTag tag;
                  tag.SetRnti (m_rnti);
                  tag.SetBid (Lcid2Bid (3));
                  msg.dedicatedInfoNas->AddPacketTag (tag);
                  m_rrc->LogDataReception (m_imsi, msg.dedicatedInfoNas->GetSize ());
                  m_rrc->m_forwardUpCallback (msg.dedicatedInfoNas);
                }
              else if (msg.dedicatedInfoNas != nullptr && msg.dedicatedInfoNas->GetSize () > 0
                       && NbIotDebugTrace ())
                {
                  std::cout << "[ENB-UPEDT-DUP] rnti=" << m_rnti << " imsi=" << m_imsi
                            << " resumeId=" << msg.resumeIdentity
                            << " already forwarded -> dropping duplicate EDT data" << std::endl;
                }
              //m_rrc->m_rrcSapUser->ResumeUe(m_rnti, m_resumeId);
              m_rrc->m_rrcSapUser->SendRrcConnectionResumeNb (m_rnti, msg2);
              RecordDataRadioBearersToBeStarted ();
              m_connectionResumeTimeout = Simulator::Schedule (
                  m_rrc->m_connectionResumeTimeoutDuration,
                  &LteEnbRrc::ConnectionResumeTimeout, m_rrc, m_rnti);
              SwitchToState (CONNECTION_RESUME);

            }
        else if (m_rrc->m_admitRrcConnectionRequest)
            {
              //m_imsi = msg.ueIdentity;
              LteRrcSap::RrcConnectionSetup msg2;
              msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
              msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();
              m_rrc->m_rrcSapUser->SendRrcConnectionSetup (m_rnti, msg2);

              RecordDataRadioBearersToBeStarted ();
              m_connectionSetupTimeout = Simulator::Schedule (
                  m_rrc->m_connectionSetupTimeoutDuration,
                  &LteEnbRrc::ConnectionSetupTimeout, m_rrc, m_rnti);
              SwitchToState (CONNECTION_SETUP);


            }
        else
        {
            NS_LOG_INFO ("rejecting connection request for RNTI " << m_rnti);

            // send RRC CONNECTION REJECT to UE
            LteRrcSap::RrcConnectionReject rejectMsg;
            rejectMsg.waitTime = 3;
            m_rrc->m_rrcSapUser->SendRrcConnectionReject (m_rnti, rejectMsg);

            m_connectionRejectedTimeout = Simulator::Schedule (
                m_rrc->m_connectionRejectedTimeoutDuration,
                &LteEnbRrc::ConnectionRejectedTimeout, m_rrc, m_rnti);
            SwitchToState (CONNECTION_REJECTED);
          }
      }
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

void
UeManager::RecvRrcEarlyDataRequestNb (NbIotRrcSap::RrcEarlyDataRequestNb msg)
{
  NS_LOG_FUNCTION (this);
  if (NbIotDebugTrace ())
    std::cout << "[ENB-EDT-REQ] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
              << " imsi=" << m_imsi << " state=" << ToString (m_state)
              << " sTmsi=" << msg.sTmsiNb.mTmsi << std::endl;
  //NS_BUILD_DEBUG(std::cout << "\n"<< m_rnti << "GOT THROUGH" << std::endl);
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
      {
        m_connectionRequestTimeout.Cancel ();

        if (m_rrc->m_edt)
          {
              m_imsi = msg.sTmsiNb.mTmsi;
              m_rrc->m_s1SapProvider->InitialUeMessage(m_imsi, m_rnti);
              // std::cout << "RecvRrcEarlyDataRequestNb: IMSI" << m_imsi << " RNTI " << m_rnti << std::endl;

              SwitchToState (IDLE_EARLY_DATA_TRANSMISSION);
              if(msg.dedicatedInfoNas->GetSize()>0){
                // data radio bearer
                EpsBearerTag tag;
                tag.SetRnti (m_rnti);
                tag.SetBid (Lcid2Bid (3));
                msg.dedicatedInfoNas->AddPacketTag (tag);

                m_rrc->LogDataReception(m_imsi, msg.dedicatedInfoNas->GetSize());

                m_rrc->m_forwardUpCallback (msg.dedicatedInfoNas);
              }

              // Reply Msg4 (RRCEarlyDataComplete) NOW to acknowledge Msg3 and release the UE
              // to idle. 3GPP TS 36.321 5.1: the eNB sends Msg4 in response to Msg3; it does
              // NOT wait for downlink data. For MO-data (UL-only) there is no DL response, so
              // without this the UE never receives Msg4 -> T300 timeout -> redundant re-RACH
              // (observed as the spurious non-EDT retry). Echo the winner IMSI so colliding
              // losers detect the mismatch and re-RACH (contention resolution). (The existing
              // DL-data path still delivers a later MT DL packet via its own completion.)
              NbIotRrcSap::RrcEarlyDataCompleteNb edcMsg;
              edcMsg.dedicatedInfoNas = Create<Packet> (0);
              edcMsg.contentionResolutionId = m_imsi;
              m_rrc->m_rrcSapUser->SendRrcEarlyDataCompleteNb (m_rnti, edcMsg);
            }
        else if (m_rrc->m_admitRrcConnectionRequest)
            {
              //m_imsi = msg.ueIdentity;
              LteRrcSap::RrcConnectionSetup msg2;
              msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
              msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();
              m_rrc->m_rrcSapUser->SendRrcConnectionSetup (m_rnti, msg2);

              RecordDataRadioBearersToBeStarted ();
              m_connectionSetupTimeout = Simulator::Schedule (
                  m_rrc->m_connectionSetupTimeoutDuration,
                  &LteEnbRrc::ConnectionSetupTimeout, m_rrc, m_rnti);
              SwitchToState (CONNECTION_SETUP);

            }
        else
        {
            NS_LOG_INFO ("rejecting connection request for RNTI " << m_rnti);

            // send RRC CONNECTION REJECT to UE
            LteRrcSap::RrcConnectionReject rejectMsg;
            rejectMsg.waitTime = 3;
            m_rrc->m_rrcSapUser->SendRrcConnectionReject (m_rnti, rejectMsg);

            m_connectionRejectedTimeout = Simulator::Schedule (
                m_rrc->m_connectionRejectedTimeoutDuration,
                &LteEnbRrc::ConnectionRejectedTimeout, m_rrc, m_rnti);
            SwitchToState (CONNECTION_REJECTED);

          }
      }
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}




void
UeManager::RecvRrcConnectionSetupCompleted (LteRrcSap::RrcConnectionSetupCompleted msg)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case CONNECTION_SETUP:
      m_rrc->m_cmacSapProvider.at(0)->NotifyConnectionSuccessful(m_rnti);
      m_connectionSetupTimeout.Cancel ();
      if ( m_caSupportConfigured == false && m_rrc->m_numberOfComponentCarriers > 1)
        {
          m_pendingRrcConnectionReconfiguration = true; // Force Reconfiguration
          m_pendingStartDataRadioBearers = true;
        }

      if (m_rrc->m_s1SapProvider != 0)
        {
          m_rrc->m_s1SapProvider->InitialUeMessage (m_imsi, m_rnti);
          SwitchToState (ATTACH_REQUEST);
        }
      else
        {
          SwitchToState (CONNECTED_NORMALLY);
        }
      // Pull NB-IoT timer values from the LteEnbRrc attributes so
      m_t3324     = MilliSeconds (m_rrc->m_t3324);    // Multiple of 10.24 up-to 1090*10.24
      m_t3412     = MilliSeconds (m_rrc->m_t3412);
      m_eDrxCycle = MilliSeconds (m_rrc->m_eDrxCycle);   // ptw ranges from 2.56 to 40.96 (2.56/16)

      m_rrc->m_connectionEstablishedTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti);
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}
void
UeManager::RecvRrcConnectionResumeCompletedNb (NbIotRrcSap::RrcConnectionResumeCompleteNb msg)
{
  NS_LOG_FUNCTION (this);
  if (NbIotDebugTrace ())
    std::cout << "[ENB-RESUME-COMPLETE] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
              << " imsi=" << m_imsi << " state=" << ToString (m_state) << std::endl;
  switch (m_state)
    {
    case CONNECTION_RESUME:
      {
      NS_LOG_DEBUG ("UeManager::RecvRrcConnectionResumeCompletedNb"
                    << " IMSI=" << m_imsi << " RNTI=" << m_rnti);
      // Resume CONFIRMED -> now consume the single-use resumeId token. ResumeUe no longer
      // erases it eagerly (deferred-erase), so a capture loser whose Msg4 didn't land kept
      // a valid token to re-RACH on. Erase here so a genuinely completed resume can't be
      // replayed and the id can be reallocated.
      m_rrc->m_ueResumedMap.erase (m_resumeId);
      // Was this a UP-EDT delivery? m_edtDataForwarded is keyed on resumeId (unique per UE-per-
      // packet), and ONLY the Msg4 contention WINNER reaches resume-complete (losers hit
      // RESUME-LOST and never get here). So this check is contention-correct by construction:
      // it is true iff THIS winner's folded Msg3 data was forwarded upstream. Captured BEFORE
      // the erase below; used at the end to arm the single-shot EDT release for the winner only.
      bool wasEdtDelivery = (m_rrc->m_edtDataForwarded.count (m_resumeId) > 0);
      m_rrc->m_edtDataForwarded.erase (m_resumeId); // resume done -> clear EDT-forwarded flag for this id
      m_rrc->m_cmacSapProvider.at(0)->NotifyConnectionSuccessful(m_rnti);
      m_connectionResumeTimeout.Cancel ();
      m_rrc->SendSavedPackets(m_imsi, m_rnti);
      if (m_rrc->m_s1SapProvider != 0)
        {
          m_rrc->m_s1SapProvider->InitialUeMessage (m_imsi, m_rnti);
          SwitchToState (CONNECTED_NORMALLY);
          m_persistentGrant = m_rrc->IsPersistentGrant();
          NS_LOG_DEBUG ("UeManager::RecvRrcConnectionResumeCompletedNb"
                        << " RNTI=" << m_rnti
                        << " -> CONNECTED_NORMALLY, PG armed=" << m_persistentGrant);
          if (msg.dedicatedInfoNas->GetSize() > 0){
            EpsBearerTag tag;
            tag.SetRnti (m_rnti);
            tag.SetBid (Lcid2Bid (3));
            msg.dedicatedInfoNas->AddPacketTag (tag);
            m_rrc->LogDataReception(m_imsi, msg.dedicatedInfoNas->GetSize());
            m_rrc->m_forwardUpCallback (msg.dedicatedInfoNas);
          }
        }
      else
        {
          SwitchToState (CONNECTED_NORMALLY);
          m_persistentGrant = m_rrc->IsPersistentGrant();
          NS_LOG_DEBUG ("UeManager::RecvRrcConnectionResumeCompletedNb"
                        << " RNTI=" << m_rnti
                        << " -> CONNECTED_NORMALLY, PG armed=" << m_persistentGrant);
        }
      m_rrc->m_connectionEstablishedTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti);
      // UP-EDT single-shot release (winner only -- see wasEdtDelivery above): the folded Msg3 data
      // was already forwarded upstream, so there is no post-resume DRB data flow to schedule the
      // data-inactivity release the normal way. Arm it explicitly now (state is CONNECTED_NORMALLY
      // here) so this EDT winner is released after the usual inactivity timeout instead of lingering
      // CONNECTED (and stranding its next packet as an ungrantable connected-send). Not for cdrxFug
      // (idealfug stays connected by design). Losers never reach this case, so they never arm it.
      if (wasEdtDelivery && !m_cdrxFug)
        {
          if (NbIotDebugTrace ())
            std::cout << "[ENB-EDT-ARM-RELEASE] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
                      << " imsi=" << m_imsi << " (arming inactivity release after UP-EDT)" << std::endl;
          NotifyDataInactivitySchedulerNb ();
        }
      break;
      }

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}
void
UeManager::RecvRrcConnectionReconfigurationCompleted (LteRrcSap::RrcConnectionReconfigurationCompleted msg)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case CONNECTION_RECONFIGURATION:
      StartDataRadioBearers ();
      if (m_needPhyMacConfiguration)
        {
          // configure MAC (and scheduler)
          LteEnbCmacSapProvider::UeConfig req;
          req.m_rnti = m_rnti;
          req.m_transmissionMode = m_physicalConfigDedicated.antennaInfo.transmissionMode;
          for (uint8_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
            {
              m_rrc->m_cmacSapProvider.at (i)->UeUpdateConfigurationReq (req);

              // configure PHY
              m_rrc->m_cphySapProvider.at (i)->SetTransmissionMode (req.m_rnti, req.m_transmissionMode);
              double paDouble = LteRrcSap::ConvertPdschConfigDedicated2Double (m_physicalConfigDedicated.pdschConfigDedicated);
              m_rrc->m_cphySapProvider.at (i)->SetPa (m_rnti, paDouble);
            }

          m_needPhyMacConfiguration = false;
        }
      SwitchToState (CONNECTED_NORMALLY);
      m_rrc->m_connectionReconfigurationTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti);
      break;

    // This case is added to NS-3 in order to handle bearer de-activation scenario for CONNECTED state UE
    case CONNECTED_NORMALLY:
      NS_LOG_INFO ("ignoring RecvRrcConnectionReconfigurationCompleted in state " << ToString (m_state));
      break;

    case HANDOVER_LEAVING:
      NS_LOG_INFO ("ignoring RecvRrcConnectionReconfigurationCompleted in state " << ToString (m_state));
      break;

    case HANDOVER_JOINING:
      {
        m_handoverJoiningTimeout.Cancel ();

        while (!m_packetBuffer.empty ())
          {
            NS_LOG_LOGIC ("dequeueing data from buffer");
            std::pair <uint8_t, Ptr<Packet> > bidPacket = m_packetBuffer.front ();
            uint8_t bid = bidPacket.first;
            Ptr<Packet> p = bidPacket.second;

            NS_LOG_LOGIC ("queueing data on PDCP for transmission over the air");
            SendPacket (bid, p);

            m_packetBuffer.pop_front ();
          }

        NS_LOG_INFO ("Send PATH SWITCH REQUEST to the MME");
        EpcEnbS1SapProvider::PathSwitchRequestParameters params;
        params.rnti = m_rnti;
        params.cellId = m_rrc->ComponentCarrierToCellId (m_componentCarrierId);
        params.mmeUeS1Id = m_imsi;
        SwitchToState (HANDOVER_PATH_SWITCH);
        for (std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it =  m_drbMap.begin ();
             it != m_drbMap.end ();
             ++it)
          {
            EpcEnbS1SapProvider::BearerToBeSwitched b;
            b.epsBearerId = it->second->m_epsBearerIdentity;
            b.teid =  it->second->m_gtpTeid;
            params.bearersToBeSwitched.push_back (b);
          }
        m_rrc->m_s1SapProvider->PathSwitchRequest (params);
      }
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }
}

void
UeManager::RecvRrcConnectionReestablishmentRequest (LteRrcSap::RrcConnectionReestablishmentRequest msg)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case CONNECTED_NORMALLY:
      break;

    case HANDOVER_LEAVING:
      m_handoverLeavingTimeout.Cancel ();
      break;

    default:
      NS_FATAL_ERROR ("method unexpected in state " << ToString (m_state));
      break;
    }

  LteRrcSap::RrcConnectionReestablishment msg2;
  msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
  msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();
  m_rrc->m_rrcSapUser->SendRrcConnectionReestablishment (m_rnti, msg2);
  SwitchToState (CONNECTION_REESTABLISHMENT);
}

void
UeManager::RecvRrcConnectionReestablishmentComplete (LteRrcSap::RrcConnectionReestablishmentComplete msg)
{
  NS_LOG_FUNCTION (this);
  SwitchToState (CONNECTED_NORMALLY);
}

void
UeManager::RecvMeasurementReport (LteRrcSap::MeasurementReport msg)
{
  uint8_t measId = msg.measResults.measId;
  NS_LOG_FUNCTION (this << (uint16_t) measId);
  NS_LOG_LOGIC ("measId " << (uint16_t) measId
                          << " haveMeasResultNeighCells " << msg.measResults.haveMeasResultNeighCells
                          << " measResultListEutra " << msg.measResults.measResultListEutra.size ()
                          << " haveScellsMeas " << msg.measResults.haveScellsMeas
                          << " measScellResultList " << msg.measResults.measScellResultList.measResultScell.size ());
  NS_LOG_LOGIC ("serving cellId " << m_rrc->ComponentCarrierToCellId (m_componentCarrierId)
                                  << " RSRP " << (uint16_t) msg.measResults.rsrpResult
                                  << " RSRQ " << (uint16_t) msg.measResults.rsrqResult);

  for (std::list <LteRrcSap::MeasResultEutra>::iterator it = msg.measResults.measResultListEutra.begin ();
       it != msg.measResults.measResultListEutra.end ();
       ++it)
    {
      NS_LOG_LOGIC ("neighbour cellId " << it->physCellId
                                        << " RSRP " << (it->haveRsrpResult ? (uint16_t) it->rsrpResult : 255)
                                        << " RSRQ " << (it->haveRsrqResult ? (uint16_t) it->rsrqResult : 255));
    }

  if ((m_rrc->m_handoverManagementSapProvider != 0)
      && (m_rrc->m_handoverMeasIds.find (measId) != m_rrc->m_handoverMeasIds.end ()))
    {
      // this measurement was requested by the handover algorithm
      m_rrc->m_handoverManagementSapProvider->ReportUeMeas (m_rnti,
                                                            msg.measResults);
    }

  if ((m_rrc->m_ccmRrcSapProvider != 0)
      && (m_rrc->m_componentCarrierMeasIds.find (measId) != m_rrc->m_componentCarrierMeasIds.end ()))
    {
      // this measurement was requested by the handover algorithm
      m_rrc->m_ccmRrcSapProvider->ReportUeMeas (m_rnti,
                                                msg.measResults);
    }

  if ((m_rrc->m_anrSapProvider != 0)
      && (m_rrc->m_anrMeasIds.find (measId) != m_rrc->m_anrMeasIds.end ()))
    {
      // this measurement was requested by the ANR function
      m_rrc->m_anrSapProvider->ReportUeMeas (msg.measResults);
    }

  if ((m_rrc->m_ffrRrcSapProvider.size () > 0)
      && (m_rrc->m_ffrMeasIds.find (measId) != m_rrc->m_ffrMeasIds.end ()))
    {
      // this measurement was requested by the FFR function
      m_rrc->m_ffrRrcSapProvider.at (0)->ReportUeMeas (m_rnti, msg.measResults);
    }
  if (msg.measResults.haveScellsMeas == true)
    {
      for (std::list <LteRrcSap::MeasResultScell>::iterator it = msg.measResults.measScellResultList.measResultScell.begin ();
           it != msg.measResults.measScellResultList.measResultScell.end ();
           ++it)
        {
          m_rrc->m_ffrRrcSapProvider.at (it->servFreqId)->ReportUeMeas (m_rnti, msg.measResults);
          /// ToDo: implement on Ffr algorithm the code to properly parsing the new measResults message format
          /// alternatively it is needed to 'repack' properly the measResults message before sending to Ffr
        }
    }

  ///Report any measurements to ComponentCarrierManager, so it can react to any change or activate the SCC
  m_rrc->m_ccmRrcSapProvider->ReportUeMeas (m_rnti, msg.measResults);
  // fire a trace source
  m_rrc->m_recvMeasurementReportTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti, msg);

} // end of UeManager::RecvMeasurementReport


// methods forwarded from CMAC SAP

void
UeManager::CmacUeConfigUpdateInd (LteEnbCmacSapUser::UeConfig cmacParams)
{
  NS_LOG_FUNCTION (this << m_rnti);
  // at this stage used only by the scheduler for updating txMode

  m_physicalConfigDedicated.antennaInfo.transmissionMode = cmacParams.m_transmissionMode;

  m_needPhyMacConfiguration = true;

  // reconfigure the UE RRC
  ScheduleRrcConnectionReconfiguration ();
}


// methods forwarded from PDCP SAP

void
UeManager::DoReceivePdcpSdu (LtePdcpSapUser::ReceivePdcpSduParameters params)
{
  NS_LOG_FUNCTION (this);

  if (params.lcid > 2)
    {
      // data radio bearer
      EpsBearerTag tag;
      tag.SetRnti (params.rnti);
      tag.SetBid (Lcid2Bid (params.lcid));
      params.pdcpSdu->AddPacketTag (tag);
      m_rrc->LogDataReception(m_imsi, params.pdcpSdu->GetSize());
      if (NbIotDebugTrace ())
        std::cout << "[ENB-FWD-UP] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
                  << " imsi=" << m_imsi << " lcid=" << (uint32_t) params.lcid
                  << " size=" << params.pdcpSdu->GetSize () << " (DRB data -> server)" << std::endl;
      m_rrc->m_forwardUpCallback (params.pdcpSdu);

      if (m_persistentGrant && !m_proactiveFug &&
          (m_state == IDLE_SUSPEND_EDRX || m_state == IDLE_SUSPEND_PSM || m_state == CONNECTED_TAU))
        {
          if (NbIotDebugTrace ())
            std::cout << "[ENB-DATA-WAKE] t=" << Simulator::Now ().GetSeconds ()
                      << " rnti=" << m_rnti << " imsi=" << m_imsi
                      << " (DRB SDU forwarded while manager parked -> wake, grace bypassed)"
                      << std::endl;
          WakeFromPersistentGrant ();
          NotifyDataInactivitySchedulerNb ();
        }
    }
}


uint16_t
UeManager::GetRnti (void) const
{
  return m_rnti;
}

uint64_t
UeManager::GetImsi (void) const
{
  return m_imsi;
}

uint64_t
UeManager::GetResumeId (void) const
{
  return m_resumeId;
}
uint8_t
UeManager::GetComponentCarrierId () const
{
  return m_componentCarrierId;
}

uint16_t
UeManager::GetSrsConfigurationIndex (void) const
{
  return m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex;
}

void
UeManager::SetSrsConfigurationIndex (uint16_t srsConfIndex)
{
  NS_LOG_FUNCTION (this);
  m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex = srsConfIndex;
  for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
      m_rrc->m_cphySapProvider.at (i)->SetSrsConfigurationIndex (m_rnti, srsConfIndex);
    }
  switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
      // do nothing, srs conf index will be correctly enforced upon
      // RRC connection establishment
      break;

    default:
      ScheduleRrcConnectionReconfiguration ();
      break;
    }
}

UeManager::State
UeManager::GetState (void) const
{
  return m_state;
}

void
UeManager::SetPdschConfigDedicated (LteRrcSap::PdschConfigDedicated pdschConfigDedicated)
{
  NS_LOG_FUNCTION (this);
  m_physicalConfigDedicated.pdschConfigDedicated = pdschConfigDedicated;

  m_needPhyMacConfiguration = true;

  // reconfigure the UE RRC
  ScheduleRrcConnectionReconfiguration ();
}

void
UeManager::CancelPendingEvents ()
{
  NS_LOG_FUNCTION (this);
  m_connectionRequestTimeout.Cancel ();
  m_connectionRejectedTimeout.Cancel ();
  m_connectionSetupTimeout.Cancel ();
  m_handoverJoiningTimeout.Cancel ();
  m_handoverLeavingTimeout.Cancel ();
}

uint8_t
UeManager::AddDataRadioBearerInfo (Ptr<LteDataRadioBearerInfo> drbInfo)
{
  NS_LOG_FUNCTION (this);
  const uint8_t MAX_DRB_ID = 32;
  for (int drbid = (m_lastAllocatedDrbid + 1) % MAX_DRB_ID;
       drbid != m_lastAllocatedDrbid;
       drbid = (drbid + 1) % MAX_DRB_ID)
    {
      if (drbid != 0) // 0 is not allowed
        {
          if (m_drbMap.find (drbid) == m_drbMap.end ())
            {
              m_drbMap.insert (std::pair<uint8_t, Ptr<LteDataRadioBearerInfo> > (drbid, drbInfo));
              drbInfo->m_drbIdentity = drbid;
              m_lastAllocatedDrbid = drbid;
              return drbid;
            }
        }
    }
  NS_FATAL_ERROR ("no more data radio bearer ids available");
  return 0;
}

Ptr<LteDataRadioBearerInfo>
UeManager::GetDataRadioBearerInfo (uint8_t drbid)
{
  NS_LOG_FUNCTION (this << (uint32_t) drbid);
  NS_ASSERT (0 != drbid);
  std::map<uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.find (drbid);
  NS_ABORT_IF (it == m_drbMap.end ());
  return it->second;
}


void
UeManager::RemoveDataRadioBearerInfo (uint8_t drbid)
{
  NS_LOG_FUNCTION (this << (uint32_t) drbid);
  std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.find (drbid);
  NS_ASSERT_MSG (it != m_drbMap.end (), "request to remove radio bearer with unknown drbid " << drbid);
  m_drbMap.erase (it);
}


LteRrcSap::RrcConnectionReconfiguration
UeManager::BuildRrcConnectionReconfiguration ()
{
  NS_LOG_FUNCTION (this);
  LteRrcSap::RrcConnectionReconfiguration msg;
  msg.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
  msg.haveRadioResourceConfigDedicated = true;
  msg.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated ();
  msg.haveMobilityControlInfo = false;
  msg.haveMeasConfig = true;
  msg.measConfig = m_rrc->m_ueMeasConfig;
  if ( m_caSupportConfigured == false && m_rrc->m_numberOfComponentCarriers > 1)
    {
      m_caSupportConfigured = true;
      NS_LOG_FUNCTION ( this << "CA not configured. Configure now!" );
      msg.haveNonCriticalExtension = true;
      msg.nonCriticalExtension = BuildNonCriticalExtentionConfigurationCa ();
      NS_LOG_FUNCTION ( this << " haveNonCriticalExtension " << msg.haveNonCriticalExtension );
    }
  else
    {
      msg.haveNonCriticalExtension = false;
    }

  return msg;
}

LteRrcSap::RadioResourceConfigDedicated
UeManager::BuildRadioResourceConfigDedicated ()
{
  NS_LOG_FUNCTION (this);
  LteRrcSap::RadioResourceConfigDedicated rrcd;

  if (m_srb1 != 0)
    {
      LteRrcSap::SrbToAddMod stam;
      stam.srbIdentity = m_srb1->m_srbIdentity;
      stam.logicalChannelConfig = m_srb1->m_logicalChannelConfig;
      rrcd.srbToAddModList.push_back (stam);
    }

  for (std::map <uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.begin ();
       it != m_drbMap.end ();
       ++it)
    {
      LteRrcSap::DrbToAddMod dtam;
      dtam.epsBearerIdentity = it->second->m_epsBearerIdentity;
      dtam.drbIdentity = it->second->m_drbIdentity;
      dtam.rlcConfig = it->second->m_rlcConfig;
      dtam.logicalChannelIdentity = it->second->m_logicalChannelIdentity;
      dtam.logicalChannelConfig = it->second->m_logicalChannelConfig;
      rrcd.drbToAddModList.push_back (dtam);
    }

  rrcd.havePhysicalConfigDedicated = true;
  rrcd.physicalConfigDedicated = m_physicalConfigDedicated;
  return rrcd;
}

uint8_t
UeManager::GetNewRrcTransactionIdentifier ()
{
  NS_LOG_FUNCTION (this);
  ++m_lastRrcTransactionIdentifier;
  m_lastRrcTransactionIdentifier %= 4;
  return m_lastRrcTransactionIdentifier;
}

uint8_t
UeManager::Lcid2Drbid (uint8_t lcid)
{
  NS_ASSERT (lcid > 2);
  return lcid - 2;
}

uint8_t
UeManager::Drbid2Lcid (uint8_t drbid)
{
  return drbid + 2;
}
uint8_t
UeManager::Lcid2Bid (uint8_t lcid)
{
  NS_ASSERT (lcid > 2);
  return lcid - 2;
}

uint8_t
UeManager::Bid2Lcid (uint8_t bid)
{
  return bid + 2;
}

uint8_t
UeManager::Drbid2Bid (uint8_t drbid)
{
  return drbid;
}

uint8_t
UeManager::Bid2Drbid (uint8_t bid)
{
  return bid;
}


void
UeManager::SwitchToState (State newState)
{
  NS_LOG_FUNCTION (this << ToString (newState));

  State oldState = m_state;
  m_state = newState;
  NS_LOG_INFO (this << " IMSI " << m_imsi << " RNTI " << m_rnti << " UeManager "
                    << ToString (oldState) << " --> " << ToString (newState));
  m_stateTransitionTrace (m_imsi, m_rrc->ComponentCarrierToCellId (m_componentCarrierId), m_rnti, oldState, newState);
  //NS_BUILD_DEBUG(std::cout  << int(newState) << ToString(newState) << std::endl);
  switch (newState)
    {
    case INITIAL_RANDOM_ACCESS:
    case HANDOVER_JOINING:
      NS_FATAL_ERROR ("cannot switch to an initial state");
      break;

    case CONNECTION_SETUP:
    case CONNECTION_RESUME:
      if(!m_eDrxTimeout.IsExpired()){
        m_eDrxTimeout.Cancel();
      }
      if(!m_psmTimeout.IsExpired()){
        m_psmTimeout.Cancel();
      }
      break;

    case ATTACH_REQUEST:
      break;

    case CONNECTED_NORMALLY:
      {
        if (m_pendingRrcConnectionReconfiguration == true)
          {
            ScheduleRrcConnectionReconfiguration ();
          }
        if (m_pendingStartDataRadioBearers == true && m_caSupportConfigured == true)
          {
            StartDataRadioBearers ();
          }
      }
      break;

    case CONNECTION_RECONFIGURATION:
      break;

    case CONNECTION_REESTABLISHMENT:
      break;

    case HANDOVER_LEAVING:
      break;
    case IDLE_SUSPEND_EDRX:
      if(m_enablePSM){
        m_eDrxTimeout = Simulator::Schedule(m_t3324, &UeManager::SwitchToState, this, IDLE_SUSPEND_PSM);
      }else{
        m_eDrxTimeout = Simulator::Schedule(m_t3324, &UeManager::SwitchToState, this, IDLE_SUSPEND_EDRX);
      }
      break;
    case IDLE_SUSPEND_PSM:
      // Move from eDRX to PSM
      m_psmTimeout = Simulator::Schedule(m_t3412-m_t3324, &UeManager::SwitchToState, this, CONNECTED_TAU);
      break;
    case CONNECTED_TAU:
      // usually wait for TAU but no TAU implemented yet
      SwitchToState(IDLE_SUSPEND_PSM);
      break;
    default:
      break;
    }
}

LteRrcSap::NonCriticalExtensionConfiguration
UeManager::BuildNonCriticalExtentionConfigurationCa ()
{
  NS_LOG_FUNCTION ( this );
  LteRrcSap::NonCriticalExtensionConfiguration ncec;

  //  LteRrcSap::SCellToAddMod scell;
  std::list<LteRrcSap::SCellToAddMod> SccCon;

  // sCellToReleaseList is always empty since no Scc is released

  for (auto &it: m_rrc->m_componentCarrierPhyConf)
    {
      uint8_t ccId = it.first;

      if (ccId == m_componentCarrierId)
        {
          // Skip primary CC.
          continue;
        }
      else if (ccId < m_componentCarrierId)
        {
          // Shift all IDs below PCC forward so PCC can use CC ID 1.
          ccId++;
        }

      Ptr<ComponentCarrierBaseStation> eNbCcm = it.second;
      LteRrcSap::SCellToAddMod component;
      component.sCellIndex = ccId;
      component.cellIdentification.physCellId = eNbCcm->GetCellId ();
      component.cellIdentification.dlCarrierFreq = eNbCcm->GetDlEarfcn ();
      component.radioResourceConfigCommonSCell.haveNonUlConfiguration = true;
      component.radioResourceConfigCommonSCell.nonUlConfiguration.dlBandwidth = eNbCcm->GetDlBandwidth ();
      component.radioResourceConfigCommonSCell.nonUlConfiguration.antennaInfoCommon.antennaPortsCount = 0;
      component.radioResourceConfigCommonSCell.nonUlConfiguration.pdschConfigCommon.referenceSignalPower = m_rrc->m_cphySapProvider.at (0)->GetReferenceSignalPower ();
      component.radioResourceConfigCommonSCell.nonUlConfiguration.pdschConfigCommon.pb = 0;
      component.radioResourceConfigCommonSCell.haveUlConfiguration = true;
      component.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulCarrierFreq = eNbCcm->GetUlEarfcn ();
      component.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulBandwidth = eNbCcm->GetUlBandwidth ();
      component.radioResourceConfigCommonSCell.ulConfiguration.ulPowerControlCommonSCell.alpha = 0;
      //component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon.type = LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
      component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon.srsBandwidthConfig = 0;
      component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon.srsSubframeConfig = 0;
      component.radioResourceConfigCommonSCell.ulConfiguration.prachConfigSCell.index = 0;

      if (true)
        {
          component.haveRadioResourceConfigDedicatedSCell = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.haveNonUlConfiguration = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.haveAntennaInfoDedicated = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.antennaInfo.transmissionMode = m_rrc->m_defaultTransmissionMode;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.crossCarrierSchedulingConfig = false;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.havePdschConfigDedicated = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.pdschConfigDedicated.pa = LteRrcSap::PdschConfigDedicated::dB0;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.haveUlConfiguration = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.haveAntennaInfoUlDedicated = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.antennaInfoUl.transmissionMode = m_rrc->m_defaultTransmissionMode;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.pushConfigDedicatedSCell.nPuschIdentity = 0;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.ulPowerControlDedicatedSCell.pSrsOffset = 0;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.haveSoundingRsUlConfigDedicated = true;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.soundingRsUlConfigDedicated.srsConfigIndex = GetSrsConfigurationIndex();
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.soundingRsUlConfigDedicated.type = LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
          component.radioResourceConfigDedicateSCell.physicalConfigDedicatedSCell.soundingRsUlConfigDedicated.srsBandwidth = 0;
        }
      else
        {
          component.haveRadioResourceConfigDedicatedSCell = false;
        }
      SccCon.push_back (component);
    }
  ncec.sCellsToAddModList = SccCon;

  return ncec;
}

void UeManager::NotifyDataInactivityNb(uint8_t lcid){

}
void UeManager::NotifyDataInactivitySchedulerNb(){
  NS_LOG_DEBUG ("UeManager::NotifyDataInactivitySchedulerNb"
                << " RNTI=" << m_rnti << " state=" << ToString (m_state));
  // Connected-DRX FUG (idealfug): do NOT arm the data-inactivity release. The UE
  // stays RRC_CONNECTED and only MAC-cDRX-sleeps (NPDCCH monitored once per DRX
  // cycle, ~10.24 s). Releasing it here would drop it to eDRX->PSM and force a
  // re-RACH (contention) on the next packet -- the opposite of an ideal FUG. The
  // oracle pushes the next grant onto the still-connected UE, contention-free.
  if (m_cdrxFug){
    if (NbIotDebugTrace ())
      std::cout << "[ENB-CDRX-KEEPALIVE] t=" << Simulator::Now ().GetSeconds ()
                << " rnti=" << m_rnti << " imsi=" << m_imsi
                << " (connected-DRX: no inactivity release)" << std::endl;
    return;
  }
  if (m_state == CONNECTED_NORMALLY){
    if(!m_dataInactivityTimeout.IsExpired()){
      m_dataInactivityTimeout.Cancel();
    }
    NS_LOG_DEBUG ("RNTI=" << m_rnti
                  << " starting inactivity timer " << m_dataInactivityInterval << "ms"
                  << " -> SwitchToResumeNb");
    m_dataInactivityTimeout = Simulator::Schedule(MilliSeconds(m_dataInactivityInterval), &UeManager::SwitchToResumeNb, this);
  }
}
void UeManager::ReleaseOnRai(){
  // AS RAI (TS 36.321 5.4.5): the UE indicated no further UL/DL data -> release
  // immediately, skipping the data-inactivity timer. This is what makes the
  // FUG suspend prompt and deterministic for single-packet ambient traffic.
  NS_LOG_DEBUG ("UeManager::ReleaseOnRai RNTI=" << m_rnti << " state=" << ToString (m_state));
  // Connected-DRX FUG (idealfug) never releases to idle -- see
  // NotifyDataInactivitySchedulerNb. A RAI must not tear the connected context down.
  if (m_cdrxFug){
    return;
  }
  if (m_state == CONNECTED_NORMALLY){
    if(!m_dataInactivityTimeout.IsExpired()){
      m_dataInactivityTimeout.Cancel();
    }
    SwitchToResumeNb();
  }
}

void UeManager::NotifyDataActivitySchedulerNb(){
    NS_LOG_DEBUG ("UeManager::NotifyDataActivitySchedulerNb"
                  << " RNTI=" << m_rnti << " state=" << ToString (m_state));
    if(!m_dataInactivityTimeout.IsExpired()){
      m_dataInactivityTimeout.Cancel();
    }
    // When the UE wakes up under persistent grant, its ideal BSR triggers a
    // NotifyDataActivity. The UeManager must follow the UE back into
    // CONNECTED_NORMALLY so the data-inactivity timer can rearm and the normal
    // release→eDRX cycle can fire again.
    // Proactive FUG: the activity notification here is fired by a SPECULATIVE
    // pushed DCI0 (lte-enb-mac NotifyDataActivitySchedulerNb), not by an observed
    // UE transmission. The real UE may still be asleep. Force-waking the eNB-side
    // UeManager (and arming inactivity) on the push orphans the UE's later real
    // wake. Cancel any inactivity countdown (so an awake UE is not prematurely
    // re-suspended) but do NOT resurrect a suspended UeManager: its CONNECTED
    // state is driven by the UE's actual UL transmission.
    if (m_proactiveFug){
      return;
    }
    // Release-grace: activity arriving within this window of the release we
    // just sent is ARQ residue of that release (UE status ACK / ideal-BSR echo
    // reported before the UE-side SRB1 reset runs), not new data. Resurrecting
    // on it re-arms data-inactivity and re-releases: a release<->ACK loop.
    // Swallow it; the +100 ms SRB1 flush then finds the manager still
    // suspended and clears the residue for good. (Same idiom as grant-grace.)
    if (m_lastReleaseTime.IsPositive ()
        && Simulator::Now () - m_lastReleaseTime < MilliSeconds (200))
      {
        if (NbIotDebugTrace ())
          std::cout << "[ENB-RELEASE-GRACE] t=" << Simulator::Now ().GetSeconds ()
                    << " rnti=" << m_rnti << " imsi=" << m_imsi
                    << " (activity within release-grace: ARQ residue, no wake)" << std::endl;
        return;
      }
    if (m_persistentGrant &&
        (m_state == IDLE_SUSPEND_EDRX || m_state == IDLE_SUSPEND_PSM || m_state == CONNECTED_TAU)){
      WakeFromPersistentGrant();
    }
}
void UeManager::NotifyUlDataObservedNb(){
    // Radio-level evidence: a DRB NPUSCH transport block from this UE was
    // actually RECEIVED (lcid >= 3, filtered at the MAC). Unlike scheduler
    // activity, this cannot be one of our own speculative proactive-FUG pushes
    // and cannot fire while the UE is genuinely asleep -- so it wakes the
    // manager for ALL persistent-grant arms, proactiveFug included. Without
    // this, a fug manager parks forever after its first (now promptly
    // delivered) release: the inactivity timer can never re-arm and the UE is
    // never re-suspended -> never-sleeps from packet 2 on.
    NS_LOG_DEBUG ("UeManager::NotifyUlDataObservedNb"
                  << " RNTI=" << m_rnti << " state=" << ToString (m_state));
    // NO release-grace veto here (unlike scheduler activity above). 
    if (NbIotDebugTrace ()
        && m_lastReleaseTime.IsPositive ()
        && Simulator::Now () - m_lastReleaseTime < MilliSeconds (200))
      std::cout << "[ENB-GRACE-DRB-WAKE] t=" << Simulator::Now ().GetSeconds ()
                << " rnti=" << m_rnti << " imsi=" << m_imsi
                << " (DRB data within release-grace: genuine, waking)" << std::endl;
    if (m_persistentGrant &&
        (m_state == IDLE_SUSPEND_EDRX || m_state == IDLE_SUSPEND_PSM || m_state == CONNECTED_TAU)){
      if (NbIotDebugTrace ())
        std::cout << "[ENB-OBSERVED-WAKE] t=" << Simulator::Now ().GetSeconds ()
                  << " rnti=" << m_rnti << " imsi=" << m_imsi
                  << " (received UL DRB data -> manager back to CONNECTED)" << std::endl;
      WakeFromPersistentGrant();
    }
    // Observed data = activity NOW: RESTART the data-inactivity countdown via
    // the canonical method (which also carries the cdrxFug never-release guard
    // and the CONNECTED_NORMALLY guard). Do NOT merely cancel it: PDU
    // reception fires AFTER the DCI opportunity-end re-arm, so a bare cancel
    // orphans the countdown and no release ever fires again (verified: a
    // cancel-only version regressed EVERY arm to never-sleep).
    NotifyDataInactivitySchedulerNb();
}
void UeManager::SwitchToResumeNb(){
  NS_LOG_DEBUG ("UeManager::SwitchToResumeNb RNTI=" << m_rnti
                << " IMSI=" << m_imsi
                << " PG=" << m_persistentGrant);
  // Complete the RLC-AM ACK exchange BEFORE commanding the UE to suspend: if any
  // of this UE's eNB-side AM entities still OWES a status PDU (ACK), releasing now
  // wipes the pending DL request (ParkUe) -- the ACK is never born, and the UE
  // poll-retransmits an already-delivered PDU at its next contact, which lands
  // below the eNB's receive window. Defer the release briefly (bounded) so the
  // status N1 goes out first; a real eNB drains ARQ state before RRC release.
  bool ackOwed = false;
  if (m_srb1 != 0 && m_srb1->m_rlc != 0)
    {
      Ptr<LteRlcAm> srb1Am = DynamicCast<LteRlcAm> (m_srb1->m_rlc);
      if (srb1Am != 0 && srb1Am->HasPendingStatusPdu ()) ackOwed = true;
    }
  for (std::map<uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator drbIt = m_drbMap.begin ();
       !ackOwed && drbIt != m_drbMap.end (); ++drbIt)
    {
      Ptr<LteRlcAm> drbAm = DynamicCast<LteRlcAm> (drbIt->second->m_rlc);
      if (drbAm != 0 && drbAm->HasPendingStatusPdu ()) ackOwed = true;
    }
  if (ackOwed && m_releaseAckDeferCount < 10)
    {
      ++m_releaseAckDeferCount;
      if (NbIotDebugTrace ())
        std::cout << "[RELEASE-DEFER] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
                  << " imsi=" << m_imsi << " try=" << (uint32_t) m_releaseAckDeferCount
                  << " (ACK owed; deferring release 40 ms)" << std::endl;
      Simulator::Schedule (MilliSeconds (40), &LteEnbRrc::DeferredReleaseNb, m_rrc, m_rnti);
      return;
    }
  m_releaseAckDeferCount = 0;
  // Loss-diagnostic: eNB data-inactivity release. Reallocates the resumeId, so if the UE
  // later resumes with its OLD resumeId (e.g. a contention loser delayed past this 5 s timer)
  // the eNB won't recognise it -> resume rejected -> the UE's buffered packet is orphaned.
  if (NbIotDebugTrace ())
    std::cout << "[ENB-INACTIVITY-RELEASE] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
              << " imsi=" << m_imsi << " oldResumeId=" << m_resumeId
              << " state=" << ToString (m_state) << std::endl;
  NbIotRrcSap::RrcConnectionReleaseNb msg;
  msg.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier ();
  msg.releaseCauseNb = NbIotRrcSap::RrcConnectionReleaseNb::ReleaseCauseNb::rrc_Suspend;
  m_resumeId = m_rrc->DoAllocateTemporaryResumeId();
  msg.resumeIdentity = m_resumeId;
  m_lastReleaseTime = Simulator::Now ();   // arm the release-grace window
  m_rrc->m_rrcSapUser->SendRrcConnectionReleaseNb(m_rnti, msg);
  // The release is fire-and-forget (no RRC ACK).
  m_srb1FlushDeferCount = 0;   // fresh delivery-wait budget for this release
  Simulator::Schedule (MilliSeconds (100), &LteEnbRrc::FlushSrb1AfterReleaseNb, m_rrc, m_rnti);
  SwitchToState(IDLE_SUSPEND_EDRX);
  if (m_persistentGrant){
    // Park the UE in the scheduler: stop UL scheduling (so residual buffer can't
    // trigger a spurious DCI N0 that re-wakes the just-suspended UE) while KEEPING
    // its context for a contention-free SR-resume. Fixes the suspend<->wake loop.
    m_rrc->m_cmacSapProvider.at(0)->ParkUeInScheduler(m_rnti);
    NS_LOG_DEBUG ("RNTI=" << m_rnti
                  << " suspended to EDRX, context PRESERVED (no MoveUeToResumed)");
    return;
  }
  NS_LOG_DEBUG ("RNTI=" << m_rnti
                << " suspended, scheduling MoveUeToResumed in 1000ms");
  Simulator::Schedule(MilliSeconds(1000), &LteEnbRrc::MoveUeToResumed, m_rrc, m_rnti, m_resumeId);
}

bool UeManager::InReleaseGraceNb() const {
  // Same window as the activity/observed-UL handlers: activity arriving within
  // 200 ms of the release we just sent is ARQ residue of that release.
  return m_lastReleaseTime.IsPositive ()
         && Simulator::Now () - m_lastReleaseTime < MilliSeconds (200);
}

void UeManager::FlushSrb1ReleaseResidueNb(){
  // Only while still suspended: if the UE resumed in the meantime, SRB1 is
  // live again and the normal machinery owns it.
  if (m_state != IDLE_SUSPEND_EDRX && m_state != IDLE_SUSPEND_PSM) { return; }
  if (m_srb1 != 0 && m_srb1->m_rlc != 0)
    {
      // Delivery-conditional flush:
      Ptr<LteRlcAm> srb1Am = DynamicCast<LteRlcAm> (m_srb1->m_rlc);
      if (srb1Am != 0 && srb1Am->GetUntransmittedBytes () > 0
          && m_srb1FlushDeferCount < 50)
        {
          m_srb1FlushDeferCount++;
          if (NbIotDebugTrace ())
            std::cout << "[ENB-SRB1-FLUSH-DEFER] t=" << Simulator::Now ().GetSeconds ()
                      << " rnti=" << m_rnti << " imsi=" << m_imsi
                      << " untransmitted=" << srb1Am->GetUntransmittedBytes ()
                      << " defers=" << (uint32_t) m_srb1FlushDeferCount
                      << " (release not yet on air -- keep it)" << std::endl;
          Simulator::Schedule (MilliSeconds (100), &LteEnbRrc::FlushSrb1AfterReleaseNb,
                               m_rrc, m_rnti);
          return;
        }
      if (NbIotDebugTrace ())
        std::cout << "[ENB-SRB1-FLUSH] t=" << Simulator::Now ().GetSeconds ()
                  << " rnti=" << m_rnti << " imsi=" << m_imsi
                  << " (drop un-ACKed release PDU; SN 0 to match the UE)" << std::endl;
      m_srb1->m_rlc->DoReset ();
    }
}

void UeManager::RePark(){
  if (NbIotDebugTrace ())
    std::cout << "[ENB-RESUME-REPARK] t=" << Simulator::Now ().GetSeconds () << " rnti=" << m_rnti
              << " imsi=" << m_imsi << " resumeId=" << m_resumeId
              << " (resume timed out -> re-parked for clean re-RACH)" << std::endl;
  // Re-park via the CANONICAL park path (the same one a normal release uses). It re-stores
  // this context under its resumeId across ALL layers -- RRC (m_ueResumedMap), MAC
  // (m_resumeRlcAttached), CCM (m_resume*), S1 -- then tears the stale temp RNTI down
  // (RemoveUeNb). This matters because every one of those resume caches is SINGLE-USE
  // (consumed by the failed first DoResumeUe); re-storing only some leaves the retry with a
  // half-restored context (SRB not re-attached -> Msg4 undeliverable -> the UE loops). The
  // manager object survives the RNTI teardown via the m_ueResumedMap reference.
  // Reset this side's SRB1 RLC-AM first, so its sequence numbers restart at 0 to MATCH the
  // UE (which reset its own SRB1 RLC on RESUME-LOST). This is the RA case (persistentGrant
  // off): the UE already delivered an earlier packet, so the eNB's SRB1 RLC SN is ADVANCED;
  // without resetting it here, the eNB's re-resume Msg4 (SN=k) falls outside the UE's reset
  // receive window (VR(R)=0) and is silently dropped -> the UE never confirms -> re-park loop
  // -> 4x T300 -> give-up -> orphaned packet. (Safe now that LteRlcAm::DoReset fully restores
  // the entity -- it previously crashed here on an unresized txed buffer.)
  if (m_srb1) m_srb1->m_rlc->DoReset ();
  // Reset the DRB RLCs too. The UE resets its SRB1 AND DRB RLCs on RESUME-LOST; if the eNB
  // resets only SRB1, the resume completes but the UE's DRB DATA (sent with SN 0) is outside
  // the eNB DRB RLC-AM's still-advanced window -> silently dropped BEFORE the S1 forward-up
  // (observed: UE-TX + ENB-DATA-RX present but no ENB-FWD-UP -> the packet dies in the eNB).
  for (std::map<uint8_t, Ptr<LteDataRadioBearerInfo> >::iterator it = m_drbMap.begin ();
       it != m_drbMap.end (); ++it)
    {
      it->second->m_rlc->DoReset ();
    }
  m_rrc->MoveUeToResumed (m_rnti, m_resumeId);
  SwitchToState (IDLE_SUSPEND_PSM);
}

void UeManager::SetPersistentGrant(bool enable){
  m_persistentGrant = enable;
}

bool UeManager::IsPersistentGrant() const{
  return m_persistentGrant;
}

void UeManager::SetProactiveFug(bool enable){
  m_proactiveFug = enable;
}

bool UeManager::IsProactiveFug() const{
  return m_proactiveFug;
}

void UeManager::SetCdrxFug(bool enable){
  m_cdrxFug = enable;
}

bool UeManager::IsCdrxFug() const{
  return m_cdrxFug;
}

void UeManager::WakeFromPersistentGrant(){
  NS_LOG_DEBUG ("UeManager::WakeFromPersistentGrant RNTI=" << m_rnti
                << " state=" << ToString (m_state));
  if (m_state != IDLE_SUSPEND_EDRX && m_state != IDLE_SUSPEND_PSM && m_state != CONNECTED_TAU){
    NS_LOG_DEBUG ("WakeFromPersistentGrant: RNTI=" << m_rnti
                  << " already in " << ToString (m_state) << ", skipping");
    return;
  }
  if (!m_eDrxTimeout.IsExpired()){ m_eDrxTimeout.Cancel(); }
  if (!m_psmTimeout.IsExpired()){ m_psmTimeout.Cancel(); }
  SwitchToState(CONNECTED_NORMALLY);
  NS_LOG_DEBUG ("WakeFromPersistentGrant: RNTI=" << m_rnti
                << " -> CONNECTED_NORMALLY, notifying eNB MAC");
  m_rrc->m_cmacSapProvider.at(0)->NotifyConnectionSuccessful(m_rnti);
}

void UeManager::SetLogDir(std::string logdir){
  m_logdir = logdir;
}

///////////////////////////////////////////
// eNB RRC methods
///////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (LteEnbRrc);

LteEnbRrc::LteEnbRrc ()
  : m_x2SapProvider (0),
    m_cmacSapProvider (0),
    m_handoverManagementSapProvider (0),
    m_ccmRrcSapProvider (0),
    m_anrSapProvider (0),
    m_ffrRrcSapProvider (0),
    m_rrcSapUser (0),
    m_macSapProvider (0),
    m_s1SapProvider (0),
    m_cphySapProvider (0),
    m_configured (false),
    m_lastAllocatedRnti (0),
    m_srsCurrentPeriodicityId (0),
    m_lastAllocatedConfigurationIndex (0),
    m_reconfigureUes (false),
    m_numberOfComponentCarriers (0),
    m_carriersConfigured (false),
    m_edt(true)
{
  NS_LOG_FUNCTION (this);
  m_legacy_lte = false;
  m_cmacSapUser.push_back (new EnbRrcMemberLteEnbCmacSapUser (this, 0));
  m_handoverManagementSapUser = new MemberLteHandoverManagementSapUser<LteEnbRrc> (this);
  m_anrSapUser = new MemberLteAnrSapUser<LteEnbRrc> (this);
  m_ffrRrcSapUser.push_back (new MemberLteFfrRrcSapUser<LteEnbRrc> (this));
  m_rrcSapProvider = new MemberLteEnbRrcSapProvider<LteEnbRrc> (this);
  m_x2SapUser = new EpcX2SpecificEpcX2SapUser<LteEnbRrc> (this);
  m_s1SapUser = new MemberEpcEnbS1SapUser<LteEnbRrc> (this);
  m_cphySapUser.push_back (new MemberLteEnbCphySapUser<LteEnbRrc> (this));
  m_ccmRrcSapUser = new MemberLteCcmRrcSapUser <LteEnbRrc>(this);

}

void
LteEnbRrc::ConfigureCarriers (std::map<uint8_t, Ptr<ComponentCarrierBaseStation>> ccPhyConf)
{
  NS_ASSERT_MSG (!m_carriersConfigured, "Secondary carriers can be configured only once.");
  m_componentCarrierPhyConf = ccPhyConf;
  NS_ABORT_MSG_IF (m_numberOfComponentCarriers != m_componentCarrierPhyConf.size (), " Number of component carriers "
                                                  "are not equal to the number of he component carrier configuration provided");

  for (uint8_t i = 1; i < m_numberOfComponentCarriers; i++)
    {
      m_cphySapUser.push_back (new MemberLteEnbCphySapUser<LteEnbRrc> (this));
      m_cmacSapUser.push_back (new EnbRrcMemberLteEnbCmacSapUser (this, i));
      m_ffrRrcSapUser.push_back (new MemberLteFfrRrcSapUser<LteEnbRrc> (this));
    }
  m_carriersConfigured = true;
  Object::DoInitialize ();
}

LteEnbRrc::~LteEnbRrc ()
{
  NS_LOG_FUNCTION (this);
}


void
LteEnbRrc::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  for ( uint8_t i = 0; i < m_numberOfComponentCarriers ; i++)
    {
      delete m_cphySapUser[i];
      delete m_cmacSapUser[i];
      delete m_ffrRrcSapUser[i];
    }
  //delete m_cphySapUser;
  m_cphySapUser.erase (m_cphySapUser.begin (),m_cphySapUser.end ());
  m_cphySapUser.clear ();
  //delete m_cmacSapUser;
  m_cmacSapUser.erase (m_cmacSapUser.begin (),m_cmacSapUser.end ());
  m_cmacSapUser.clear ();
  //delete m_ffrRrcSapUser;
  m_ffrRrcSapUser.erase (m_ffrRrcSapUser.begin (),m_ffrRrcSapUser.end ());
  m_ffrRrcSapUser.clear ();
  m_ueActiveMap.clear ();
  delete m_handoverManagementSapUser;
  delete m_ccmRrcSapUser;
  delete m_anrSapUser;
  delete m_rrcSapProvider;
  delete m_x2SapUser;
  delete m_s1SapUser;

}

TypeId
LteEnbRrc::GetTypeId (void)
{
  NS_LOG_FUNCTION ("LteEnbRrc::GetTypeId");
  static TypeId tid = TypeId ("ns3::LteEnbRrc")
    .SetParent<Object> ()
    .SetGroupName("Lte")
    .AddConstructor<LteEnbRrc> ()
    .AddAttribute ("UeMap", "List of UeManager by C-RNTI.",
                   ObjectMapValue (),
                   MakeObjectMapAccessor (&LteEnbRrc::m_ueActiveMap),
                   MakeObjectMapChecker<UeManager> ())
    .AddAttribute ("DefaultTransmissionMode",
                   "The default UEs' transmission mode (0: SISO)",
                   UintegerValue (0),  // default tx-mode
                   MakeUintegerAccessor (&LteEnbRrc::m_defaultTransmissionMode),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("EpsBearerToRlcMapping",
                   "Specify which type of RLC will be used for each type of EPS bearer. ",
                   EnumValue (RLC_AM_ALWAYS),
                   MakeEnumAccessor (&LteEnbRrc::m_epsBearerToRlcMapping),
                   MakeEnumChecker (RLC_SM_ALWAYS, "RlcSmAlways",
                                    RLC_UM_ALWAYS, "RlcUmAlways",
                                    RLC_AM_ALWAYS, "RlcAmAlways",
                                    PER_BASED,     "PacketErrorRateBased"))
    .AddAttribute ("SystemInformationPeriodicity",
                   "The interval for sending system information (Time value)",
                   TimeValue (MilliSeconds (80)),
                   MakeTimeAccessor (&LteEnbRrc::m_systemInformationPeriodicity),
                   MakeTimeChecker ())
    .AddAttribute ("T3324",
                "NB-IoT Active timer T3324 in ms (3GPP TS 24.008 §10.5.7.4a). "
                "Time spent in IDLE_SUSPEND_EDRX after RRC release before "
                "entering PSM. Operator-typical: 20 s (mass IoT).",
                IntegerValue (20000),  // 20 s, GSMA NB-IoT deployment guide
                MakeIntegerAccessor (&LteEnbRrc::m_t3324),
                MakeIntegerChecker<int32_t> ())
    .AddAttribute ("T3412",
                  "NB-IoT Periodic TAU timer T3412 in ms (3GPP TS 24.008 "
                  "§10.5.7.3). Time in PSM before periodic TAU wake. "
                  "Operator-typical: 1 h (mass IoT).",
                  IntegerValue (3600000),  // 1 h
                  MakeIntegerAccessor (&LteEnbRrc::m_t3412),
                  MakeIntegerChecker<int64_t> ())
    .AddAttribute ("TeDRXC",
              "NB-IoT eDRX cycle in ms (TS 36.331 -NB). Standard NB-IoT "
              "values: 2.56*2^k for k=0..15 → 2.56s..23304s. Operator-"
              "typical: 20.48 s.",
              IntegerValue (20480),  // 20.48 s
              MakeIntegerAccessor (&LteEnbRrc::m_eDrxCycle),
              MakeIntegerChecker<int32_t> ())
    .AddAttribute ("RrcReleaseInterval",
              "RRC inactivity release timer in ms — eNB releases the UE "
              "this long after the last UL/DL data event. Operator-typical: "
              "5 s. Set to a small value (e.g. 40) to mimic the aggressive "
              "release behaviour of some NB-IoT deployments.",
              UintegerValue (5000),  // 5 s, vs. legacy 50000
              MakeUintegerAccessor (&LteEnbRrc::m_dataInactivityInterval),
              MakeUintegerChecker<uint32_t> (0, 600000) )  // up to 10 min (was uint16_t: 100 s truncated to 34.5 s)
    .AddAttribute ("EnablePSM",
               "If true, PSM will be enabled.",
               BooleanValue (true),
               MakeBooleanAccessor (&LteEnbRrc::m_enablePSM),
               MakeBooleanChecker ())
    .AddAttribute ("PersistentGrant",
               "If true, UEs keep their RNTI and scheduler context across "
               "eDRX suspend cycles; the eNB wakes them via DCI0 on UL data "
               "instead of requiring a resume-via-RACH.",
               BooleanValue (false),
               MakeBooleanAccessor (&LteEnbRrc::m_persistentGrant),
               MakeBooleanChecker ())
    .AddAttribute ("ProactiveFug",
               "Proactive FUG (4th mode): a speculatively pushed DCI0 must not "
               "force-wake the eNB-side UeManager from a suspended state; the "
               "wake is driven by the UE's actual UL transmission.",
               BooleanValue (false),
               MakeBooleanAccessor (&LteEnbRrc::m_proactiveFug),
               MakeBooleanChecker ())
    .AddAttribute ("CdrxFug",
               "Connected-DRX FUG (idealfug): the UE stays RRC_CONNECTED between "
               "sparse packets and only MAC-cDRX-sleeps, so the eNB must NOT run "
               "the data-inactivity release for it -- otherwise it drops to "
               "eDRX->PSM and must re-RACH (contend) on the next packet.",
               BooleanValue (false),
               MakeBooleanAccessor (&LteEnbRrc::m_cdrxFug),
               MakeBooleanChecker ())


    // SRS related attributes
    .AddAttribute ("SrsPeriodicity",
                   "The SRS periodicity in milliseconds",
                   UintegerValue (320),
                   MakeUintegerAccessor (&LteEnbRrc::SetSrsPeriodicity,
                                         &LteEnbRrc::GetSrsPeriodicity),
                   MakeUintegerChecker<uint32_t> ())

    // Timeout related attributes
    .AddAttribute ("ConnectionRequestTimeoutDuration",
                   "After a RA attempt, if no RRC CONNECTION REQUEST is "
                   "received before this time, the UE context is destroyed. "
                   "Must account for reception of RAR and transmission of "
                   "RRC CONNECTION REQUEST over UL GRANT. The value of this"
                   "timer should not be greater than T300 timer at UE RRC",
                   TimeValue (MilliSeconds (30000)),
                   MakeTimeAccessor (&LteEnbRrc::m_connectionRequestTimeoutDuration),
                   MakeTimeChecker (MilliSeconds (1), MilliSeconds (50000)))
    .AddAttribute ("ConnectionSetupTimeoutDuration",
                   "After accepting connection request, if no RRC CONNECTION "
                   "SETUP COMPLETE is received before this time, the UE "
                   "context is destroyed. Must account for the UE's reception "
                   "of RRC CONNECTION SETUP and transmission of RRC CONNECTION "
                   "SETUP COMPLETE.",
                   TimeValue (MilliSeconds (30000)),
                   MakeTimeAccessor (&LteEnbRrc::m_connectionSetupTimeoutDuration),
                   MakeTimeChecker ())
    .AddAttribute ("ConnectionResumeTimeoutDuration",
                   "After accepting connection request, if no RRC CONNECTION "
                   "SETUP COMPLETE is received before this time, the UE "
                   "context is destroyed. Must account for the UE's reception "
                   "of RRC CONNECTION SETUP and transmission of RRC CONNECTION "
                   "SETUP COMPLETE.",
                   TimeValue (MilliSeconds (30000)),
                   MakeTimeAccessor (&LteEnbRrc::m_connectionResumeTimeoutDuration),
                   MakeTimeChecker ())
    .AddAttribute ("ConnectionRejectedTimeoutDuration",
                   "Time to wait between sending a RRC CONNECTION REJECT and "
                   "destroying the UE context",
                   TimeValue (MilliSeconds (30)),
                   MakeTimeAccessor (&LteEnbRrc::m_connectionRejectedTimeoutDuration),
                   MakeTimeChecker ())
    .AddAttribute ("HandoverJoiningTimeoutDuration",
                   "After accepting a handover request, if no RRC CONNECTION "
                   "RECONFIGURATION COMPLETE is received before this time, the "
                   "UE context is destroyed. Must account for reception of "
                   "X2 HO REQ ACK by source eNB, transmission of the Handover "
                   "Command, non-contention-based random access and reception "
                   "of the RRC CONNECTION RECONFIGURATION COMPLETE message.",
                   TimeValue (MilliSeconds (200)),
                   MakeTimeAccessor (&LteEnbRrc::m_handoverJoiningTimeoutDuration),
                   MakeTimeChecker ())
    .AddAttribute ("HandoverLeavingTimeoutDuration",
                   "After issuing a Handover Command, if neither RRC "
                   "CONNECTION RE-ESTABLISHMENT nor X2 UE Context Release has "
                   "been previously received, the UE context is destroyed.",
                   TimeValue (MilliSeconds (500)),
                   MakeTimeAccessor (&LteEnbRrc::m_handoverLeavingTimeoutDuration),
                   MakeTimeChecker ())

    // Cell selection related attribute
    .AddAttribute ("QRxLevMin",
                   "One of information transmitted within the SIB1 message, "
                   "indicating the required minimum RSRP level that any UE must "
                   "receive from this cell before it is allowed to camp to this "
                   "cell. The default value -70 corresponds to -140 dBm and is "
                   "the lowest possible value as defined by Section 6.3.4 of "
                   "3GPP TS 36.133. This restriction, however, only applies to "
                   "initial cell selection and EPC-enabled simulation.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   IntegerValue (-70),
                   MakeIntegerAccessor (&LteEnbRrc::m_qRxLevMin),
                   MakeIntegerChecker<int8_t> (-70, -22))
    .AddAttribute ("NumberOfComponentCarriers",
                   "Number of Component Carriers ",
                   UintegerValue (1),
                   MakeIntegerAccessor (&LteEnbRrc::m_numberOfComponentCarriers),
                   MakeIntegerChecker<int16_t> (MIN_NO_CC, MAX_NO_CC))

    // Handover related attributes
    .AddAttribute ("AdmitHandoverRequest",
                   "Whether to admit an X2 handover request from another eNB",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteEnbRrc::m_admitHandoverRequest),
                   MakeBooleanChecker ())
  .AddAttribute ("AdmitRrcConnectionRequest",
                  "Whether to admit a connection request from a UE",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteEnbRrc::m_admitRrcConnectionRequest),
                   MakeBooleanChecker ())
  .AddAttribute ("AdmitRrcConnectionResumeRequest",
                   "Whether to admit a connection request from a UE",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteEnbRrc::m_admitRrcConnectionResumeRequest),
                   MakeBooleanChecker ())
    // UE measurements related attributes
    .AddAttribute ("RsrpFilterCoefficient",
                   "Determines the strength of smoothing effect induced by "
                   "layer 3 filtering of RSRP in all attached UE; "
                   "if set to 0, no layer 3 filtering is applicable",
                   // i.e. the variable k in 3GPP TS 36.331 section 5.5.3.2
                   UintegerValue (4),
                   MakeUintegerAccessor (&LteEnbRrc::m_rsrpFilterCoefficient),
                   MakeUintegerChecker<uint8_t> (0))
    .AddAttribute ("RsrqFilterCoefficient",
                   "Determines the strength of smoothing effect induced by "
                   "layer 3 filtering of RSRQ in all attached UE; "
                   "if set to 0, no layer 3 filtering is applicable",
                   // i.e. the variable k in 3GPP TS 36.331 section 5.5.3.2
                   UintegerValue (4),
                   MakeUintegerAccessor (&LteEnbRrc::m_rsrqFilterCoefficient),
                   MakeUintegerChecker<uint8_t> (0))

    // Trace sources
    .AddTraceSource ("NewUeContext",
                     "Fired upon creation of a new UE context.",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_newUeContextTrace),
                     "ns3::LteEnbRrc::NewUeContextTracedCallback")
    .AddTraceSource ("ConnectionEstablished",
                     "Fired upon successful RRC connection establishment.",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_connectionEstablishedTrace),
                     "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
    .AddTraceSource ("ConnectionReconfiguration",
                     "trace fired upon RRC connection reconfiguration",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_connectionReconfigurationTrace),
                     "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
    .AddTraceSource ("HandoverStart",
                     "trace fired upon start of a handover procedure",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_handoverStartTrace),
                     "ns3::LteEnbRrc::HandoverStartTracedCallback")
    .AddTraceSource ("HandoverEndOk",
                     "trace fired upon successful termination of a handover procedure",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_handoverEndOkTrace),
                     "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
    .AddTraceSource ("RecvMeasurementReport",
                     "trace fired when measurement report is received",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_recvMeasurementReportTrace),
                     "ns3::LteEnbRrc::ReceiveReportTracedCallback")
    .AddTraceSource ("NotifyConnectionRelease",
                     "trace fired when an UE is released",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_connectionReleaseTrace),
                     "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
    .AddTraceSource ("RrcTimeout",
                     "trace fired when a timer expires",
                     MakeTraceSourceAccessor (&LteEnbRrc::m_rrcTimeoutTrace),
                     "ns3::LteEnbRrc::TimerExpiryTracedCallback")
  ;
  return tid;
}

void
LteEnbRrc::SetEpcX2SapProvider (EpcX2SapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_x2SapProvider = s;
}

EpcX2SapUser*
LteEnbRrc::GetEpcX2SapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_x2SapUser;
}

void
LteEnbRrc::SetLteEnbCmacSapProvider (LteEnbCmacSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_cmacSapProvider.at (0) = s;
}

void
LteEnbRrc::SetLteEnbCmacSapProvider (LteEnbCmacSapProvider * s, uint8_t pos)
{
  NS_LOG_FUNCTION (this << s);
  if (m_cmacSapProvider.size () > pos)
    {
      m_cmacSapProvider.at (pos) = s;
    }
  else
    {
      m_cmacSapProvider.push_back (s);
      NS_ABORT_IF (m_cmacSapProvider.size () - 1 != pos);
    }
}

LteEnbCmacSapUser*
LteEnbRrc::GetLteEnbCmacSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_cmacSapUser.at (0);
}

LteEnbCmacSapUser*
LteEnbRrc::GetLteEnbCmacSapUser (uint8_t pos)
{
  NS_LOG_FUNCTION (this);
  return m_cmacSapUser.at (pos);
}

void
LteEnbRrc::SetLteHandoverManagementSapProvider (LteHandoverManagementSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_handoverManagementSapProvider = s;
}

LteHandoverManagementSapUser*
LteEnbRrc::GetLteHandoverManagementSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_handoverManagementSapUser;
}

void
LteEnbRrc::SetLteCcmRrcSapProvider (LteCcmRrcSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_ccmRrcSapProvider = s;
}

LteCcmRrcSapUser*
LteEnbRrc::GetLteCcmRrcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_ccmRrcSapUser;
}

void
LteEnbRrc::SetLteAnrSapProvider (LteAnrSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_anrSapProvider = s;
}

LteAnrSapUser*
LteEnbRrc::GetLteAnrSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_anrSapUser;
}

void
LteEnbRrc::SetLteFfrRrcSapProvider (LteFfrRrcSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  if (m_ffrRrcSapProvider.size () > 0)
    {
      m_ffrRrcSapProvider.at (0) = s;
    }
  else
    {
      m_ffrRrcSapProvider.push_back (s);
    }

}

void
LteEnbRrc::SetLteFfrRrcSapProvider (LteFfrRrcSapProvider * s, uint8_t index)
{
  NS_LOG_FUNCTION (this << s);
  if (m_ffrRrcSapProvider.size () > index)
    {
      m_ffrRrcSapProvider.at (index) = s;
    }
  else
    {
      m_ffrRrcSapProvider.push_back (s);
      NS_ABORT_MSG_IF (m_ffrRrcSapProvider.size () - 1 != index,
                       "You meant to store the pointer at position " <<
                       static_cast<uint32_t> (index) <<
                       " but it went to " << m_ffrRrcSapProvider.size () - 1);
    }
}

LteFfrRrcSapUser*
LteEnbRrc::GetLteFfrRrcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_ffrRrcSapUser.at (0);
}

LteFfrRrcSapUser*
LteEnbRrc::GetLteFfrRrcSapUser (uint8_t index)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (index < m_numberOfComponentCarriers, "Invalid component carrier index:"<<index<<" provided in order to obtain FfrRrcSapUser.");
  return m_ffrRrcSapUser.at (index);
}

void
LteEnbRrc::SetLteEnbRrcSapUser (LteEnbRrcSapUser * s)
{
  NS_LOG_FUNCTION (this << s);
  m_rrcSapUser = s;
}

LteEnbRrcSapProvider*
LteEnbRrc::GetLteEnbRrcSapProvider ()
{
  NS_LOG_FUNCTION (this);
  return m_rrcSapProvider;
}

void
LteEnbRrc::SetLteMacSapProvider (LteMacSapProvider * s)
{
  NS_LOG_FUNCTION (this);
  m_macSapProvider = s;
}

void
LteEnbRrc::SetS1SapProvider (EpcEnbS1SapProvider * s)
{
  m_s1SapProvider = s;
}


EpcEnbS1SapUser*
LteEnbRrc::GetS1SapUser ()
{
  return m_s1SapUser;
}

void
LteEnbRrc::SetLteEnbCphySapProvider (LteEnbCphySapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  if (m_cphySapProvider.size () > 0)
    {
      m_cphySapProvider.at (0) = s;
    }
  else
    {
      m_cphySapProvider.push_back (s);
    }
}

LteEnbCphySapUser*
LteEnbRrc::GetLteEnbCphySapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_cphySapUser.at(0);
}

void
LteEnbRrc::SetLteEnbCphySapProvider (LteEnbCphySapProvider * s, uint8_t pos)
{
  NS_LOG_FUNCTION (this << s);
  if (m_cphySapProvider.size () > pos)
    {
      m_cphySapProvider.at(pos) = s;
    }
  else
    {
      m_cphySapProvider.push_back (s);
      NS_ABORT_IF (m_cphySapProvider.size () - 1 != pos);
    }
}

LteEnbCphySapUser*
LteEnbRrc::GetLteEnbCphySapUser (uint8_t pos)
{
  NS_LOG_FUNCTION (this);
  return m_cphySapUser.at(pos);
}

bool
LteEnbRrc::HasUeManager (uint16_t rnti) const
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  std::map<uint16_t, Ptr<UeManager> >::const_iterator it = m_ueActiveMap.find (rnti);
  return (it != m_ueActiveMap.end ());
}

Ptr<UeManager>
LteEnbRrc::GetUeManagerbyRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  NS_ASSERT (0 != rnti);
  std::map<uint16_t, Ptr<UeManager> >::iterator it = m_ueActiveMap.find (rnti);
  if (it == m_ueActiveMap.end ())
    {
      // Stale RNTI: the UE has been moved to m_ueResumedMap or removed.
      NS_LOG_DEBUG ("GetUeManagerbyRnti: RNTI " << rnti << " not active (released/resumed)");
      return nullptr;
    }
  return it->second;
}

void
LteEnbRrc::DeferredReleaseNb (uint16_t rnti)
{
  // Deferred re-entry of the pre-release ACK-drain (UeManager::SwitchToResumeNb).
  // Looked up by RNTI: if the UE was removed, already released, or re-activated
  // (left CONNECTED_NORMALLY -- new data restarted the session and the normal
  // inactivity machinery owns the next release) this safely no-ops.
  Ptr<UeManager> um = GetUeManagerbyRnti (rnti);
  if (um != nullptr && um->GetState () == UeManager::CONNECTED_NORMALLY)
    {
      um->SwitchToResumeNb ();
    }
}

void
LteEnbRrc::FlushSrb1AfterReleaseNb (uint16_t rnti)
{
  Ptr<UeManager> um = GetUeManagerbyRnti (rnti);
  if (um != nullptr)
    {
      um->FlushSrb1ReleaseResidueNb ();
    }
}

Ptr<UeManager>
LteEnbRrc::GetUeManagerbyResumeId (uint64_t resumeId)
{
  NS_LOG_FUNCTION (this << (uint64_t) resumeId);
  NS_ASSERT (0 != resumeId);
  std::map<uint16_t, Ptr<UeManager> >::iterator it = m_ueResumedMap.find (resumeId);
  NS_ASSERT_MSG (it != m_ueResumedMap.end (), "UE manager for ResumeId" << resumeId << " not found");
  return it->second;
}
uint8_t
LteEnbRrc::AddUeMeasReportConfig (LteRrcSap::ReportConfigEutra config)
{
  NS_LOG_FUNCTION (this);

  // SANITY CHECK

  NS_ASSERT_MSG (m_ueMeasConfig.measIdToAddModList.size () == m_ueMeasConfig.reportConfigToAddModList.size (),
                 "Measurement identities and reporting configuration should not have different quantity");

  if (Simulator::Now () != Seconds (0))
    {
      NS_FATAL_ERROR ("AddUeMeasReportConfig may not be called after the simulation has run");
    }

  // INPUT VALIDATION

  switch (config.triggerQuantity)
    {
    case LteRrcSap::ReportConfigEutra::RSRP:
      if ((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5)
          && (config.threshold2.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRP))
        {
          NS_FATAL_ERROR ("The given triggerQuantity (RSRP) does not match with the given threshold2.choice");
        }

      if (((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A1)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A2)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A4)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5))
          && (config.threshold1.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRP))
        {
          NS_FATAL_ERROR ("The given triggerQuantity (RSRP) does not match with the given threshold1.choice");
        }
      break;

    case LteRrcSap::ReportConfigEutra::RSRQ:
      if ((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5)
          && (config.threshold2.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ))
        {
          NS_FATAL_ERROR ("The given triggerQuantity (RSRQ) does not match with the given threshold2.choice");
        }

      if (((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A1)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A2)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A4)
           || (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5))
          && (config.threshold1.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ))
        {
          NS_FATAL_ERROR ("The given triggerQuantity (RSRQ) does not match with the given threshold1.choice");
        }
      break;

    default:
      NS_FATAL_ERROR ("unsupported triggerQuantity");
      break;
    }

  if (config.purpose != LteRrcSap::ReportConfigEutra::REPORT_STRONGEST_CELLS)
    {
      NS_FATAL_ERROR ("Only REPORT_STRONGEST_CELLS purpose is supported");
    }

  if (config.reportQuantity != LteRrcSap::ReportConfigEutra::BOTH)
    {
      NS_LOG_WARN ("reportQuantity = BOTH will be used instead of the given reportQuantity");
    }

  uint8_t nextId = m_ueMeasConfig.reportConfigToAddModList.size () + 1;

  // create the reporting configuration
  LteRrcSap::ReportConfigToAddMod reportConfig;
  reportConfig.reportConfigId = nextId;
  reportConfig.reportConfigEutra = config;

  // create the measurement identity
  LteRrcSap::MeasIdToAddMod measId;
  measId.measId = nextId;
  measId.measObjectId = 1;
  measId.reportConfigId = nextId;

  // add both to the list of UE measurement configuration
  m_ueMeasConfig.reportConfigToAddModList.push_back (reportConfig);
  m_ueMeasConfig.measIdToAddModList.push_back (measId);

  return nextId;
}

void
LteEnbRrc::ConfigureCell (std::map<uint8_t, Ptr<ComponentCarrierBaseStation>> ccPhyConf)
{
  auto it = ccPhyConf.begin ();
  NS_ASSERT (it != ccPhyConf.end ());
  uint16_t ulBandwidth = it->second->GetUlBandwidth ();
  uint16_t dlBandwidth = it->second->GetDlBandwidth ();
  uint32_t ulEarfcn = it->second->GetUlEarfcn ();
  uint32_t dlEarfcn = it->second->GetDlEarfcn ();
  NS_LOG_FUNCTION (this << ulBandwidth << dlBandwidth
                        << ulEarfcn << dlEarfcn);
  NS_ASSERT (!m_configured);

  for (const auto &it: ccPhyConf)
    {
      m_cphySapProvider.at (it.first)->SetBandwidth (it.second->GetUlBandwidth (), it.second->GetDlBandwidth ());
      m_cphySapProvider.at (it.first)->SetEarfcn (it.second->GetUlEarfcn (), it.second->GetDlEarfcn ());
      m_cphySapProvider.at (it.first)->SetCellId (it.second->GetCellId ());
      m_cmacSapProvider.at (it.first)->ConfigureMac (it.second->GetUlBandwidth (), it.second->GetDlBandwidth ());
      if (m_ffrRrcSapProvider.size () > it.first)
        {
          m_ffrRrcSapProvider.at (it.first)->SetCellId (it.second->GetCellId ());
          m_ffrRrcSapProvider.at (it.first)->SetBandwidth (it.second->GetUlBandwidth (), it.second->GetDlBandwidth ());
        }
    }

  m_dlEarfcn = dlEarfcn;
  m_ulEarfcn = ulEarfcn;
  m_dlBandwidth = dlBandwidth;
  m_ulBandwidth = ulBandwidth;

  /*
   * Initializing the list of UE measurement configuration (m_ueMeasConfig).
   * Only intra-frequency measurements are supported, so only one measurement
   * object is created.
   */

  LteRrcSap::MeasObjectToAddMod measObject;
  measObject.measObjectId = 1;
  measObject.measObjectEutra.carrierFreq = m_dlEarfcn;
  measObject.measObjectEutra.allowedMeasBandwidth = m_dlBandwidth;
  measObject.measObjectEutra.presenceAntennaPort1 = false;
  measObject.measObjectEutra.neighCellConfig = 0;
  measObject.measObjectEutra.offsetFreq = 0;
  measObject.measObjectEutra.haveCellForWhichToReportCGI = false;

  m_ueMeasConfig.measObjectToAddModList.push_back (measObject);
  m_ueMeasConfig.haveQuantityConfig = true;
  m_ueMeasConfig.quantityConfig.filterCoefficientRSRP = m_rsrpFilterCoefficient;
  m_ueMeasConfig.quantityConfig.filterCoefficientRSRQ = m_rsrqFilterCoefficient;
  m_ueMeasConfig.haveMeasGapConfig = false;
  m_ueMeasConfig.haveSmeasure = false;
  m_ueMeasConfig.haveSpeedStatePars = false;
  if (m_legacy_lte){
    m_sib1.clear ();
    m_sib1.reserve (ccPhyConf.size ());
    for (const auto &it: ccPhyConf)
      {
        // Enabling MIB transmission
        LteRrcSap::MasterInformationBlock mib;
        mib.dlBandwidth = it.second->GetDlBandwidth ();
        mib.systemFrameNumber = 0;
        m_cphySapProvider.at (it.first)->SetMasterInformationBlock (mib);

        // Enabling SIB1 transmission with default values
        LteRrcSap::SystemInformationBlockType1 sib1;
        sib1.cellAccessRelatedInfo.cellIdentity = it.second->GetCellId ();
        sib1.cellAccessRelatedInfo.csgIndication = false;
        sib1.cellAccessRelatedInfo.csgIdentity = 0;
        sib1.cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity = 0; // not used
        sib1.cellSelectionInfo.qQualMin = -34; // not used, set as minimum value
        sib1.cellSelectionInfo.qRxLevMin = m_qRxLevMin; // set as minimum value
        m_sib1.push_back (sib1);
        m_cphySapProvider.at (it.first)->SetSystemInformationBlockType1 (sib1);
      }

  Simulator::Schedule (MilliSeconds (16), &LteEnbRrc::SendSystemInformation, this);
  }
  else{
    m_sib1Nb.clear();
    m_sib1Nb.reserve(ccPhyConf.size());
    for(const auto &it: ccPhyConf){
      NbIotRrcSap::MasterInformationBlockNb mibNb;
      m_cphySapProvider.at(it.first)->SetMasterInformationBlockNb(mibNb);

      NbIotRrcSap::SystemInformationBlockType1Nb sib1Nb;
      sib1Nb.cellAccessRelatedInfoNb.cellIdentity = it.second->GetCellId();
      sib1Nb.cellSelectionInfo.qRxLevMin = m_qRxLevMin; // set as minimum value
      m_sib1Nb.push_back(sib1Nb);
      m_cphySapProvider.at(it.first)->SetSystemInformationBlockType1Nb(sib1Nb);
    }
  Simulator::Schedule (MilliSeconds (16), &LteEnbRrc::SendSystemInformationNb, this);
  }
  /*
   * Enabling transmission of other SIB. The first time System Information is
   * transmitted is arbitrarily assumed to be at +0.016s, and then it will be
   * regularly transmitted every 80 ms by default (set the
   * SystemInformationPeriodicity attribute to configure this).
   */

  m_configured = true;

}


void
LteEnbRrc::SetCellId (uint16_t cellId)
{
  // update SIB1
  m_sib1.at (0).cellAccessRelatedInfo.cellIdentity = cellId;
  m_cphySapProvider.at (0)->SetSystemInformationBlockType1 (m_sib1.at (0));
}

void
LteEnbRrc::SetCellId (uint16_t cellId, uint8_t ccIndex)
{
  // update SIB1
  m_sib1.at (ccIndex).cellAccessRelatedInfo.cellIdentity = cellId;
  m_cphySapProvider.at (ccIndex)->SetSystemInformationBlockType1 (m_sib1.at (ccIndex));
}

uint8_t
LteEnbRrc::CellToComponentCarrierId (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);
  for (auto &it: m_componentCarrierPhyConf)
    {
      if (it.second->GetCellId () == cellId)
        {
          return it.first;
        }
    }
  NS_FATAL_ERROR ("Cell " << cellId << " not found in CC map");
}

uint16_t
LteEnbRrc::ComponentCarrierToCellId (uint8_t componentCarrierId)
{
  NS_LOG_FUNCTION (this << +componentCarrierId);
  return m_componentCarrierPhyConf.at (componentCarrierId)->GetCellId ();
}

bool
LteEnbRrc::SendData (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);

  EpsBearerTag tag;
  bool found = packet->RemovePacketTag (tag);
  NS_ASSERT_MSG (found, "no EpsBearerTag found in packet to be sent");

  if(m_ueActiveMap.find(tag.GetRnti()) != m_ueActiveMap.end()){
    // Ue is in Connected State, just send Packet
    Ptr<UeManager> ueManager = GetUeManagerbyRnti (tag.GetRnti ());
    ueManager->SendData (tag.GetBid (), packet);
  }
  else{
    // Device is in eDRX or PSM -> Store Packet for Imsi
    m_imsiSavedPacketsMap[tag.GetImsi()].push_back(std::pair<uint8_t, Ptr<Packet>>(tag.GetBid (), packet));
  }

  return true;
}

bool
LteEnbRrc::SendSavedPackets(uint64_t imsi, uint16_t rnti){

  for(std::vector<std::pair<uint8_t, Ptr<Packet>>>::iterator it = m_imsiSavedPacketsMap[imsi].begin(); it != m_imsiSavedPacketsMap[imsi].end(); ++it){
    GetUeManagerbyRnti(rnti)->SendData(it->first,it->second);
  }
  return true;
}

void
LteEnbRrc::SetForwardUpCallback (Callback <void, Ptr<Packet> > cb)
{
  m_forwardUpCallback = cb;
}

void
LteEnbRrc::ConnectionRequestTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  NS_ASSERT_MSG (GetUeManagerbyRnti (rnti)->GetState () == UeManager::INITIAL_RANDOM_ACCESS,
                 "ConnectionRequestTimeout in unexpected state " << ToString (GetUeManagerbyRnti (rnti)->GetState ()));
  m_rrcTimeoutTrace (GetUeManagerbyRnti (rnti)->GetImsi (), rnti,
                     ComponentCarrierToCellId (GetUeManagerbyRnti (rnti)->GetComponentCarrierId ()), "ConnectionRequestTimeout");
  RemoveUe (rnti);
}

void
LteEnbRrc::ConnectionSetupTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  // The UE may already be gone (resumed/removed) under capture/contention
  // resolution; GetUeManagerbyRnti then returns null.
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  if (ueManager == nullptr)
    {
      return;
    }
  NS_ASSERT_MSG (ueManager->GetState () == UeManager::CONNECTION_SETUP,
                 "ConnectionSetupTimeout in unexpected state " << ToString (ueManager->GetState ()));
  m_rrcTimeoutTrace (ueManager->GetImsi (), rnti,
                     ComponentCarrierToCellId (ueManager->GetComponentCarrierId ()), "ConnectionSetupTimeout");
  RemoveUe (rnti);
}

void
LteEnbRrc::ConnectionResumeTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  // By the time this fires the UE may already have CONFIRMED the resume (this timer was
  // cancelled in RecvRrcConnectionResumeCompletedNb and the state left CONNECTION_RESUME),
  // or the temp context may have been removed -- GetUeManagerbyRnti then returns null.
  // In either case there is nothing to re-park, so bail out. Only a context still stuck
  // in CONNECTION_RESUME reaches the re-park below.
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  if (ueManager == nullptr || ueManager->GetState () != UeManager::CONNECTION_RESUME)
    {
      return;
    }
  // The resume was accepted (CONNECTION_RESUME) but the UE never sent Resume-Complete:
  // under capture it lost the shared Temp C-RNTI and its Msg4 never landed. Re-PARK this
  // context to a clean IDLE_SUSPEND (keeping its still-valid resumeId, thanks to the
  // deferred-erase) so the UE's re-RACH resumes normally, instead of leaving a stranded
  // half-resumed context that would trap the UE (STALE=ABSENT) or force a fragile
  // cross-RNTI move. Timeout must be < UE T300 so this fires before the UE retries.
  ueManager->RePark ();
}
void
LteEnbRrc::ConnectionRejectedTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  if (ueManager == nullptr)
    {
      return;
    }
  NS_ASSERT_MSG (ueManager->GetState () == UeManager::CONNECTION_REJECTED,
                 "ConnectionRejectedTimeout in unexpected state " << ToString (ueManager->GetState ()));
  m_rrcTimeoutTrace (ueManager->GetImsi (), rnti,
                     ComponentCarrierToCellId (ueManager->GetComponentCarrierId ()), "ConnectionRejectedTimeout");
  RemoveUe (rnti);
}

void
LteEnbRrc::HandoverJoiningTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  NS_ASSERT_MSG (GetUeManagerbyRnti (rnti)->GetState () == UeManager::HANDOVER_JOINING,
                 "HandoverJoiningTimeout in unexpected state " << ToString (GetUeManagerbyRnti (rnti)->GetState ()));
  m_rrcTimeoutTrace (GetUeManagerbyRnti (rnti)->GetImsi (), rnti,
                     ComponentCarrierToCellId (GetUeManagerbyRnti (rnti)->GetComponentCarrierId ()), "HandoverJoiningTimeout");
  RemoveUe (rnti);
}

void
LteEnbRrc::HandoverLeavingTimeout (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  NS_ASSERT_MSG (GetUeManagerbyRnti (rnti)->GetState () == UeManager::HANDOVER_LEAVING,
                 "HandoverLeavingTimeout in unexpected state " << ToString (GetUeManagerbyRnti (rnti)->GetState ()));
  m_rrcTimeoutTrace (GetUeManagerbyRnti (rnti)->GetImsi (), rnti,
                     ComponentCarrierToCellId (GetUeManagerbyRnti (rnti)->GetComponentCarrierId ()), "HandoverLeavingTimeout");
  RemoveUe (rnti);
}

void
LteEnbRrc::SendHandoverRequest (uint16_t rnti, uint16_t cellId)
{
  NS_LOG_FUNCTION (this << rnti << cellId);
  NS_LOG_LOGIC ("Request to send HANDOVER REQUEST");
  NS_ASSERT (m_configured);

  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->PrepareHandover (cellId);

}

void
LteEnbRrc::DoCompleteSetupUe (uint16_t rnti, LteEnbRrcSapProvider::CompleteSetupUeParameters params)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->CompleteSetupUe (params);
}

void
LteEnbRrc::DoRecvRrcConnectionRequest (uint16_t rnti, LteRrcSap::RrcConnectionRequest msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionRequest (msg);
}

void
LteEnbRrc::DoRecvRrcConnectionResumeRequestNb (uint16_t rnti, NbIotRrcSap::RrcConnectionResumeRequestNb msg)
{
  NS_LOG_FUNCTION (this << rnti);
  // Contention resolution under capture (TS 36.321 5.1.5). Several UEs can adopt the
  // SAME Temporary C-RNTI from one captured RAR and all resume on it. Only the FIRST
  // resumer may claim this RNTI: once it is bound, the UeManager at this RNTI has left
  // INITIAL_RANDOM_ACCESS (the fresh-temp state). A later resume request arriving on an
  // already-claimed RNTI is a loser -- ignore it so it receives no Resume-Msg4, times
  // out, and re-RACHes with backoff onto a fresh C-RNTI. Without this guard, ResumeUe
  // rebinds the same RNTI to several UEs and they all transmit as one C-RNTI -> the
  // eNB reassembles multiple UEs' PDUs as one RNTI (RLC corruption / loss).
  if (HasUeManager (rnti)
      && GetUeManagerbyRnti (rnti)->GetState () != UeManager::INITIAL_RANDOM_ACCESS)
    {
      NS_LOG_INFO ("Resume contention: RNTI " << rnti
                   << " already claimed by a resumed UE; ignoring duplicate (loser re-RACHes)");
      if (NbIotDebugTrace ())
        {
          bool hasEdt = (msg.dedicatedInfoNas != nullptr && msg.dedicatedInfoNas->GetSize () > 0);
          std::cout << "[ENB-CONTENTION-LOSER] t=" << Simulator::Now ().GetSeconds () << " rnti=" << rnti
                    << " reqResumeId=" << msg.resumeIdentity
                    << " carriedMsg3Data=" << (hasEdt ? "YES" : "no")
                    << " -> REJECTED before forward (data NOT sent upstream)" << std::endl;
        }
      return;
    }
  // If the resumeId is unknown/consumed or resume isn't admitted, ResumeUe does NOT run, so the
  // UeManager stays in INITIAL_RANDOM_ACCESS. In that case we must NOT call the handler below --
  // its switch has no INITIAL_RANDOM_ACCESS case and would NS_FATAL ("method unexpected in
  // state"). This bites under CAPTURE: colliders share a Temp C-RNTI, and a loser's resume
  // (unknown/already-consumed resumeId) reaches here in INITIAL_RANDOM_ACCESS. Return instead:
  // no Msg4 is sent, the UE times out (T300) and re-RACHes with backoff.
  if (!(DoCheckIfResumeIdExists (msg.resumeIdentity) && m_admitRrcConnectionResumeRequest))
    {
      if (NbIotDebugTrace ())
        {
          auto sit = m_ueResumedMap.find (msg.resumeIdentity);
          const char *why = (sit == m_ueResumedMap.end ()) ? "ABSENT(erased/never-issued)"
                            : (sit->second == nullptr)      ? "NULL-PLACEHOLDER(parked,persistentGrant)"
                                                            : "present-but-not-admitted";
          std::cout << "[ENB-STALE-RESUMEID] t=" << Simulator::Now ().GetSeconds () << " rnti=" << rnti
                    << " reqResumeId=" << msg.resumeIdentity
                    << " reason=" << why
                    << " (no resume, UE re-RACHes)" << std::endl;
        }
      return;
    }
  ResumeUe (rnti, msg.resumeIdentity);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionResumeRequestNb (msg);
}

void
LteEnbRrc::DoRecvRrcEarlyDataRequestNb (uint16_t rnti, NbIotRrcSap::RrcEarlyDataRequestNb msg)
{
    //NS_LOG_FUNCTION (this << rnti);
    // Contention resolution under capture (TS 36.321 5.1.5), EDT twin of the resume-request
    // guard in DoRecvRrcConnectionResumeRequestNb. Several UEs can adopt the SAME captured
    // Temp C-RNTI and mix EDT with full-resume on it -> ONE shared UeManager. Only the FIRST
    // claimant may drive it; once bound it has left INITIAL_RANDOM_ACCESS. An EDT request
    // arriving on an already-claimed RNTI is a loser: ignore it (no Msg4 -> T300 -> re-RACH
    // with backoff onto a fresh C-RNTI). Without this, the EDT request lands on a UeManager
    // already in CONNECTION_RESUME/CONNECTED_NORMALLY and drives an illegal state transition
    // -> NS_FATAL_ERROR "method unexpected in state" (enb-rrc RecvRrc*Nb default cases).
    if (HasUeManager (rnti)
        && GetUeManagerbyRnti (rnti)->GetState () != UeManager::INITIAL_RANDOM_ACCESS)
      {
        NS_LOG_INFO ("EDT contention: RNTI " << rnti
                     << " already claimed by another UE; ignoring EDT request (loser re-RACHes)");
        return;
      }
    GetUeManagerbyRnti (rnti)->RecvRrcEarlyDataRequestNb (msg);

}

bool
LteEnbRrc::DoCheckIfResumeIdExists(uint64_t resumeId){
  // Must check the VALUE, not just the key. DoAllocateTemporaryResumeId reserves the id with a
  // null placeholder (m_ueResumedMap[id]=0) and only MoveUeToResumed populates the real
  // UeManager -- and for persistent-grant UEs MoveUeToResumed is never scheduled (SwitchToResumeNb
  // parks the context and returns), so the placeholder stays null. Treating a null-context id as
  // "resumable" makes ResumeUe dereference a null UeManager -> crash. Require a live context.
  auto it = m_ueResumedMap.find (resumeId);
  return (it != m_ueResumedMap.end () && it->second != nullptr);
}

void
LteEnbRrc::DoRecvRrcConnectionSetupCompleted (uint16_t rnti, LteRrcSap::RrcConnectionSetupCompleted msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionSetupCompleted (msg);
}

void
LteEnbRrc::DoRecvRrcConnectionResumeCompletedNb (uint16_t rnti, NbIotRrcSap::RrcConnectionResumeCompleteNb msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionResumeCompletedNb (msg);
}

void
LteEnbRrc::DoRecvRrcConnectionReconfigurationCompleted (uint16_t rnti, LteRrcSap::RrcConnectionReconfigurationCompleted msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionReconfigurationCompleted (msg);
}

void
LteEnbRrc::DoRecvRrcConnectionReestablishmentRequest (uint16_t rnti, LteRrcSap::RrcConnectionReestablishmentRequest msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionReestablishmentRequest (msg);
}

void
LteEnbRrc::DoRecvRrcConnectionReestablishmentComplete (uint16_t rnti, LteRrcSap::RrcConnectionReestablishmentComplete msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvRrcConnectionReestablishmentComplete (msg);
}

void
LteEnbRrc::DoRecvMeasurementReport (uint16_t rnti, LteRrcSap::MeasurementReport msg)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvMeasurementReport (msg);
}

void
LteEnbRrc::DoInitialContextSetupRequest (EpcEnbS1SapUser::InitialContextSetupRequestParameters msg)
{
  NS_LOG_FUNCTION (this);
  std::map<uint16_t, Ptr<UeManager> >::iterator it = m_ueActiveMap.find (msg.rnti);
  if(it != m_ueActiveMap.end()){
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (msg.rnti);
  ueManager->InitialContextSetupRequest ();
  }
}

void
LteEnbRrc::DoRecvIdealUeContextRemoveRequest (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  GetUeManagerbyRnti (rnti)->RecvIdealUeContextRemoveRequest (rnti);
  //delete the UE context at the eNB
  RemoveUe (rnti);
}

void
LteEnbRrc::DoDataRadioBearerSetupRequest (EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters request)
{
  NS_LOG_FUNCTION (this);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (request.rnti);
  ueManager->SetupDataRadioBearer (request.bearer, request.bearerId, request.gtpTeid, request.transportLayerAddress);
}

void
LteEnbRrc::DoPathSwitchRequestAcknowledge (EpcEnbS1SapUser::PathSwitchRequestAcknowledgeParameters params)
{
  NS_LOG_FUNCTION (this);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (params.rnti);
  ueManager->SendUeContextRelease ();
}

void
LteEnbRrc::DoRecvHandoverRequest (EpcX2SapUser::HandoverRequestParams req)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: HANDOVER REQUEST");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << req.oldEnbUeX2apId);
  NS_LOG_LOGIC ("sourceCellId = " << req.sourceCellId);
  NS_LOG_LOGIC ("targetCellId = " << req.targetCellId);
  NS_LOG_LOGIC ("mmeUeS1apId = " << req.mmeUeS1apId);

  if (m_admitHandoverRequest == false)
    {
      NS_LOG_INFO ("rejecting handover request from cellId " << req.sourceCellId);
      EpcX2Sap::HandoverPreparationFailureParams res;
      res.oldEnbUeX2apId =  req.oldEnbUeX2apId;
      res.sourceCellId = req.sourceCellId;
      res.targetCellId = req.targetCellId;
      res.cause = 0;
      res.criticalityDiagnostics = 0;
      m_x2SapProvider->SendHandoverPreparationFailure (res);
      return;
    }

  uint16_t rnti = AddUe (UeManager::HANDOVER_JOINING, CellToComponentCarrierId (req.targetCellId));
  LteEnbCmacSapProvider::AllocateNcRaPreambleReturnValue anrcrv = m_cmacSapProvider.at (0)->AllocateNcRaPreamble (rnti);
  if (anrcrv.valid == false)
    {
      NS_LOG_INFO (this << " failed to allocate a preamble for non-contention based RA => cannot accept HO");
      RemoveUe (rnti);
      NS_FATAL_ERROR ("should trigger HO Preparation Failure, but it is not implemented");
      return;
    }

  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->SetSource (req.sourceCellId, req.oldEnbUeX2apId);
  ueManager->SetImsi (req.mmeUeS1apId);

  EpcX2SapProvider::HandoverRequestAckParams ackParams;
  ackParams.oldEnbUeX2apId = req.oldEnbUeX2apId;
  ackParams.newEnbUeX2apId = rnti;
  ackParams.sourceCellId = req.sourceCellId;
  ackParams.targetCellId = req.targetCellId;

  for (std::vector <EpcX2Sap::ErabToBeSetupItem>::iterator it = req.bearers.begin ();
       it != req.bearers.end ();
       ++it)
    {
      ueManager->SetupDataRadioBearer (it->erabLevelQosParameters, it->erabId, it->gtpTeid, it->transportLayerAddress);
      EpcX2Sap::ErabAdmittedItem i;
      i.erabId = it->erabId;
      ackParams.admittedBearers.push_back (i);
    }

  LteRrcSap::RrcConnectionReconfiguration handoverCommand = ueManager->GetRrcConnectionReconfigurationForHandover ();
  handoverCommand.haveMobilityControlInfo = true;
  handoverCommand.mobilityControlInfo.targetPhysCellId = req.targetCellId;
  handoverCommand.mobilityControlInfo.haveCarrierFreq = true;
  handoverCommand.mobilityControlInfo.carrierFreq.dlCarrierFreq = m_dlEarfcn;
  handoverCommand.mobilityControlInfo.carrierFreq.ulCarrierFreq = m_ulEarfcn;
  handoverCommand.mobilityControlInfo.haveCarrierBandwidth = true;
  handoverCommand.mobilityControlInfo.carrierBandwidth.dlBandwidth = m_dlBandwidth;
  handoverCommand.mobilityControlInfo.carrierBandwidth.ulBandwidth = m_ulBandwidth;
  handoverCommand.mobilityControlInfo.newUeIdentity = rnti;
  handoverCommand.mobilityControlInfo.haveRachConfigDedicated = true;
  handoverCommand.mobilityControlInfo.rachConfigDedicated.raPreambleIndex = anrcrv.raPreambleId;
  handoverCommand.mobilityControlInfo.rachConfigDedicated.raPrachMaskIndex = anrcrv.raPrachMaskIndex;

  LteEnbCmacSapProvider::RachConfig rc = m_cmacSapProvider.at (0)->GetRachConfig ();
  handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
  handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
  handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;
  handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.txFailParam.connEstFailCount = rc.connEstFailCount;

  Ptr<Packet> encodedHandoverCommand = m_rrcSapUser->EncodeHandoverCommand (handoverCommand);

  ackParams.rrcContext = encodedHandoverCommand;

  NS_LOG_LOGIC ("Send X2 message: HANDOVER REQUEST ACK");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << ackParams.oldEnbUeX2apId);
  NS_LOG_LOGIC ("newEnbUeX2apId = " << ackParams.newEnbUeX2apId);
  NS_LOG_LOGIC ("sourceCellId = " << ackParams.sourceCellId);
  NS_LOG_LOGIC ("targetCellId = " << ackParams.targetCellId);

  m_x2SapProvider->SendHandoverRequestAck (ackParams);
}

void
LteEnbRrc::DoRecvHandoverRequestAck (EpcX2SapUser::HandoverRequestAckParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: HANDOVER REQUEST ACK");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
  NS_LOG_LOGIC ("newEnbUeX2apId = " << params.newEnbUeX2apId);
  NS_LOG_LOGIC ("sourceCellId = " << params.sourceCellId);
  NS_LOG_LOGIC ("targetCellId = " << params.targetCellId);

  uint16_t rnti = params.oldEnbUeX2apId;
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->RecvHandoverRequestAck (params);
}

void
LteEnbRrc::DoRecvHandoverPreparationFailure (EpcX2SapUser::HandoverPreparationFailureParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: HANDOVER PREPARATION FAILURE");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
  NS_LOG_LOGIC ("sourceCellId = " << params.sourceCellId);
  NS_LOG_LOGIC ("targetCellId = " << params.targetCellId);
  NS_LOG_LOGIC ("cause = " << params.cause);
  NS_LOG_LOGIC ("criticalityDiagnostics = " << params.criticalityDiagnostics);

  uint16_t rnti = params.oldEnbUeX2apId;
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->RecvHandoverPreparationFailure (params.targetCellId);
}

void
LteEnbRrc::DoRecvSnStatusTransfer (EpcX2SapUser::SnStatusTransferParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: SN STATUS TRANSFER");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
  NS_LOG_LOGIC ("newEnbUeX2apId = " << params.newEnbUeX2apId);
  NS_LOG_LOGIC ("erabsSubjectToStatusTransferList size = " << params.erabsSubjectToStatusTransferList.size ());

  uint16_t rnti = params.newEnbUeX2apId;
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->RecvSnStatusTransfer (params);
}

void
LteEnbRrc::DoRecvUeContextRelease (EpcX2SapUser::UeContextReleaseParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: UE CONTEXT RELEASE");

  NS_LOG_LOGIC ("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
  NS_LOG_LOGIC ("newEnbUeX2apId = " << params.newEnbUeX2apId);

  uint16_t rnti = params.oldEnbUeX2apId;
  GetUeManagerbyRnti (rnti)->RecvUeContextRelease (params);
  RemoveUe (rnti);
}

void
LteEnbRrc::DoRecvLoadInformation (EpcX2SapUser::LoadInformationParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: LOAD INFORMATION");

  NS_LOG_LOGIC ("Number of cellInformationItems = " << params.cellInformationList.size ());

  NS_ABORT_IF (m_ffrRrcSapProvider.size () == 0);
  m_ffrRrcSapProvider.at (0)->RecvLoadInformation (params);
}

void
LteEnbRrc::DoRecvResourceStatusUpdate (EpcX2SapUser::ResourceStatusUpdateParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv X2 message: RESOURCE STATUS UPDATE");

  NS_LOG_LOGIC ("Number of cellMeasurementResultItems = " << params.cellMeasurementResultList.size ());

  NS_ASSERT ("Processing of RESOURCE STATUS UPDATE X2 message IS NOT IMPLEMENTED");
}

void
LteEnbRrc::DoRecvUeData (EpcX2SapUser::UeDataParams params)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Recv UE DATA FORWARDING through X2 interface");
  NS_LOG_LOGIC ("sourceCellId = " << params.sourceCellId);
  NS_LOG_LOGIC ("targetCellId = " << params.targetCellId);
  NS_LOG_LOGIC ("gtpTeid = " << params.gtpTeid);
  NS_LOG_LOGIC ("ueData = " << params.ueData);
  NS_LOG_LOGIC ("ueData size = " << params.ueData->GetSize ());

  std::map<uint32_t, X2uTeidInfo>::iterator
    teidInfoIt = m_x2uTeidInfoMap.find (params.gtpTeid);
  if (teidInfoIt != m_x2uTeidInfoMap.end ())
    {
      GetUeManagerbyRnti (teidInfoIt->second.rnti)->SendData (teidInfoIt->second.drbid, params.ueData);
    }
  else
    {
      NS_FATAL_ERROR ("X2-U data received but no X2uTeidInfo found");
    }
}


uint16_t
LteEnbRrc::DoAllocateTemporaryCellRnti (uint8_t componentCarrierId)
{
  NS_LOG_FUNCTION (this << +componentCarrierId);
  return AddUe (UeManager::INITIAL_RANDOM_ACCESS, componentCarrierId);
}

void
LteEnbRrc::DoRrcConfigurationUpdateInd (LteEnbCmacSapUser::UeConfig cmacParams)
{
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (cmacParams.m_rnti);
  ueManager->CmacUeConfigUpdateInd (cmacParams);
}

void
LteEnbRrc::DoNotifyLcConfigResult (uint16_t rnti, uint8_t lcid, bool success)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  NS_FATAL_ERROR ("not implemented");
}

void
LteEnbRrc::DoNotifyDataInactivityNb(uint16_t rnti, uint8_t lcid)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti(rnti);
  ueManager->NotifyDataInactivityNb(lcid);
}

void
LteEnbRrc::DoNotifyDataInactivitySchedulerNb(uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  // The MAC schedules this callback for subframestillDataInactivity ms ahead;
  // the UeManager may have moved to m_ueResumedMap before it fires.
  if (!HasUeManager (rnti))
    {
      NS_LOG_DEBUG ("DoNotifyDataInactivitySchedulerNb: RNTI " << rnti
                    << " already released/resumed, ignoring");
      return;
    }
  Ptr<UeManager> ueManager = GetUeManagerbyRnti(rnti);
  ueManager->NotifyDataInactivitySchedulerNb();
}
void
LteEnbRrc::DoNotifyReleaseAssistanceNb(uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  if (!HasUeManager (rnti))
    {
      NS_LOG_DEBUG ("DoNotifyReleaseAssistanceNb: RNTI " << rnti
                    << " already released/resumed, ignoring");
      return;
    }
  GetUeManagerbyRnti(rnti)->ReleaseOnRai();
}
void
LteEnbRrc::DoNotifyDataActivitySchedulerNb(uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  if (!HasUeManager (rnti))
    {
      NS_LOG_DEBUG ("DoNotifyDataActivitySchedulerNb: RNTI " << rnti
                    << " already released/resumed, ignoring");
      return;
    }
  Ptr<UeManager> ueManager = GetUeManagerbyRnti(rnti);
  ueManager->NotifyDataActivitySchedulerNb();
}

void
LteEnbRrc::DoNotifyUlDataObservedNb(uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  if (!HasUeManager (rnti))
    {
      return;
    }
  Ptr<UeManager> ueManager = GetUeManagerbyRnti(rnti);
  ueManager->NotifyUlDataObservedNb();
}
bool
LteEnbRrc::DoIsUeInReleaseGraceNb(uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  if (!HasUeManager (rnti))
    {
      return false;
    }
  return GetUeManagerbyRnti (rnti)->InReleaseGraceNb ();
}
uint8_t
LteEnbRrc::DoAddUeMeasReportConfigForHandover (LteRrcSap::ReportConfigEutra reportConfig)
{
  NS_LOG_FUNCTION (this);
  uint8_t measId = AddUeMeasReportConfig (reportConfig);
  m_handoverMeasIds.insert (measId);
  return measId;
}

uint8_t
LteEnbRrc::DoAddUeMeasReportConfigForComponentCarrier (LteRrcSap::ReportConfigEutra reportConfig)
{
  NS_LOG_FUNCTION (this);
  uint8_t measId = AddUeMeasReportConfig (reportConfig);
  m_componentCarrierMeasIds.insert (measId);
  return measId;
}

void
LteEnbRrc::DoSetNumberOfComponentCarriers (uint16_t numberOfComponentCarriers)
{
  m_numberOfComponentCarriers = numberOfComponentCarriers;
}

void
LteEnbRrc::DoTriggerHandover (uint16_t rnti, uint16_t targetCellId)
{
  NS_LOG_FUNCTION (this << rnti << targetCellId);

  bool isHandoverAllowed = true;

  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  NS_ASSERT_MSG (ueManager != 0, "Cannot find UE context with RNTI " << rnti);

  if (m_anrSapProvider != 0)
    {
      // ensure that proper neighbour relationship exists between source and target cells
      bool noHo = m_anrSapProvider->GetNoHo (targetCellId);
      bool noX2 = m_anrSapProvider->GetNoX2 (targetCellId);
      NS_LOG_DEBUG (this << " cellId=" << ComponentCarrierToCellId (ueManager->GetComponentCarrierId ())
                         << " targetCellId=" << targetCellId
                         << " NRT.NoHo=" << noHo << " NRT.NoX2=" << noX2);

      if (noHo || noX2)
        {
          isHandoverAllowed = false;
          NS_LOG_LOGIC (this << " handover to cell " << targetCellId
                             << " is not allowed by ANR");
        }
    }

  if (ueManager->GetState () != UeManager::CONNECTED_NORMALLY)
    {
      isHandoverAllowed = false;
      NS_LOG_LOGIC (this << " handover is not allowed because the UE"
                         << " rnti=" << rnti << " is in "
                         << ToString (ueManager->GetState ()) << " state");
    }

  if (isHandoverAllowed)
    {
      // initiate handover execution
      ueManager->PrepareHandover (targetCellId);
    }
}

uint8_t
LteEnbRrc::DoAddUeMeasReportConfigForAnr (LteRrcSap::ReportConfigEutra reportConfig)
{
  NS_LOG_FUNCTION (this);
  uint8_t measId = AddUeMeasReportConfig (reportConfig);
  m_anrMeasIds.insert (measId);
  return measId;
}

uint8_t
LteEnbRrc::DoAddUeMeasReportConfigForFfr (LteRrcSap::ReportConfigEutra reportConfig)
{
  NS_LOG_FUNCTION (this);
  uint8_t measId = AddUeMeasReportConfig (reportConfig);
  m_ffrMeasIds.insert (measId);
  return measId;
}

void
LteEnbRrc::DoSetPdschConfigDedicated (uint16_t rnti, LteRrcSap::PdschConfigDedicated pdschConfigDedicated)
{
  NS_LOG_FUNCTION (this);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  ueManager->SetPdschConfigDedicated (pdschConfigDedicated);
}

void
LteEnbRrc::DoSendLoadInformation (EpcX2Sap::LoadInformationParams params)
{
  NS_LOG_FUNCTION (this);

  m_x2SapProvider->SendLoadInformation(params);
}

uint16_t
LteEnbRrc::AddUe (UeManager::State state, uint8_t componentCarrierId)
{
  NS_LOG_FUNCTION (this);
  bool found = false;
  uint16_t rnti;
  for (rnti = m_lastAllocatedRnti + 1;  (rnti != m_lastAllocatedRnti - 1) && (!found); ++rnti)
    {
      if ((rnti != 0) && (m_ueActiveMap.find (rnti) == m_ueActiveMap.end ()))
        {
          found = true;
          break;
        }
    }

  NS_ASSERT_MSG (found, "no more RNTIs available (do you have more than 65535 UEs in a cell?)");
  m_lastAllocatedRnti = rnti;
  Ptr<UeManager> ueManager = CreateObject<UeManager> (this, rnti, state, componentCarrierId);
  m_ccmRrcSapProvider-> AddUe (rnti, (uint8_t)state);
  m_ueActiveMap.insert (std::pair<uint16_t, Ptr<UeManager> > (rnti, ueManager));
  ueManager->Initialize ();
  const uint16_t cellId = ComponentCarrierToCellId (componentCarrierId);
  NS_LOG_DEBUG (this << "New UE RNTI:" << rnti << ", cellId:" << cellId << ", srs CI:" << ueManager->GetSrsConfigurationIndex () << ", IMSI:" << ueManager->GetImsi() );
  m_newUeContextTrace (cellId, rnti);
  return rnti;
}

uint64_t
LteEnbRrc::DoAllocateTemporaryResumeId()
{
  NS_LOG_FUNCTION (this);
  uint64_t resumeId;
  uint64_t maxresumeId = uint64_t(std::pow(2, 40)-1);

  for(resumeId = 1; resumeId < maxresumeId; resumeId++){
    if ((resumeId != 0) && (m_ueResumedMap.find(resumeId) == m_ueResumedMap.end())){
      m_ueResumedMap[resumeId] = 0;
      m_edtDataForwarded.erase (resumeId); // fresh id: clear any stale EDT-forwarded flag
      break;
    }
  }
  return resumeId;

}

void
LteEnbRrc::MoveUeToResumed(uint16_t rnti, uint64_t resumeId){

  // Store information of old UeManager
  m_ueResumedMap[resumeId] = GetUeManagerbyRnti(rnti);
  m_cmacSapProvider.at(0)->MoveUeToResume(rnti, resumeId);
  m_ccmRrcSapProvider->MoveUeToResume(rnti, resumeId);
  m_rrcSapUser->MoveUeToResume(rnti,resumeId);
  m_s1SapProvider->MoveUeToResume(rnti, resumeId);
  m_cmacSapProvider.at(0)->RemoveUeFromScheduler(rnti);
  RemoveUeNb(rnti, true);
}

void
LteEnbRrc::ResumeUe(uint16_t rnti, uint64_t resumeId){

  NS_LOG_DEBUG ("UE is resumed. RNTI:" << rnti << ", resumeId:" << resumeId );
  // Remove parts of new Temporary UeManager that arent needed
  std::map <uint16_t, Ptr<UeManager> >::iterator it = m_ueActiveMap.find (rnti);
  it->second->CancelPendingEvents ();//cancel pending events
  m_ueActiveMap.erase (it);
  m_cmacSapProvider.at (0)->RemoveUe (rnti);
  m_ccmRrcSapProvider-> RemoveUe (rnti);
  m_rrcSapUser->RemoveUe (rnti); // Remove UE context at RRC protocol
  m_s1SapProvider->UeContextRelease(rnti);
  // Resume Old UeManager
  m_ueActiveMap[rnti] = m_ueResumedMap[resumeId];
  NS_LOG_DEBUG("Old UE is resumed RNTI:" << rnti << ", IMSI:" << m_ueResumedMap[resumeId]->GetImsi());
  m_ueActiveMap[rnti]->SetRnti(rnti);  // Set RNTI
  m_cmacSapProvider.at(0)->ResumeUe(rnti, resumeId);
  m_rrcSapUser->ResumeUe(rnti,resumeId);
  m_ccmRrcSapProvider->ResumeUe(rnti, resumeId);
  m_s1SapProvider->ResumeUe(rnti, resumeId);
  // Token NOT erased here: keep the resumeId valid until the resume is CONFIRMED
  // (RecvRrcConnectionResumeCompletedNb). A capture loser whose Msg4 didn't land keeps a
  // valid token; when its resume attempt times out the eNB RE-PARKS this context to a
  // clean IDLE_SUSPEND (ConnectionResumeTimeout -> UeManager::RePark), so the UE's re-RACH
  // resumes fresh instead of hitting STALE=ABSENT. Single-use consumption happens on
  // confirm, so the parked context here is always freshly IDLE_SUSPEND, never mid-resume.
}
void
LteEnbRrc::RemoveUe (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  std::map <uint16_t, Ptr<UeManager> >::iterator it = m_ueActiveMap.find (rnti);
  NS_ASSERT_MSG (it != m_ueActiveMap.end (), "request to remove UE info with unknown rnti " << rnti);
  NS_LOG_DEBUG (this << "Request to remove UE info with RNTI " << rnti);
  uint64_t imsi = it->second->GetImsi ();
  uint16_t srsCi = (*it).second->GetSrsConfigurationIndex ();
  //cancel pending events
  it->second->CancelPendingEvents ();
  // fire trace upon connection release
  m_connectionReleaseTrace (imsi, ComponentCarrierToCellId (it->second->GetComponentCarrierId ()), rnti);
  m_ueActiveMap.erase (it);
  for (uint8_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
      m_cmacSapProvider.at (i)->RemoveUe (rnti);
      m_cphySapProvider.at (i)->RemoveUe (rnti);
    }
  if (m_s1SapProvider != 0)
    {
      m_s1SapProvider->UeContextRelease (rnti);
    }
  m_ccmRrcSapProvider-> RemoveUe (rnti);
  // need to do this after UeManager has been deleted
  if (srsCi != 0)
    {
      RemoveSrsConfigurationIndex (srsCi);
    }

  m_rrcSapUser->RemoveUe (rnti); // Remove UE context at RRC protocol
}
void
LteEnbRrc::RemoveUeNb(uint16_t rnti, bool resumed)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  std::map <uint16_t, Ptr<UeManager> >::iterator it = m_ueActiveMap.find (rnti);
  NS_ASSERT_MSG (it != m_ueActiveMap.end (), "request to remove UE info with unknown rnti " << rnti);
  uint64_t imsi = it->second->GetImsi ();
  uint16_t srsCi = (*it).second->GetSrsConfigurationIndex ();
  //cancel pending events
  it->second->CancelPendingEvents ();
  // fire trace upon connection release
  m_connectionReleaseTrace (imsi, ComponentCarrierToCellId (it->second->GetComponentCarrierId ()), rnti);
  m_ueActiveMap.erase (it);
  for (uint8_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
      m_cmacSapProvider.at (i)->RemoveUe (rnti);
      m_cphySapProvider.at (i)->RemoveUe (rnti);
    }
  if (m_s1SapProvider != 0)
    {
      m_s1SapProvider->UeContextRelease (rnti);
    }
  m_ccmRrcSapProvider-> RemoveUe (rnti);
  // need to do this after UeManager has been deleted
  if (srsCi != 0)
    {
      RemoveSrsConfigurationIndex (srsCi);
    }
  NS_LOG_DEBUG ("LteEnbRrc::RemoveUeNb RNTI:" << rnti << ", IMSI:" << imsi);
  m_rrcSapUser->RemoveUe (rnti,resumed); // Remove UE context at RRC protocol
}
TypeId
LteEnbRrc::GetRlcType (EpsBearer bearer)
{
  switch (m_epsBearerToRlcMapping)
    {
    case RLC_SM_ALWAYS:
      return LteRlcSm::GetTypeId ();
      break;

    case  RLC_UM_ALWAYS:
      return LteRlcUm::GetTypeId ();
      break;

    case RLC_AM_ALWAYS:
      return LteRlcAm::GetTypeId ();
      break;

    case PER_BASED:
      if (bearer.GetPacketErrorLossRate () > 1.0e-5)
        {
          return LteRlcUm::GetTypeId ();
        }
      else
        {
          return LteRlcAm::GetTypeId ();
        }
      break;

    default:
      return LteRlcSm::GetTypeId ();
      break;
    }
}


void
LteEnbRrc::AddX2Neighbour (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);

  if (m_anrSapProvider != 0)
    {
      m_anrSapProvider->AddNeighbourRelation (cellId);
    }
}

void
LteEnbRrc::SetCsgId (uint32_t csgId, bool csgIndication)
{
  NS_LOG_FUNCTION (this << csgId << csgIndication);
  for (uint8_t componentCarrierId = 0; componentCarrierId < m_sib1.size (); componentCarrierId++)
    {
      m_sib1.at (componentCarrierId).cellAccessRelatedInfo.csgIdentity = csgId;
      m_sib1.at (componentCarrierId).cellAccessRelatedInfo.csgIndication = csgIndication;
      m_cphySapProvider.at (componentCarrierId)->SetSystemInformationBlockType1 (m_sib1.at (componentCarrierId));
    }
}

/// Number of distinct SRS periodicity plus one.
static const uint8_t SRS_ENTRIES = 9;
/**
 * Sounding Reference Symbol (SRS) periodicity (TSRS) in milliseconds. Taken
 * from 3GPP TS 36.213 Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsPeriodicity[SRS_ENTRIES] = {0, 2, 5, 10, 20, 40,  80, 160, 320};
/**
 * The lower bound (inclusive) of the SRS configuration indices (ISRS) which
 * use the corresponding SRS periodicity (TSRS). Taken from 3GPP TS 36.213
 * Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsCiLow[SRS_ENTRIES] =       {0, 0, 2,  7, 17, 37,  77, 157, 317};
/**
 * The upper bound (inclusive) of the SRS configuration indices (ISRS) which
 * use the corresponding SRS periodicity (TSRS). Taken from 3GPP TS 36.213
 * Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsCiHigh[SRS_ENTRIES] =      {0, 1, 6, 16, 36, 76, 156, 316, 636};

void
LteEnbRrc::SetSrsPeriodicity (uint32_t p)
{
  NS_LOG_FUNCTION (this << p);
  for (uint32_t id = 1; id < SRS_ENTRIES; ++id)
    {
      if (g_srsPeriodicity[id] == p)
        {
          m_srsCurrentPeriodicityId = id;
          return;
        }
    }
  // no match found
  std::ostringstream allowedValues;
  for (uint32_t id = 1; id < SRS_ENTRIES; ++id)
    {
      allowedValues << g_srsPeriodicity[id] << " ";
    }
  NS_FATAL_ERROR ("illecit SRS periodicity value " << p << ". Allowed values: " << allowedValues.str ());
}

uint32_t
LteEnbRrc::GetSrsPeriodicity () const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_srsCurrentPeriodicityId > 0);
  NS_ASSERT (m_srsCurrentPeriodicityId < SRS_ENTRIES);
  return g_srsPeriodicity[m_srsCurrentPeriodicityId];
}


uint16_t
LteEnbRrc::GetNewSrsConfigurationIndex ()
{
  //NS_LOG_FUNCTION (this << m_ueSrsConfigurationIndexSet.size ());
  //// SRS
  //NS_ASSERT (m_srsCurrentPeriodicityId > 0);
  //NS_ASSERT (m_srsCurrentPeriodicityId < SRS_ENTRIES);
  //NS_LOG_DEBUG (this << " SRS p " << g_srsPeriodicity[m_srsCurrentPeriodicityId] << " set " << m_ueSrsConfigurationIndexSet.size ());
  //if (m_ueSrsConfigurationIndexSet.size () >= g_srsPeriodicity[m_srsCurrentPeriodicityId])
  //  {
  //    NS_FATAL_ERROR ("too many UEs (" << m_ueSrsConfigurationIndexSet.size () + 1
  //                                     << ") for current SRS periodicity "
  //                                     <<  g_srsPeriodicity[m_srsCurrentPeriodicityId]
  //                                     << ", consider increasing the value of ns3::LteEnbRrc::SrsPeriodicity");
  //  }

  //if (m_ueSrsConfigurationIndexSet.empty ())
  //  {
  //    // first entry
  //    m_lastAllocatedConfigurationIndex = g_srsCiLow[m_srsCurrentPeriodicityId];
  //    m_ueSrsConfigurationIndexSet.insert (m_lastAllocatedConfigurationIndex);
  //  }
  //else
  //  {
  //    // find a CI from the available ones
  //    std::set<uint16_t>::reverse_iterator rit = m_ueSrsConfigurationIndexSet.rbegin ();
  //    NS_ASSERT (rit != m_ueSrsConfigurationIndexSet.rend ());
  //    NS_LOG_DEBUG (this << " lower bound " << (*rit) << " of " << g_srsCiHigh[m_srsCurrentPeriodicityId]);
  //    if ((*rit) < g_srsCiHigh[m_srsCurrentPeriodicityId])
  //      {
  //        // got it from the upper bound
  //        m_lastAllocatedConfigurationIndex = (*rit) + 1;
  //        m_ueSrsConfigurationIndexSet.insert (m_lastAllocatedConfigurationIndex);
  //      }
  //    else
  //      {
  //        // look for released ones
  //        for (uint16_t srcCi = g_srsCiLow[m_srsCurrentPeriodicityId]; srcCi < g_srsCiHigh[m_srsCurrentPeriodicityId]; srcCi++)
  //          {
  //            std::set<uint16_t>::iterator it = m_ueSrsConfigurationIndexSet.find (srcCi);
  //            if (it == m_ueSrsConfigurationIndexSet.end ())
  //              {
  //                m_lastAllocatedConfigurationIndex = srcCi;
  //                m_ueSrsConfigurationIndexSet.insert (srcCi);
  //                break;
  //              }
  //          }
  //      }
  //  }
  return m_lastAllocatedConfigurationIndex;

}


void
LteEnbRrc::RemoveSrsConfigurationIndex (uint16_t srcCi)
{
  NS_LOG_FUNCTION (this << srcCi);
  std::set<uint16_t>::iterator it = m_ueSrsConfigurationIndexSet.find (srcCi);
  NS_ASSERT_MSG (it != m_ueSrsConfigurationIndexSet.end (), "request to remove unkwown SRS CI " << srcCi);
  m_ueSrsConfigurationIndexSet.erase (it);
}

uint8_t
LteEnbRrc::GetLogicalChannelGroup (EpsBearer bearer)
{
  if (bearer.IsGbr ())
    {
      return 1;
    }
  else
    {
      return 2;
    }
}

uint8_t
LteEnbRrc::GetLogicalChannelPriority (EpsBearer bearer)
{
  return bearer.qci;
}

void
LteEnbRrc::SendSystemInformation ()
{
  // NS_LOG_FUNCTION (this);

  for (auto &it: m_componentCarrierPhyConf)
    {
      uint8_t ccId = it.first;

      LteRrcSap::SystemInformation si;
      si.haveSib2 = true;
      si.sib2.freqInfo.ulCarrierFreq = it.second->GetUlEarfcn ();
      si.sib2.freqInfo.ulBandwidth = it.second->GetUlBandwidth ();
      si.sib2.radioResourceConfigCommon.pdschConfigCommon.referenceSignalPower = m_cphySapProvider.at (ccId)->GetReferenceSignalPower ();
      si.sib2.radioResourceConfigCommon.pdschConfigCommon.pb = 0;

      LteEnbCmacSapProvider::RachConfig rc = m_cmacSapProvider.at (ccId)->GetRachConfig ();
      LteRrcSap::RachConfigCommon rachConfigCommon;
      rachConfigCommon.preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
      rachConfigCommon.raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
      rachConfigCommon.raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;
      rachConfigCommon.txFailParam.connEstFailCount = rc.connEstFailCount;
      si.sib2.radioResourceConfigCommon.rachConfigCommon = rachConfigCommon;

      m_rrcSapUser->SendSystemInformation (it.second->GetCellId (), si);
    }

  /*
   * For simplicity, we use the same periodicity for all SIBs. Note that in real
   * systems the periodicy of each SIBs could be different.
   */
  Simulator::Schedule (m_systemInformationPeriodicity, &LteEnbRrc::SendSystemInformation, this);
}

void
LteEnbRrc::SendSystemInformationNb ()
{
  // NS_LOG_FUNCTION (this);

  NbIotRrcSap::SystemInformationNb si;
  si.haveSib2 = true;
  si.sib2 = m_sib2Nb.back();

  std::map<uint8_t, Ptr<ComponentCarrierBaseStation>>::iterator cc = m_componentCarrierPhyConf.begin();
  m_rrcSapUser->SendSystemInformationNb (cc->second->GetCellId (), si);


  /*
   * For simplicity, we use the same periodicity for all SIBs. Note that in real
   * systems the periodicy of each SIBs could be different.
   */
  Simulator::Schedule (m_systemInformationPeriodicity, &LteEnbRrc::SendSystemInformationNb, this);
}

//bool LteEnbRrc::IsSystemInformationBlock2NbAvailable(){
//  if (m_sib2Nb.size()>0){
//    return true;
//  }
//    return false;
//}
NbIotRrcSap::SystemInformationBlockType2Nb LteEnbRrc::DoGetCurrentSystemInformationBlockType2Nb(){
  // Only NB-IoT code calls this method so we assume there is only one CC
  if(m_sib2Nb.size() == 0){
    std::map<uint8_t, Ptr<ComponentCarrierBaseStation>>::iterator cc = m_componentCarrierPhyConf.begin();
    GenerateSystemInformationBlockType2Nb(*cc);
  }
  return m_sib2Nb.back();

}

bool
LteEnbRrc::IsRandomAccessCompleted (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  Ptr<UeManager> ueManager = GetUeManagerbyRnti (rnti);
  switch (ueManager->GetState ())
    {
    case UeManager::CONNECTED_NORMALLY:
    case UeManager::CONNECTION_RECONFIGURATION:
      return true;
      break;
    default:
      return false;
      break;

    }
}

uint64_t LteEnbRrc::AttachSuspendedUeNb(uint32_t imsi)
{
  uint16_t rnti = AddUe(UeManager::INITIAL_RANDOM_ACCESS,0);
  NS_LOG_DEBUG("AttachSuspendedUeNb - RNTI:" << rnti << " IMSI:" << imsi);
  return GetUeManagerbyRnti(rnti)->AttachSuspendedNb(imsi);
}

NbIotRrcSap::SystemInformationBlockType1Nb LteEnbRrc::GetSib1Nb(){
  return m_sib1Nb.back();
}
NbIotRrcSap::SystemInformationNb LteEnbRrc::GetSiNb(){
  NbIotRrcSap::SystemInformationNb si;
  si.haveSib2 = true;
  if(m_sib2Nb.size() == 0){
    std::map<uint8_t, Ptr<ComponentCarrierBaseStation>>::iterator cc = m_componentCarrierPhyConf.begin();
    GenerateSystemInformationBlockType2Nb(*cc);
  }
  si.sib2 = m_sib2Nb.back();
  return si;
}

void LteEnbRrc::GenerateSystemInformationBlockType1Nb(){

}

void LteEnbRrc::GenerateSystemInformationBlockType2Nb(std::pair<const uint8_t, ns3::Ptr<ns3::ComponentCarrierBaseStation>> cc){
  // Sib2 and SibNb are created based on the ComponentCarrier configuration. At this point only one CC ist implemented for NB-Iot
  NbIotRrcSap::SystemInformationBlockType2Nb sib2;
  sib2.freqInfo.ulCarrierFreq = cc.second->GetUlEarfcn ();
  sib2.radioResourceConfigCommon.npdschConfigCommon.nrsPower = m_cphySapProvider.at (cc.first)->GetReferenceSignalPower ();

  //LteEnbCmacSapProvider::RachConfigNb rc = m_cmacSapProvider.at (ccId)->GetRachConfigNb ();
  NbIotRrcSap::RachInfo rachce0;
  rachce0.RaResponseWindowSize  = NbIotRrcSap::RachInfo::RaResponseWindowSize::pp10;
  rachce0.macContentionResolutionTimer =  NbIotRrcSap::RachInfo::MacContentionResolutionTimer::pp32;

  NbIotRrcSap::RachInfo rachce1;
  rachce1.RaResponseWindowSize  = NbIotRrcSap::RachInfo::RaResponseWindowSize::pp8;
  rachce1.macContentionResolutionTimer =  NbIotRrcSap::RachInfo::MacContentionResolutionTimer::pp32;

  NbIotRrcSap::RachInfo rachce2;
  rachce2.RaResponseWindowSize  = NbIotRrcSap::RachInfo::RaResponseWindowSize::pp8;
  rachce2.macContentionResolutionTimer =  NbIotRrcSap::RachInfo::MacContentionResolutionTimer::pp32;

  NbIotRrcSap::RachConfigCommon rc;
  rc.preambleTransMaxCE = 10;
  rc.powerRampingParameters.powerRampingStep = NbIotRrcSap::PowerRampingParameters::PowerRampingStep::dB4;
  rc.powerRampingParameters.preambleInitialReceivedTargetPower = NbIotRrcSap::PowerRampingParameters::PreambleInitialReceivedTargetPower::dbm_110;
  rc.connEstFailOffset = 0;
  rc.rachInfoList.rachInfo1 = rachce0;
  rc.rachInfoList.rachInfo2 = rachce1;
  rc.rachInfoList.rachInfo3 = rachce2;

  sib2.radioResourceConfigCommon.rachConfigCommon = rc;
  NbIotRrcSap::RsrpThresholdsPrachInfoList rsrpprachinfolist;
  // From Vodafone wireshark
  rsrpprachinfolist.ce1_lowerbound = -115.5;
  rsrpprachinfolist.ce2_lowerbound = -127.5;

  sib2.radioResourceConfigCommon.nprachConfig.rsrpThresholdsPrachInfoList = rsrpprachinfolist;
  // Values from Vodafone Cell / temporary
  NbIotRrcSap::NprachParametersNb ce0;
  ce0.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::zero;
  // CE0 NPRACH periodicity set to 80 ms (TS 36.331 enum: ms40..ms2560). This is the
  // SR/RA occasion cadence for the good-coverage class; it cuts the round-robin SR
  // wait 4x vs the 320 ms default at ~8% NPRACH airtime (6.4 ms preamble / 80 ms).
  // Sensitivity floor: ms40 halves the SR wait again but doubles airtime to ~16% and
  // is CE0-only (CE1/CE2 preambles, 51.2/204.8 ms, do not fit a 40 ms period).
  // Note: nprachStartTime must be < periodicity, hence ms64 (was ms256 at 320 ms).
  ce0.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms80;
  ce0.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms64;
  ce0.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n36;
  ce0.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce0.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce0.maxNumPreambleAttemptCE = NbIotRrcSap::NprachParametersNb::MaxNumPreambleAttemptCE::n10;
  ce0.numRepetitionsPerPreambleAttempt = NbIotRrcSap::NprachParametersNb::NumRepetitionsPerPreambleAttempt::n1;
  ce0.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r8;
  ce0.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v2;
  ce0.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;

  NbIotRrcSap::NprachParametersNb ce1;
  ce1.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::one;
  ce1.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms160;
  ce1.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms256;
  ce1.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n24;
  ce1.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce1.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce1.maxNumPreambleAttemptCE = NbIotRrcSap::NprachParametersNb::MaxNumPreambleAttemptCE::n10;
  ce1.numRepetitionsPerPreambleAttempt = NbIotRrcSap::NprachParametersNb::NumRepetitionsPerPreambleAttempt::n8;
  ce1.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r64;
  ce1.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v1dot5;
  ce1.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;

  NbIotRrcSap::NprachParametersNb ce2;
  ce2.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::two;
  ce2.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms640;
  ce2.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms256;
  ce2.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n12;
  ce2.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce2.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce2.maxNumPreambleAttemptCE = NbIotRrcSap::NprachParametersNb::MaxNumPreambleAttemptCE::n10;
  ce2.numRepetitionsPerPreambleAttempt = NbIotRrcSap::NprachParametersNb::NumRepetitionsPerPreambleAttempt::n32;
  ce2.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r512;
  ce2.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v4;
  ce2.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;


  NbIotRrcSap::NprachParametersNbR14 ce0v14;
  ce0v14.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::zero;
  ce0v14.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms80; // match ce0 (80 ms; floor ms40)
  ce0v14.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms64;     // must be < periodicity
  // Dedicated EDT preamble partition (3GPP Rel-15, TS 36.321: edt-PRACH-ParametersCE as a
  // SEPARATE resource -> distinct nprach-SubcarrierOffset). Must NOT overlap the legacy
  // partition: the eNB distinguishes preambles only by subcarrier offset and runs the
  // legacy check first, so a shared offset means the legacy check consumes the EDT preamble
  // -> isEdt=false -> no edt-TBS grant -> EDT never engages. Legacy occupies n12/n24/n36
  // (CE2/CE1/CE0); n0 is the only free group, so EDT-CE0 goes there. CE0-only deployment.
  ce0v14.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n0;
  ce0v14.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce0v14.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce0v14.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r8;
  ce0v14.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v2;
  ce0v14.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;

  NbIotRrcSap::NprachParametersNbR14 ce1v14;
  ce1v14.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::one;
  ce1v14.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms640;
  ce1v14.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms512;
  ce1v14.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n24;
  ce1v14.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce1v14.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce1v14.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r64;
  ce1v14.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v1dot5;
  ce1v14.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;

  NbIotRrcSap::NprachParametersNbR14 ce2v14;
  ce2v14.coverageEnhancementLevel = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::two;
  ce2v14.nprachPeriodicity = NbIotRrcSap::NprachParametersNb::NprachPeriodicity::ms2560;
  ce2v14.nprachStartTime = NbIotRrcSap::NprachParametersNb::NprachStartTime::ms1024;
  ce2v14.nprachSubcarrierOffset = NbIotRrcSap::NprachParametersNb::NprachSubcarrierOffset::n12;
  ce2v14.nprachNumSubcarriers = NbIotRrcSap::NprachParametersNb::NprachNumSubcarriers::n12;
  ce2v14.nprachSubcarrierMsg3RangeStart = NbIotRrcSap::NprachParametersNb::NprachSubcarrierMsg3RangeStart::twoThird;
  ce2v14.npdcchNumRepetitionsRA = NbIotRrcSap::NprachParametersNb::NpdcchNumRepetitionsRA::r512;
  ce2v14.npdcchStartSfCssRa = NbIotRrcSap::NprachParametersNb::NpdcchStartSfCssRa::v4;
  ce2v14.npdcchOffsetRa= NbIotRrcSap::NprachParametersNb::NpdcchOffsetRa::zero;


  sib2.radioResourceConfigCommon.nprachConfig.nprachParametersList.nprachParametersNb0 = ce0;
  sib2.radioResourceConfigCommon.nprachConfig.nprachParametersList.nprachParametersNb1 = ce1;
  sib2.radioResourceConfigCommon.nprachConfig.nprachParametersList.nprachParametersNb2 = ce2;
  sib2.radioResourceConfigCommon.nprachConfigR15.nprachParameterListEdt.nprachParametersNb0 = ce0v14;
  sib2.radioResourceConfigCommon.nprachConfigR15.nprachParameterListEdt.nprachParametersNb1 = ce1v14;
  sib2.radioResourceConfigCommon.nprachConfigR15.nprachParameterListEdt.nprachParametersNb2 = ce2v14;
  sib2.radioResourceConfigCommon.nprachConfigR15.edtTbsInfoList.edtTbsNb0.edtTbs = NbIotRrcSap::EdtTbsNb::EdtTbs::b1000;
  sib2.radioResourceConfigCommon.nprachConfigR15.edtTbsInfoList.edtTbsNb1.edtTbs = NbIotRrcSap::EdtTbsNb::EdtTbs::b1000;
  sib2.radioResourceConfigCommon.nprachConfigR15.edtTbsInfoList.edtTbsNb2.edtTbs = NbIotRrcSap::EdtTbsNb::EdtTbs::b1000;
  sib2.freqInfo.ulCarrierFreq = m_ulEarfcn;

  if(m_sib2Nb.size() == 0){
    m_sib2Nb.push_back(sib2);
  }else{
    m_sib2Nb.back()= sib2;
  }
}

void LteEnbRrc::SetLogDir(std::string logdir){
  m_logdir = logdir;
  m_logging = !logdir.empty();
  m_cmacSapProvider.at(0)->SetLogDir(m_logdir);
  m_rrcSapUser->SetLogDir(logdir);
}

void LteEnbRrc::LogDataReception(uint32_t imsi, uint32_t size){
  if (!m_logging) return;
  std::string logfile_path = m_logdir+"DataRecep.log";
  std::ofstream logfile;
  logfile.open(logfile_path, std::ios_base::app);
  // imsi, size, time
  logfile <<  imsi <<  "," << size << "," << Simulator::Now().GetMilliSeconds() << "\n";
  logfile.close();
}
} // namespace ns3

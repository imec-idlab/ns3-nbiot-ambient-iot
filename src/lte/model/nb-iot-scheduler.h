/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Tim Gebauer <tim.gebauer@tu-dortmund.de>
 */


#ifndef NBIOT_MAC_SCHEDULER_H
#define NBIOT_MAC_SCHEDULER_H

#include <ns3/object.h>
#include "nb-iot-rrc-sap.h"
#include "lte-mac-sap.h"
#include <algorithm>
#include <unordered_map>
#include "nb-iot-amc.h"
#include <tuple>
#include <string> 

namespace ns3 {

struct SearchSpaceConfig{
  uint32_t R_max;
  double startSf;
  double offset; 
  NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel ce;
  friend bool operator==(const SearchSpaceConfig& lhs, const SearchSpaceConfig& rhs)
  {
      return lhs.R_max == rhs.R_max&&
           lhs.startSf == rhs.startSf &&
           lhs.offset == rhs.offset &&
           lhs.ce == rhs.ce;
  }
  bool operator<(const SearchSpaceConfig& rhs) const
    {
        // compares n to rhs.n,
        // then s to rhs.s,
        // then d to rhs.d
        return std::tie(R_max, startSf, offset, ce) < std::tie(rhs.R_max, rhs.startSf, rhs.offset, rhs.ce);
    }
};

struct UeConfig{
  enum class SchedulePriority{
    DOWNLINK,
    UPLINK
  } priority;
  enum class LastUplinkType{
    HARQ, // only 3 ms between sending and NPDCCH  Liberg, p286
    UPLINK //
  };
  uint16_t rnti;
  SearchSpaceConfig searchSpaceConfig;
  uint64_t rlcDlBuffer;
  uint64_t rlcUlBuffer;
  uint64_t lastUl;
  uint64_t lastUlStart = 0; ///< first subframe of the pending NPUSCH allocation (half-duplex DL guard)
  uint64_t lastDl;
  // Proactive Fast Uplink Grant (standalone 4th mode, distinct from the reactive
  // SR-based FUG): the eNB predicts this UE's traffic period and pushes a grant
  // at the predicted time with NO scheduling request. These fields are used ONLY
  // when proactive==true; all other modes leave them at their defaults.
  bool     proactive    = false;  ///< this UE is served by proactive FUG
  uint64_t predPeriodSf = 0;      ///< EWMA-estimated inter-arrival period [subframes]; 0 = not yet learned
  uint64_t lastArrivalSf = 0;     ///< absolute subframe of the last observed UL arrival
  uint64_t nextGrantSf  = 0;      ///< absolute subframe of the next proactive grant
  uint32_t arrivalCount = 0;      ///< observed UL arrivals (>=2 needed to estimate a period)
  uint64_t proactiveGrantsIssued = 0; ///< proactive DCI N0 grants pushed (denominator for FP accounting)
  // Retry cadence: a prediction is only approximate, so the first push at the
  // predicted occasion often lands a few ms before the packet actually reaches
  // the (awake, monitoring) UE's buffer -> wasted. Rather than wait a full
  // period for the next push, keep re-pushing at a short cadence for a bounded
  // window so the awake UE catches a grant soon after its data arrives. A real
  // UL arrival (NotifyUlArrival) confirms the grant was caught and re-anchors
  // the next push one full period ahead.
  uint32_t pushRetriesLeft = 0;   ///< remaining short-cadence retries for the current occasion
};





class NbiotScheduler : public Object
{
public:

  NbiotScheduler(std::vector<NbIotRrcSap::NprachParametersNb> ces, NbIotRrcSap::SystemInformationBlockType2Nb sib2);

  //~NbiotScheduler();

  virtual void DoDispose (void);


bool IsSearchSpaceBegin(SearchSpaceConfig ssc);
static SearchSpaceConfig ConvertNpdcchConfigDedicatedNb2SearchSpaceConfig(NbIotRrcSap::NpdcchConfigDedicatedNb configDedicated);
static SearchSpaceConfig ConvertNprachParametersNb2SearchSpaceConfig(NbIotRrcSap::NprachParametersNb ce);
//bool IsSeachSpaceType1Begin(NbIotRrcSap::NprachParametersNb ce);
//bool IsSeachSpaceUeSpecificBegin(NbIotRrcSap::NprachParametersNb ce);
void SetUssSearchSpaces(NbIotRrcSap::NpdcchConfigDedicatedNb uss0, NbIotRrcSap::NpdcchConfigDedicatedNb uss1, NbIotRrcSap::NpdcchConfigDedicatedNb uss2);
void SetCeLevel(NbIotRrcSap::NprachParametersNb ce0, NbIotRrcSap::NprachParametersNb ce1, NbIotRrcSap::NprachParametersNb ce2);

void ScheduleRarReq(NbIotRrcSap::NpdcchMessage, SearchSpaceConfig ssc);
bool ScheduleNpdcchMessage(NbIotRrcSap::NpdcchMessage &message, SearchSpaceConfig ssc);
           
void SetRntiRsrpMap(std::map<uint16_t, double> map);
void ScheduleUlRlcBufferReq(uint64_t rnti, uint64_t dataSize); // Data in Byte
void ScheduleDlRlcBufferReq(uint64_t rnti, std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> lcids); // Data in Byte
void AddToUlBufferReq(uint64_t rnti, uint64_t dataSize);

void SortBasedOnSelectedSchedulingAlgorithm(SearchSpaceConfig ssc);
std::vector<uint64_t> GetNextAvailableSearchSpaceCandidate(uint32_t rnti, uint64_t SearchSpaceStartFrame, uint64_t SearchSpaceStartSubframe, uint64_t R_max, uint64_t R);
std::vector<uint64_t> GetDlSubframeRangeWithoutSystemResources(uint64_t overallSubframeNo, uint64_t numSubframes);
std::vector<uint64_t> GetUlSubframeRangeWithoutSystemResources(uint64_t overallSubframeNo, uint64_t numSubframes, uint64_t carrier);
std::vector<uint64_t> CheckforNContiniousSubframesDl(std::vector<uint64_t> Subframes, uint64_t StartSubframe, uint64_t N);
std::vector<uint64_t> CheckforNContiniousSubframesUl(std::vector<uint64_t> Subframes, uint64_t StartSubframe, uint64_t N, uint64_t carrier);
std::vector<uint64_t> GetNextAvailableNpdschCandidate(uint64_t endSubframeDci, uint64_t minSchedulingDelay, uint64_t numSubframes, uint64_t R_max);
std::vector<NbIotRrcSap::NpdcchMessage> Schedule(uint64_t frameNo, uint64_t subframeNo);
std::vector<NbIotRrcSap::NpdcchMessage> ScheduleSearchSpace(SearchSpaceConfig ssc);
std::vector<std::pair<uint64_t, std::vector<uint64_t>>> GetNextAvailableNpuschCandidate(uint64_t endSubframeNpdsch, uint64_t minSchedulingDelay, uint64_t numSubframes, bool isHarq);
std::pair<NbIotRrcSap::UlGrant, std::pair<uint64_t,std::vector<uint64_t>>> GetNextAvailableMsg3UlGrantCandidate(uint64_t endSubframeMsg2, uint64_t numSubframes);
NbIotRrcSap::NpdcchMessage CreateDciNpdcchMessage(uint16_t rnti, NbIotRrcSap::NpdcchMessage::DciType dci_type);

void SetLogDir(std::string logdir);
void RoundRobinScheduling(SearchSpaceConfig ssc);
std::vector<int> m_downlink;
void RemoveUe(uint16_t rnti);
  void ParkUe(uint16_t rnti);
  // Proactive FUG (4th mode). SetProactiveMode: put the whole cell in proactive
  // FUG -- every UE added thereafter is predicted and granted without an SR.
  // NotifyUlArrival: feed the predictor an observed UL arrival (called by the
  // eNB MAC on UL data reception). GetProactiveGrantsIssued: total proactive
  // grants pushed (FP denominator).
  void SetProactiveMode (bool enable);
  // Proactive FUG round-robin arm: instead of predicting per-UE periods, poll every
  // registered UE in turn (no prediction, no SR). Bounded delay (one RR cycle), no
  // 300 s prediction-miss tail, at the cost of grants pushed to UEs with no data.
  void SetProactiveRoundRobin (bool enable);
  void NotifyUlArrival (uint16_t rnti, uint64_t nowSf);
  uint64_t GetProactiveGrantsIssued () const;
protected:
  std::vector<std::vector<int>> m_uplink;
  std::vector<NbIotRrcSap::NpdcchMessage> m_rars_to_schedule;
  //std::vector<NbIotRrcSap::NpdcchMessage> m_NpdcchQueue;
  std::map<SearchSpaceConfig, std::vector<NbIotRrcSap::NpdcchMessage>> m_NpdcchQueue;
  std::vector<NbIotRrcSap::DciN1::NpdcchTimeOffset> m_DciTimeOffsetRmaxSmall;
  std::vector<NbIotRrcSap::DciN1::NpdcchTimeOffset> m_DciTimeOffsetRmaxBig;
  std::vector<NbIotRrcSap::UlGrant::SchedulingDelay> m_Msg3TimeOffset;
  std::vector<NbIotRrcSap::DciN0::NpuschSchedulingDelay> m_DciTimeOffsetUplink;
  std::vector<NbIotRrcSap::HarqAckResource::TimeOffset> m_HarqTimeOffsets;
  std::vector<NbIotRrcSap::HarqAckResource::SubcarrierIndex> m_HarqSubcarrierIndex;
  
  NbIotRrcSap::NprachParametersNb m_ce0;
  NbIotRrcSap::NprachParametersNb m_ce1;
  NbIotRrcSap::NprachParametersNb m_ce2;

  NbIotRrcSap::NpdcchConfigDedicatedNb m_uss0;
  NbIotRrcSap::NpdcchConfigDedicatedNb m_uss1;
  NbIotRrcSap::NpdcchConfigDedicatedNb m_uss2;

  std::map<SearchSpaceConfig, std::vector<uint16_t>> m_searchSpaceRntiMap;
  std::map<uint16_t, UeConfig> m_rntiUeConfigMap;
  bool m_proactiveMode = false;          ///< cell-wide proactive FUG (4th mode)
  bool m_proactiveRoundRobin = false;    ///< fug RR arm: poll UEs in turn instead of predicting (only used when m_proactiveMode)
  uint16_t m_rrCursor = 0;               ///< RR: last RNTI granted (resume point; advance once it has been served)
  uint64_t m_proactiveGrantsIssued = 0;  ///< running total of proactive grants pushed
  // Proactive-push retry window: total grants attempted per predicted occasion
  // (1 initial + retries) and the short re-push spacing (subframes == ms). The
  // window spans kProactivePushRetries * kProactivePushRetrySf ms around the
  // prediction; sized to comfortably cover the EWMA prediction error so the
  // awake UE catches a grant within a few seconds of its data arriving, while
  // staying far below the traffic period so it cannot bleed into the next epoch.
  static constexpr uint32_t kProactivePushRetries  = 3;    ///< pushes per occasion (1 + 2 retries)
  static constexpr uint64_t kProactivePushRetrySf  = 400;  ///< retry spacing [subframes/ms] (0.4 s)
  static constexpr uint64_t kProactivePhaseLeadSf  = 400;  ///< phase-lock: bias the first push this far BEFORE the predicted arrival (1 retry spacing) so the data is caught on an early retry and the catch latency stays a small constant (no drift)
  static constexpr uint64_t kProactiveColdStartFloorSf = 60000; ///< sub-60 s "arrivals" before a period is known are cold-start artifacts, not new epochs
  std::map<SearchSpaceConfig, std::vector<NbIotRrcSap::NpdcchMessage>> m_rarQueue;
  bool m_only15KhzSpacing = true;
  uint64_t m_frameNo;
  uint64_t m_subframeNo;
  int m_currenthyperindex;
  const uint64_t m_minSchedulingDelayDci2Downlink = 4;
  const uint64_t m_minSchedulingDelayDci2Uplink = 8;
  bool m_log;
  std::map<uint16_t,double> m_rntiRsrpMap;
  NbiotAmc m_Amc;
  NbIotRrcSap::SystemInformationBlockType2Nb m_sib2config;
  std::map<SearchSpaceConfig, uint16_t> m_RoundRobinLastScheduled;

  std::string m_logdir;
  bool m_logging {false};
//std::vector<std::pair<uint64_t,uint64_t>> GetAllPossibleSearchSpaceCandidates(std::vector<uint64_t> subframes, uint64_t R_max);
  void LogUplinkGrid();
  void LogDownlinkGrid();
};

}  // namespace ns3

#endif /* FF_MAC_SCHEDULER_H */


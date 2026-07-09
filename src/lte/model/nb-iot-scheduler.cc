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
 * Modified by:
 *      Pascal Jörke <pascal.joerke@tu-dortmund.de>
 */


#include "nb-iot-scheduler.h"
#include "lte-common.h"
#include <istream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <ns3/build-profile.h>
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NbiotScheduler");

NS_OBJECT_ENSURE_REGISTERED (NbiotScheduler);

NbiotScheduler::NbiotScheduler (std::vector<NbIotRrcSap::NprachParametersNb> ces,
                                NbIotRrcSap::SystemInformationBlockType2Nb sib2)
{
  m_Amc = NbiotAmc ();
  m_ce0 = ces[0];
  m_ce1 = ces[1];
  m_ce2 = ces[2];
  m_sib2config = sib2;
  m_DciTimeOffsetRmaxSmall.reserve (8);
  m_DciTimeOffsetRmaxBig.reserve (8);
  m_Msg3TimeOffset.reserve (4);
  m_logdir = "";

  m_DciTimeOffsetRmaxSmall.insert (
      m_DciTimeOffsetRmaxSmall.begin (),
      {NbIotRrcSap::DciN1::NpdcchTimeOffset::ms0, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms4,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms8, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms12,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms16, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms32,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms64, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms128});
  m_DciTimeOffsetRmaxBig.insert (
      m_DciTimeOffsetRmaxBig.begin (),
      {NbIotRrcSap::DciN1::NpdcchTimeOffset::ms0, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms16,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms32, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms64,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms128, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms256,
       NbIotRrcSap::DciN1::NpdcchTimeOffset::ms512, NbIotRrcSap::DciN1::NpdcchTimeOffset::ms1024});
  m_Msg3TimeOffset.insert (
      m_Msg3TimeOffset.begin (),
      {NbIotRrcSap::UlGrant::SchedulingDelay::ms12, NbIotRrcSap::UlGrant::SchedulingDelay::ms16,
       NbIotRrcSap::UlGrant::SchedulingDelay::ms32, NbIotRrcSap::UlGrant::SchedulingDelay::ms64});
  m_DciTimeOffsetUplink.insert (m_DciTimeOffsetUplink.begin (),
                                {NbIotRrcSap::DciN0::NpuschSchedulingDelay::ms8,
                                 NbIotRrcSap::DciN0::NpuschSchedulingDelay::ms16,
                                 NbIotRrcSap::DciN0::NpuschSchedulingDelay::ms32,
                                 NbIotRrcSap::DciN0::NpuschSchedulingDelay::ms64});

  m_HarqTimeOffsets.insert (m_HarqTimeOffsets.begin (),
                            {NbIotRrcSap::HarqAckResource::TimeOffset::thirteen,
                             NbIotRrcSap::HarqAckResource::TimeOffset::fifteen,
                             NbIotRrcSap::HarqAckResource::TimeOffset::seventeen,
                             NbIotRrcSap::HarqAckResource::TimeOffset::eighteen});
  m_HarqSubcarrierIndex.insert (m_HarqSubcarrierIndex.begin (),
                                {NbIotRrcSap::HarqAckResource::SubcarrierIndex::zero,
                                 NbIotRrcSap::HarqAckResource::SubcarrierIndex::one,
                                 NbIotRrcSap::HarqAckResource::SubcarrierIndex::two,
                                 NbIotRrcSap::HarqAckResource::SubcarrierIndex::three});

  uint64_t numHyperframes = 1024;
  uint64_t numFrames = 1024;
  uint64_t numSubframes = 10;
  // SIB1 Scheduling
  NbIotRrcSap::MasterInformationBlockNb m_mibNb;
  m_mibNb.schedulingInfoSib1 = 2;
  bool sib1NbPeriod = false;
  uint16_t sib1NbRepetitions = 0;
  m_downlink.resize (numHyperframes * numFrames * numSubframes, 0);
  NbIotRrcSap::SystemInformationBlockType1Nb sib1;
  NbIotRrcSap::SchedulingInfoNb info;
  info.sibMappingInfo.push_back (2);
  info.siPeriodicity = NbIotRrcSap::SchedulingInfoNb::SiPeriodicityNb::rf512;
  info.siRepetitionPattern = NbIotRrcSap::SchedulingInfoNb::SiRepetitionPatternNb::every4thRF;
  info.siTb = NbIotRrcSap::SchedulingInfoNb::SiTbNb::b440;
  sib1.schedulingInfoList.push_back (info);
  sib1.siWindowLength = NbIotRrcSap::SystemInformationBlockType1Nb::SiWindowLengthNb::ms160;
  sib1.siRadioFrameOffset = 0;
  bool si = false;
  uint8_t siRepetitions = 0;
  uint8_t siRepetitionPattern = 0;
  uint8_t tmpSiRepetitions = 0;
  uint16_t siWindow = 0;
  for (size_t i = 0; i < m_downlink.size (); ++i)
    {
      if ((i % 10) == 0)
        {
          m_downlink[i] = -1; // MIB-NB
        }
      if ((i % 10) == 5)
        {
          m_downlink[i] = -2; // NPSS
        }
      if (((i % 10) == 9) && ((i / 10) % 2 == 1))
        {
          m_downlink[i] = -3; // NSSS
        }
      if (((i / 10) % 256) == 0)
        {
          sib1NbPeriod = true;
          switch (m_mibNb.schedulingInfoSib1)
            {
            case 0:
            case 3:
            case 6:
            case 9:
              sib1NbRepetitions = 4;
              break;
            case 1:
            case 4:
            case 7:
            case 10:
              sib1NbRepetitions = 8;
              break;
            default:
              sib1NbRepetitions = 16;
              break;
            }
        }
      if (sib1NbPeriod && (i / 160) % 2 == 0)
        {

          if (((i % 10) == 4) && ((i / 10) % 2 == 0))
            {
              m_downlink[i] = -4; // SIB1-NB
            }
        }
      else if (sib1NbPeriod && (i / 10) % 16 == 0 && i % 10 == 0)
        {
          sib1NbRepetitions--;
        }
      if (sib1NbRepetitions == 0)
        {
          sib1NbPeriod = false;
        }
      // SI Scheduling

      for (size_t j = 0; j < sib1.schedulingInfoList.size (); j++)
        {

          uint16_t lhs =
              i / 10 %
              NbIotRrcSap::ConvertSchedulingInfoPeriodicity2int (sib1.schedulingInfoList[j]);
          uint16_t x = (j) *NbIotRrcSap::ConvertSiWindowLength2int (sib1) + sib1.siRadioFrameOffset;
          uint16_t rhs = x / 10 + sib1.siRadioFrameOffset;
          if (lhs == rhs && !si)
            {
              si = true;
              if (NbIotRrcSap::ConvertSchedulingInfoTb2int (sib1.schedulingInfoList[j]) > 120)
                {
                  siRepetitions = 8;
                }
              else
                {
                  siRepetitions = 2;
                }
              siWindow = NbIotRrcSap::ConvertSiWindowLength2int (sib1);
              siRepetitionPattern = NbIotRrcSap::ConvertSchedulingInfoRepetitionPattern2int (
                  sib1.schedulingInfoList[j]);
              // Begin of SI Message
            }
        }
      if (si)
        {
          if (((i / 10) - sib1.siRadioFrameOffset) % siRepetitionPattern == 0 && i % 10 == 0)
            {
              tmpSiRepetitions = siRepetitions;
            }
          if (tmpSiRepetitions > 0)
            {
              if (m_downlink[i] == 0)
                {
                  m_downlink[i] = -5;
                  tmpSiRepetitions--;
                }
            }
        }
      if (siWindow > 0)
        {
          siWindow--;
        }
      else
        {
          si = false;
        }
    }

  if (m_only15KhzSpacing)
    {
      m_uplink.resize (12, std::vector<int> ());
      for (size_t i = 0; i < m_uplink.size (); ++i)
        {
          m_uplink[i].resize (numHyperframes * numFrames * numSubframes, 0);
        }
      for (std::vector<NbIotRrcSap::NprachParametersNb>::iterator it = ces.begin ();
           it != ces.end (); ++it)
        {
          uint64_t sendingTime = NbIotRrcSap::ConvertNprachStartTime2int (*it);
          double ts = 1000.0 / (15000.0 * 2048.0);
          double preambleSymbolTime = 8192.0 * ts;
          double preambleGroupTimeNoCP = 5.0 * preambleSymbolTime;
          double preambleGroupTime = 0.266 + preambleGroupTimeNoCP;
          double preambleRepetition = 4.0 * preambleGroupTime;
          double nprachduration =
              (NbIotRrcSap::ConvertNumRepetitionsPerPreambleAttempt2int (*it) * preambleRepetition);
          size_t subcarrierOffset = NbIotRrcSap::ConvertNprachSubcarrierOffset2int (*it);
          uint8_t numberSubcarriers = NbIotRrcSap::ConvertNprachNumSubcarriers2int (*it);
          double time_tmp = uint64_t (nprachduration) + 1;

          for (size_t i = 0; i < m_uplink[0].size (); ++i)
            {

              uint16_t window_condition =
                  (i / 10) % (NbIotRrcSap::ConvertNprachPeriodicity2int (*it) / 10);
              if (window_condition == 0)
                {
                  i += sendingTime;
                  for (size_t j = 0; j < time_tmp; ++j)
                    {
                      for (size_t k = 0; k < numberSubcarriers / 4; ++k)
                        {
                          m_uplink[subcarrierOffset / 4 + k][i + j] = -1;
                        }
                    }
                  i += time_tmp;
                }
            }
        }
    }
}

void
NbiotScheduler::DoDispose ()
{
  if (m_logging)
    {
      LogUplinkGrid();
      LogDownlinkGrid ();
    }
  NS_LOG_FUNCTION (this);
}

void
NbiotScheduler::SetCeLevel (NbIotRrcSap::NprachParametersNb ce0,
                            NbIotRrcSap::NprachParametersNb ce1,
                            NbIotRrcSap::NprachParametersNb ce2)
{
  m_ce0 = ce0;
  m_ce1 = ce1;
  m_ce2 = ce2;
}
void
NbiotScheduler::SetUssSearchSpaces (NbIotRrcSap::NpdcchConfigDedicatedNb uss0,
                                    NbIotRrcSap::NpdcchConfigDedicatedNb uss1,
                                    NbIotRrcSap::NpdcchConfigDedicatedNb uss2)
{
  m_uss0 = uss0;
  m_uss1 = uss1;
  m_uss2 = uss2;
}
bool
NbiotScheduler::IsSearchSpaceBegin (SearchSpaceConfig ssc)
{
  uint32_t searchSpacePeriodicity = ssc.R_max * ssc.startSf;
  uint32_t searchSpaceConditionLeftSide =
      (10 * (m_frameNo - 1) + (m_subframeNo - 1)) % searchSpacePeriodicity;
  uint32_t searchSpaceConditionRightSide = ssc.offset * searchSpacePeriodicity;
  if (searchSpaceConditionLeftSide == searchSpaceConditionRightSide)
    {
      return true;
    }
  return false;
}

void
NbiotScheduler::SetRntiRsrpMap (std::map<uint16_t, double> map)
{
  m_rntiRsrpMap = map;
}

void
NbiotScheduler::ScheduleRarReq (NbIotRrcSap::NpdcchMessage msg, SearchSpaceConfig ssc)
{
  // NPDCCH Parameters taken from Liberg, Olof, et al. The Cellular Internet of Things 2017 p.305, In-Band-Deployment Table 8.9

  if (msg.ce == m_ce0.coverageEnhancementLevel)
    {
      msg.dciN0.dciRepetitions = NbIotRrcSap::DciN0::DciRepetitions::r2;
      msg.dciN1.dciRepetitions = NbIotRrcSap::DciN1::DciRepetitions::r2;
    }
  else if (msg.ce == m_ce1.coverageEnhancementLevel)
    {
      msg.dciN0.dciRepetitions = NbIotRrcSap::DciN0::DciRepetitions::r32;
      msg.dciN1.dciRepetitions = NbIotRrcSap::DciN1::DciRepetitions::r32;
    }
  else if (msg.ce == m_ce2.coverageEnhancementLevel)
    {
      msg.dciN0.dciRepetitions = NbIotRrcSap::DciN0::DciRepetitions::r256;
      msg.dciN1.dciRepetitions = NbIotRrcSap::DciN1::DciRepetitions::r256;
    }

  for (std::vector<NbIotRrcSap::Rar>::iterator it = msg.rars.begin (); it != msg.rars.end (); ++it)
    {
      m_rntiUeConfigMap[it->cellRnti] = UeConfig ();

      m_rntiUeConfigMap[it->cellRnti].lastDl = 0;
      m_rntiUeConfigMap[it->cellRnti].lastUl = 0;
      m_rntiUeConfigMap[it->cellRnti].lastUlStart = 0;
      m_rntiUeConfigMap[it->cellRnti].rlcDlBuffer = 0;
      m_rntiUeConfigMap[it->cellRnti].rlcUlBuffer = 0;
      m_rntiUeConfigMap[it->cellRnti].rnti = it->cellRnti;
      m_rntiUeConfigMap[it->cellRnti].searchSpaceConfig = ssc;
      m_rntiUeConfigMap[it->cellRnti].proactive = m_proactiveMode; // 4th mode: predict+push grants, no SR
    }
  m_rarQueue[ssc].push_back (msg);
}

std::vector<NbIotRrcSap::NpdcchMessage>
NbiotScheduler::Schedule (uint64_t frameNo, uint64_t subframeNo)
{
  m_frameNo = frameNo;
  m_subframeNo = subframeNo;
  std::vector<NbIotRrcSap::NpdcchMessage> ret = std::vector<NbIotRrcSap::NpdcchMessage> ();
  std::vector<NbIotRrcSap::NpdcchMessage> tmp;
  if (frameNo == 1 && subframeNo == 1)
    {
      return ret;
    }
  // Proactive FUG (4th mode): for each UE whose period has been learned, push a
  // blind minimum grant at the predicted occasion -- NO scheduling request. The
  // grant flows through the normal rlcUlBuffer>0 gate below. If the UE has data
  // it fills the grant (+ a BSR for follow-ups); if not, the grant is wasted (a
  // false positive). RNTIs are collected first to avoid mutating the maps mid-
  // iteration.
  if (m_proactiveMode)
    {
      uint64_t nowSf = 10 * (frameNo - 1) + (subframeNo - 1);
      std::vector<uint16_t> toGrant;
      if (m_proactiveRoundRobin)
        {
          // ROUND-ROBIN arm (fug-rr): poll UEs in turn, no prediction, no SR. Paced
          // by the NPUSCH itself -- advance to the next UE only once the previously
          // polled UE has actually been GRANTED (its rlcUlBuffer drained when the DCI
          // N0 was allocated below). So the cell polls exactly as fast as the single
          // narrowband can serve grants, with at most one poll outstanding -- no magic
          // interval. Bounded delay (= one cycle), no 300 s prediction-miss tail; the
          // cost is grants spent on UEs that have no data this round.
          auto cur = m_rntiUeConfigMap.find (m_rrCursor);
          bool prevServed = (m_rrCursor == 0) || (cur == m_rntiUeConfigMap.end ())
                            || (cur->second.rlcUlBuffer == 0);
          if (prevServed)
            {
              uint16_t next = 0;
              for (auto & kv : m_rntiUeConfigMap)
                if (kv.second.proactive && kv.first > m_rrCursor) { next = kv.first; break; }
              if (next == 0) // wrap to the first proactive UE
                for (auto & kv : m_rntiUeConfigMap)
                  if (kv.second.proactive) { next = kv.first; break; }
              if (next != 0)
                {
                  toGrant.push_back (next);
                  m_rrCursor = next;
                  m_proactiveGrantsIssued++;
                  m_rntiUeConfigMap[next].proactiveGrantsIssued++;
                }
            }
        }
      else
       for (auto & kv : m_rntiUeConfigMap)
        {
          UeConfig & ue = kv.second;
          if (!ue.proactive || ue.predPeriodSf == 0)
            continue;
          if (nowSf >= ue.nextGrantSf)
            {
              toGrant.push_back (ue.rnti);
              ue.proactiveGrantsIssued++;
              m_proactiveGrantsIssued++;
              // The prediction is only approximate: the first push at a predicted
              // occasion frequently lands a few ms before the packet reaches the
              // (awake) UE's buffer -> wasted. Instead of waiting a full period
              // for the next chance, re-push at a short retry cadence for a
              // bounded window so the awake, data-pending UE catches a grant soon
              // after its data arrives. A confirmed UL arrival (NotifyUlArrival)
              // zeroes pushRetriesLeft and re-anchors nextGrantSf one period out.
              if (ue.pushRetriesLeft == 0)
                {
                  ue.pushRetriesLeft = kProactivePushRetries; // open a fresh retry window
                }
              if (ue.pushRetriesLeft > 0)
                {
                  ue.pushRetriesLeft--;
                }
              if (ue.pushRetriesLeft > 0)
                {
                  // still inside the retry window: re-push soon
                  ue.nextGrantSf = nowSf + kProactivePushRetrySf;
                }
              else
                {
                  // window exhausted (or single shot): fall back to one period out
                  do { ue.nextGrantSf += ue.predPeriodSf; } while (nowSf >= ue.nextGrantSf);
                }
            }
        }
      for (uint16_t r : toGrant)
        ScheduleUlRlcBufferReq (r, 50); // blind service-sized grant (~1 ambient packet), as the SR path
    }
  // check and Schedule DCIs for SearchSpaceType2 (RAR, HARQ, RRC)
  SearchSpaceConfig currentSearchSpace;
  if (IsSearchSpaceBegin (ConvertNprachParametersNb2SearchSpaceConfig (m_ce0)))
    {
      currentSearchSpace = ConvertNprachParametersNb2SearchSpaceConfig (m_ce0);
      tmp = ScheduleSearchSpace (currentSearchSpace);
      ret.reserve (ret.size () + std::distance (tmp.begin (), tmp.end ()));
      ret.insert (ret.end (), tmp.begin (), tmp.end ());
    }
  if (IsSearchSpaceBegin (ConvertNprachParametersNb2SearchSpaceConfig (m_ce1)))
    {
      currentSearchSpace = ConvertNprachParametersNb2SearchSpaceConfig (m_ce1);
      tmp = ScheduleSearchSpace (currentSearchSpace);
      ret.reserve (ret.size () + std::distance (tmp.begin (), tmp.end ()));
      ret.insert (ret.end (), tmp.begin (), tmp.end ());
    }
  if (IsSearchSpaceBegin (ConvertNprachParametersNb2SearchSpaceConfig (m_ce2)))
    {
      currentSearchSpace = ConvertNprachParametersNb2SearchSpaceConfig (m_ce2);
      tmp = ScheduleSearchSpace (currentSearchSpace);
      ret.reserve (ret.size () + std::distance (tmp.begin (), tmp.end ()));
      ret.insert (ret.end (), tmp.begin (), tmp.end ());
    }

  // check and Schedule DCIs for User specific searchspace
  //  if (IsSeachSpaceType2Begin (m_uss0))
  //    {
  //      tmp = ScheduleSearchSpace (NbIotRrcSap::NpdcchMessage::SearchSpaceType::type2, m_ce0);
  //      ret.reserve(ret.size() + std::distance(tmp.begin(),tmp.end()));
  //      ret.insert(ret.end(),tmp.begin(),tmp.end());
  //    }
  //  if (IsSeachSpaceType2Begin (m_ce1))
  //    {
  //      tmp = ScheduleSearchSpace (NbIotRrcSap::NpdcchMessage::SearchSpaceType::type2, m_ce1);
  //      ret.reserve(ret.size() + std::distance(tmp.begin(),tmp.end()));
  //      ret.insert(ret.end(),tmp.begin(),tmp.end());
  //    }
  //  if (IsSeachSpaceType2Begin (m_ce2))
  //    {
  //      tmp = ScheduleSearchSpace (NbIotRrcSap::NpdcchMessage::SearchSpaceType::type2, m_ce2);
  //      ret.reserve(ret.size() + std::distance(tmp.begin(),tmp.end()));
  //      ret.insert(ret.end(),tmp.begin(),tmp.end());
  //    }
  //
  return ret;
}


void
NbiotScheduler::SortBasedOnSelectedSchedulingAlgorithm (SearchSpaceConfig ssc)
{
  // When implementing new Scheduling Algorithms
  RoundRobinScheduling (ssc);
}

void
NbiotScheduler::RoundRobinScheduling (SearchSpaceConfig ssc)
{
  std::vector<uint16_t> &rntis = m_searchSpaceRntiMap[ssc];
  // Optional : Sort List by RNTI
  if (rntis.size () == 0)
    {
      // Nothing to sort
      return;
    }
  std::sort (rntis.begin (), rntis.end ());

  // Find last scheduled RNTI of this searchspace
  uint16_t start_rnti = m_RoundRobinLastScheduled[ssc];
  std::vector<uint16_t>::iterator it = std::find (rntis.begin (), rntis.end (), start_rnti);
  // Rnti not in list anymore, e.g. RrcRelease or moved to another Search Space
  uint16_t offset = 0;
  if (it != rntis.end ())
    {
      offset = it - rntis.begin () + 1;
    }
  else
    {
      while (it == rntis.end ())
        {
          start_rnti = ((start_rnti + 1) % 65535) + 1;
          it = std::find (rntis.begin (), rntis.end (), start_rnti);
        }
      offset = it - rntis.begin ();
    }
  std::rotate (rntis.begin (), rntis.begin () + offset, rntis.end ());
}

bool
NbiotScheduler::ScheduleNpdcchMessage (NbIotRrcSap::NpdcchMessage &message, SearchSpaceConfig ssc)
{

  bool scheduleSuccessful = false;

  if (message.dciType == NbIotRrcSap::NpdcchMessage::DciType::n1)
    {
      std::vector<uint64_t> test = GetNextAvailableSearchSpaceCandidate (
          message.rnti, m_frameNo - 1, m_subframeNo - 1, ssc.R_max,
          NbIotRrcSap::ConvertDciN1Repetitions2int (message.dciN1));
      if (NbIotDebugTrace () && test.empty ())
        std::cout << "[N1-FAIL] sf=" << (10 * (m_frameNo - 1) + m_subframeNo - 1) << " rnti=" << message.rnti << " step=searchspace" << std::endl;
      if (test.size () > 0) // WE GOT A DOWNLINK NPDCCH CANDIDATE
        {

          uint64_t subframesNpdsch =
              NbIotRrcSap::ConvertNumNpdschSubframesPerRepetition2int (message.dciN1) *
              NbIotRrcSap::ConvertNumNpdschRepetitions2int (message.dciN1);
          std::vector<uint64_t> npdschsubframes = GetNextAvailableNpdschCandidate (
              *(test.end () - 1), m_minSchedulingDelayDci2Downlink, subframesNpdsch, ssc.R_max);
          if (NbIotDebugTrace () && npdschsubframes.empty ())
            std::cout << "[N1-FAIL] sf=" << (10 * (m_frameNo - 1) + m_subframeNo - 1) << " rnti=" << message.rnti << " step=npdsch" << std::endl;
          if (npdschsubframes.size () > 0) // WE GOT A DOWNLINK CANDIDATE
            {
              uint64_t subframesNpusch;
              std::pair<NbIotRrcSap::UlGrant, std::pair<uint8_t, std::vector<uint64_t>>> ulgrant;
              if (message.isRar)
                {
                  for (std::vector<NbIotRrcSap::Rar>::iterator rar = message.rars.begin ();
                       rar != message.rars.end ();)
                    {
                      uint64_t size_mac_pdu = 0;
                      if (message.isEdt)
                        {
                          if (message.ce ==
                              NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::zero)
                            {
                              size_mac_pdu = NbIotRrcSap::ConvertEdtTbs2int (
                                  m_sib2config.radioResourceConfigCommon.nprachConfigR15
                                      .edtTbsInfoList.edtTbsNb0);
                            }
                          else if (message.ce ==
                                   NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::one)
                            {
                              size_mac_pdu = NbIotRrcSap::ConvertEdtTbs2int (
                                  m_sib2config.radioResourceConfigCommon.nprachConfigR15
                                      .edtTbsInfoList.edtTbsNb1);
                            }
                          else if (message.ce ==
                                   NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::two)
                            {
                              size_mac_pdu = NbIotRrcSap::ConvertEdtTbs2int (
                                  m_sib2config.radioResourceConfigCommon.nprachConfigR15
                                      .edtTbsInfoList.edtTbsNb2);
                            }
                        }
                      else
                        {
                          size_mac_pdu = 88; // Fixed Ulgrant Size. See 136.331
                        }
                      uint64_t couplingloss = 0;
                      if (rar->ceLevel ==
                          NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::two)
                        {
                          couplingloss = 159; // PASCAL: Woher diese Zahlen?
                        }
                      else if (rar->ceLevel ==
                               NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::one)
                        {
                          couplingloss = 149;
                        }
                      else if (rar->ceLevel ==
                               NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::zero)
                        {
                          couplingloss = 139;
                        }
                      subframesNpusch =
                          m_Amc.getMsg3Subframes (couplingloss, size_mac_pdu, 15000, 15);
                      ulgrant = GetNextAvailableMsg3UlGrantCandidate (npdschsubframes.back (),
                                                                      subframesNpusch);
                      if (ulgrant.first.success) // WE GOT AN UPLINK MSG3 CANDIDATE
                        {
                          scheduleSuccessful = true;
                          rar->rarPayload.ulGrant = ulgrant.first;
                          rar->rarPayload.ulGrant.subframes = ulgrant.second;
                          rar->rarPayload.ulGrant.tbs_size = size_mac_pdu;
                          //NS_BUILD_DEBUG (std::cout << "Scheduling NPUSCH at ");
                          //NS_BUILD_DEBUG (std::cout << " Subcarrier " << ulgrant.second.first << " ");
                          for (size_t i = 0; i < ulgrant.second.second.size (); i++)
                            {
                              m_uplink[ulgrant.second.first][ulgrant.second.second[i]] =
                                  message.rnti;
                                  //m_currenthyperindex;

                              //NS_BUILD_DEBUG (std::cout << ulgrant.second.second[i] << " ");
                            }
                          //NS_BUILD_DEBUG (std::cout << std::endl);
                          ++rar;
                          m_rntiUeConfigMap[rar->cellRnti].lastUl = ulgrant.second.second.back ();
                          m_rntiUeConfigMap[rar->cellRnti].lastUlStart = ulgrant.second.second.front ();
                        }
                      else
                        {
                          message.rars.erase (rar);
                          // Stuff if cant schedule npusch
                        }
                    }
                }
              else
                {
                  // Create HARQ Ressource
                  uint64_t subframesNpuschHarq =
                      16; // Have to be set by higher layer | 4 for debugging
                  std::vector<std::pair<uint64_t, std::vector<uint64_t>>> npuschharqsubframes =
                      GetNextAvailableNpuschCandidate (*(npdschsubframes.end () - 1), 0,
                                                       subframesNpuschHarq, true);
                  if (NbIotDebugTrace () && npuschharqsubframes.empty ())
                    std::cout << "[N1-FAIL] sf=" << (10 * (m_frameNo - 1) + m_subframeNo - 1) << " rnti=" << message.rnti << " step=harq" << std::endl;
                  if (npuschharqsubframes.size () > 0)
                    {
                      //NS_BUILD_DEBUG (std::cout << "Scheduling NPUSCH HARQ at ");
                      scheduleSuccessful = true;
                      for (size_t i = 0; i < npuschharqsubframes[0].second.size (); i++)
                        {
                          m_uplink[npuschharqsubframes[0].first][npuschharqsubframes[0].second[i]] =
                              message.rnti;
                              //m_currenthyperindex;
                          //NS_BUILD_DEBUG (std::cout << npuschharqsubframes[0].second[i] << " ");
                        }
                      //NS_BUILD_DEBUG (std::cout << std::endl);
                      message.dciN1.npuschOpportunity = npuschharqsubframes;

                      m_rntiUeConfigMap[message.rnti].lastUl =
                          npuschharqsubframes[0].second.back ();
                      m_rntiUeConfigMap[message.rnti].lastUlStart =
                          npuschharqsubframes[0].second.front ();
                    }
                }
              if (scheduleSuccessful)
                {
                  //NS_BUILD_DEBUG (std::cout << "Scheduling NPDCCH at ");

                  for (size_t j = 0; j < test.size (); ++j)
                    {
                      m_downlink[test[j]] = message.rnti;//m_currenthyperindex;
                      //NS_BUILD_DEBUG (std::cout << test[j] << " ");
                    }

                  //NS_BUILD_DEBUG (std::cout << std::endl);
                  //NS_BUILD_DEBUG (std::cout << "Scheduling NPDSCH at ");

                  for (size_t j = 0; j < npdschsubframes.size (); ++j)
                    {
                      m_downlink[npdschsubframes[j]] = message.rnti;//m_currenthyperindex;
                      //NS_BUILD_DEBUG (std::cout << npdschsubframes[j] << " ");
                    }

                  message.dciRepetitionsubframes = test;
                  message.dciN1.npdschOpportunity = npdschsubframes;
                  message.dciN1.dciSubframes = test;
                  //NS_BUILD_DEBUG (std::cout << std::endl);
                  return true;
                }
            }
        }
    }
  else if (message.dciType == NbIotRrcSap::NpdcchMessage::DciType::n0)
    {
      std::vector<uint64_t> test = GetNextAvailableSearchSpaceCandidate (
          message.rnti, m_frameNo - 1, m_subframeNo - 1, ssc.R_max,
          NbIotRrcSap::ConvertDciN0Repetitions2int (message.dciN0));
      if (test.size () > 0)
        {
          uint64_t size_ru = 1; // 15 khz spacing
          uint64_t subframesNpusch = NbIotRrcSap::ConvertNumResourceUnits2int (message.dciN0)*size_ru *
                                     NbIotRrcSap::ConvertNumNpuschRepetitions2int (message.dciN0);
          // Have to be set by higher layer | 4 for debugging
          std::vector<std::pair<uint64_t, std::vector<uint64_t>>> npuschsubframes =
              GetNextAvailableNpuschCandidate (*(test.end () - 1), 0, subframesNpusch, true);

          if (npuschsubframes.size () > 0)
            {
              //NS_BUILD_DEBUG (std::cout << "Scheduling NPDCCH at ");

              for (size_t j = 0; j < test.size (); ++j)
                {
                  m_downlink[test[j]] = message.rnti;//m_currenthyperindex;
                  //NS_BUILD_DEBUG (std::cout << test[j] << " ");
                }

              //NS_BUILD_DEBUG (std::cout << std::endl);
              //NS_BUILD_DEBUG (std::cout << "Scheduling NPUSCH at ");
              scheduleSuccessful = true;
              for (size_t i = 0; i < npuschsubframes[0].second.size (); i++)
                {
                  m_uplink[npuschsubframes[0].first][npuschsubframes[0].second[i]] =
                      message.rnti;
                      //m_currenthyperindex;
                  //NS_BUILD_DEBUG (std::cout << npuschsubframes[0].second[i] << " ");
                }
              //NS_BUILD_DEBUG (std::cout << std::endl);

              message.dciRepetitionsubframes = test;
              message.dciN0.npuschOpportunity = npuschsubframes;
              message.dciN0.dciSubframes = test;
              m_rntiUeConfigMap[message.rnti].lastUl = npuschsubframes[0].second.back ();
              m_rntiUeConfigMap[message.rnti].lastUlStart = npuschsubframes[0].second.front ();
              //NS_BUILD_DEBUG (std::cout << std::endl);
              return true;
            }
        }
    }
  return false;
}

std::vector<NbIotRrcSap::NpdcchMessage>
NbiotScheduler::ScheduleSearchSpace (SearchSpaceConfig ssc)
{
  std::vector<NbIotRrcSap::NpdcchMessage> scheduledMessages;

  //AddRntiDatatoNpdcchQueue(seachspace);

  // priorities rar
  for (std::vector<NbIotRrcSap::NpdcchMessage>::iterator rar = m_rarQueue[ssc].begin ();
       rar != m_rarQueue[ssc].end ();)
    {
      if (ScheduleNpdcchMessage ((*rar), ssc))
        {
          scheduledMessages.push_back (*rar);
          m_rarQueue[ssc].erase (rar);
        }
      else
        {
          ++rar;
        }
    }
  /*
  Scheduling Magic. For now FIFO
  */
  SortBasedOnSelectedSchedulingAlgorithm (ssc);
  //std::vector<uint16_t> test_tmp = m_searchSpaceRntiMap[ssc];
  for (std::vector<uint16_t>::iterator it = m_searchSpaceRntiMap[ssc].begin ();
       it != m_searchSpaceRntiMap[ssc].end ();)
    {
      NbIotRrcSap::NpdcchMessage dci_candidate;
      if (m_rntiUeConfigMap[(*it)].priority == UeConfig::SchedulePriority::DOWNLINK)
        {
          if (m_rntiUeConfigMap[(*it)].rlcDlBuffer > 0)
            {
              dci_candidate =
                  CreateDciNpdcchMessage ((*it), NbIotRrcSap::NpdcchMessage::DciType::n1);
              bool okN1 = ScheduleNpdcchMessage (dci_candidate, ssc);
              if (NbIotDebugTrace ())
                std::cout << "[SCHED-N1] sf=" << (10 * (m_frameNo - 1) + m_subframeNo - 1)
                          << " rnti=" << (*it) << " prio=DL dlBuf="
                          << m_rntiUeConfigMap[(*it)].rlcDlBuffer << " npdcchOk=" << okN1
                          << " tbs=" << dci_candidate.tbs << std::endl;
              if (okN1)
                {
                  scheduledMessages.push_back (dci_candidate);
                  m_rntiUeConfigMap[(*it)].rlcDlBuffer = 0;
                  m_RoundRobinLastScheduled[ssc] = (*it);
                  m_rntiUeConfigMap[(*it)].priority = UeConfig::SchedulePriority::UPLINK;
                }
            }
          else if (m_rntiUeConfigMap[(*it)].rlcUlBuffer > 0)
                {
                  dci_candidate =
                      CreateDciNpdcchMessage ((*it), NbIotRrcSap::NpdcchMessage::DciType::n0);
                  bool okN0dl = ScheduleNpdcchMessage (dci_candidate, ssc);
                  if (NbIotDebugTrace ())
                    std::cout << "[SCHED-N0] rnti=" << (*it) << " prio=DL ulBuf="
                              << m_rntiUeConfigMap[(*it)].rlcUlBuffer << " npdcchOk=" << okN0dl
                              << " tbs=" << dci_candidate.tbs << std::endl;
                  if (okN0dl)
                    {
                      scheduledMessages.push_back (dci_candidate);
                      if (int (m_rntiUeConfigMap[(*it)].rlcUlBuffer - (dci_candidate.tbs / 8)) > 0)
                        {
                          m_rntiUeConfigMap[(*it)].rlcUlBuffer =
                              m_rntiUeConfigMap[(*it)].rlcUlBuffer - (dci_candidate.tbs / 8);
                        }
                      else
                        {
                          m_rntiUeConfigMap[(*it)].rlcUlBuffer = 0;
                        }
                      m_RoundRobinLastScheduled[ssc] = (*it);
                    }
                }
            }
      else if (m_rntiUeConfigMap[(*it)].priority == UeConfig::SchedulePriority::UPLINK)
        {
          if (m_rntiUeConfigMap[(*it)].rlcUlBuffer > 0)
            {
              dci_candidate =
                  CreateDciNpdcchMessage ((*it), NbIotRrcSap::NpdcchMessage::DciType::n0);
              bool okN0 = ScheduleNpdcchMessage (dci_candidate, ssc);
              if (NbIotDebugTrace ())
                std::cout << "[SCHED-N0] rnti=" << (*it) << " prio=UL ulBuf="
                          << m_rntiUeConfigMap[(*it)].rlcUlBuffer << " npdcchOk=" << okN0
                          << " tbs=" << dci_candidate.tbs << std::endl;
              if (okN0)
                {
                  scheduledMessages.push_back (dci_candidate);
                  if (int (m_rntiUeConfigMap[(*it)].rlcUlBuffer - (dci_candidate.tbs / 8)) > 0)
                    {
                      m_rntiUeConfigMap[(*it)].rlcUlBuffer =
                          m_rntiUeConfigMap[(*it)].rlcUlBuffer - (dci_candidate.tbs / 8);
                    }
                  else
                    {
                      m_rntiUeConfigMap[(*it)].rlcUlBuffer = 0;
                    }
                  m_RoundRobinLastScheduled[ssc] = (*it);
                  m_rntiUeConfigMap[(*it)].priority = UeConfig::SchedulePriority::DOWNLINK;
                }
            }
          else if (m_rntiUeConfigMap[(*it)].rlcDlBuffer > 0)
            {
              dci_candidate =
                  CreateDciNpdcchMessage ((*it), NbIotRrcSap::NpdcchMessage::DciType::n1);
              if (ScheduleNpdcchMessage (dci_candidate, ssc))
                {
                  scheduledMessages.push_back (dci_candidate);
                  m_rntiUeConfigMap[(*it)].rlcDlBuffer = 0;
                  m_RoundRobinLastScheduled[ssc] = (*it);
                }
            }
        }
      ++it;
    }

  return scheduledMessages;
}

std::vector<std::pair<uint64_t, std::vector<uint64_t>>>
NbiotScheduler::GetNextAvailableNpuschCandidate (uint64_t endSubframeNpdsch,
                                                 uint64_t minSchedulingDelay, uint64_t numSubframes,
                                                 bool isHarq)
{
  std::vector<std::pair<uint64_t, std::vector<uint64_t>>> allocation;
  if (isHarq)
    {
      // Spec HARQ time offsets first (shift=0 == the original behavior). Under a deep
      // NPUSCH backlog (cold-start herd) ALL of them can be booked; previously that
      // failed the WHOLE DCI N1, blocking every DL to the UE (incl. the 4-byte RLC-AM
      // status/ACK) until the backlog drained (~25 s at N=80) -- the UE's
      // t-PollRetransmit then forced a duplicate retransmission the eNB dropped below
      // the receive window. Fall back to progressively later anchors (bounded) so the
      // DL itself proceeds; a late HARQ-ACK slot is a benign model approximation (no
      // radio loss modeled), and the half-duplex guard in
      // GetNextAvailableSearchSpaceCandidate clears DL candidates on either side of a
      // far-future UL booking.
      // Cap the fallback at 2000 sf (2 s): enough to clear a real cold-start NPUSCH
      // queue, but never deep enough to fight the SR arms' pre-booked grant lattice
      // or let a pathological rescheduling loop book the UL map solid (observed:
      // an eternal N1 loop pushed anchors 46+ s out and ground the sim to a crawl
      // before this cap). Beyond the cap the N1 fails exactly like pre-fallback code.
      for (uint64_t shift = 0; shift <= 2000;
           shift += (numSubframes > 0 ? numSubframes : 16))
        {
          for (auto &i : m_HarqTimeOffsets)
            {
              for (size_t j = 0; j < 4; ++j)
                { // For subcarrier 0-3 for 15khz Subcarrier spacing | needs change for 3.75 Khz
                  uint64_t candidate =
                      endSubframeNpdsch + shift +
                      NbIotRrcSap::HarqAckResource::ConvertHarqTimeOffset2int (i);
                  std::vector<uint64_t> subframesOccupied =
                      GetUlSubframeRangeWithoutSystemResources (candidate, numSubframes, j);
                  subframesOccupied =
                      CheckforNContiniousSubframesUl (subframesOccupied, candidate, numSubframes, j);
                  if (subframesOccupied.size () > 0)
                    {
                      if (shift > 0 && NbIotDebugTrace ())
                        std::cout << "[HARQ-DEFER] anchor shifted +" << shift
                                  << "sf (spec offsets booked)" << std::endl;
                      allocation.push_back (std::make_pair (j, subframesOccupied));
                      return allocation;
                    }
                }
            }
        }
    }
  else
    {
      /*TODO NPUSCH DELAYS ETC*/
      for (auto &i : m_DciTimeOffsetUplink)
        {
          for (size_t j = 0; j < 4; ++j)
            { // For subcarrier 0-3 for 15khz Subcarrier spacing | needs change for 3.75 Khz
              NbIotRrcSap::DciN0 tmp;
              tmp.npuschSchedulingDelay = i;
              uint64_t candidate = endSubframeNpdsch +
                                   NbIotRrcSap::ConvertNpuschSchedulingDelay2int (tmp) +
                                   1; // Start on next frame aber minSchedulingDelay
              std::vector<uint64_t> subframesOccupied =
                  GetUlSubframeRangeWithoutSystemResources (candidate, numSubframes, j);
              subframesOccupied =
                  CheckforNContiniousSubframesUl (subframesOccupied, candidate, numSubframes, j);
              if (subframesOccupied.size () > 0)
                {
                  return allocation;
                }
            }
        }
    }
  return std::vector<std::pair<uint64_t, std::vector<uint64_t>>> ();
}

std::pair<NbIotRrcSap::UlGrant, std::pair<uint64_t, std::vector<uint64_t>>>
NbiotScheduler::GetNextAvailableMsg3UlGrantCandidate (uint64_t endSubframeMsg2,
                                                      uint64_t numSubframes)
{
  for (auto &i : m_Msg3TimeOffset)
    {
      for (size_t j = 0; j < m_uplink.size (); ++j)
        {
          uint64_t candidate = endSubframeMsg2 +
                               NbIotRrcSap::UlGrant::ConvertUlGrantSchedulingDelay2int (i) +
                               1; // Start one subframe after delay
          std::vector<uint64_t> subframesOccupied =
              GetUlSubframeRangeWithoutSystemResources (candidate, numSubframes, j);
          subframesOccupied =
              CheckforNContiniousSubframesUl (subframesOccupied, candidate, numSubframes, j);
          if (subframesOccupied.size () > 0)
            {
              NbIotRrcSap::UlGrant ret = {};
              ret.schedulingDelay = i;
              ret.msg3Repetitions = NbIotRrcSap::UlGrant::Msg3Repetitions::r4;
              ret.subcarrierIndication = j;
              ret.Subcarrierspacing = 1;
              ret.success = true;
              ret.subframes = std::make_pair (j, subframesOccupied);
              return std::make_pair (ret, std::make_pair (j, subframesOccupied));
            }
        }
    }
  NbIotRrcSap::UlGrant ret = {};
  ret.success = false;
  return std::make_pair (ret, std::make_pair (uint64_t (), std::vector<uint64_t> ()));
}
std::vector<uint64_t>
NbiotScheduler::GetNextAvailableNpdschCandidate (uint64_t endSubframeDci,
                                                 uint64_t minSchedulingDelay, uint64_t numSubframes,
                                                 uint64_t R_max)
{

  uint64_t npdschCandidate = endSubframeDci + minSchedulingDelay +
                             1; // Start on the next Subframe of afer minSchedulingDelay
      // |0|1|2|3|4|5|6|7|8|9|10|
      //     |^  |^      |^     |
      //     |DCI|Delay  |NPDSCH|
  if (R_max < 128)
    {
      for (auto &i : m_DciTimeOffsetRmaxSmall)
        {
          NbIotRrcSap::DciN1 tmp; /// FIX AS SOON AS POSSIBLE
          tmp.npdcchTimeOffset = i;
          uint64_t tmpCandidate = npdschCandidate + NbIotRrcSap::ConvertNpdcchTimeOffset2int (tmp);
          std::vector<uint64_t> subframesOccupied =
              GetDlSubframeRangeWithoutSystemResources (tmpCandidate, numSubframes);
          subframesOccupied =
              CheckforNContiniousSubframesDl (subframesOccupied, tmpCandidate, numSubframes);
          if (subframesOccupied.size () > 0)
            {
              return subframesOccupied;
            }
        }
    }
  else
    {
      for (auto &i : m_DciTimeOffsetRmaxBig)
        {
          NbIotRrcSap::DciN1 tmp; /// FIX AS SOON AS POSSIBLE
          tmp.npdcchTimeOffset = i;
          uint64_t tmpCandidate = npdschCandidate + NbIotRrcSap::ConvertNpdcchTimeOffset2int (tmp);
          std::vector<uint64_t> subframesOccupied =
              GetDlSubframeRangeWithoutSystemResources (tmpCandidate, numSubframes);
          subframesOccupied =
              CheckforNContiniousSubframesDl (subframesOccupied, tmpCandidate, numSubframes);
          if (subframesOccupied.size () > 0)
            {
              return subframesOccupied;
            }
        }
    }
  return std::vector<uint64_t> ();
}

std::vector<uint64_t>
NbiotScheduler::GetDlSubframeRangeWithoutSystemResources (uint64_t overallSubframeNo,
                                                          uint64_t numSubframes)
{
  std::vector<uint64_t> subframeIndexes;
  size_t i = 0; // Starting on the given Subframe
  m_currenthyperindex = 1;
  while (numSubframes > 0)
    {
      size_t currentindex = overallSubframeNo + i;
      // Bound the scan to the grid (see the UL twin): without this guard a saturated
      // grid makes the loop run off the end of m_downlink -> unbounded push_back ->
      // std::bad_alloc. Return empty so allocation fails gracefully for this occasion.
      if (currentindex >= m_downlink.size ())
        return std::vector<uint64_t> ();
      if ((m_downlink[currentindex] == 0))
        {
          subframeIndexes.push_back (currentindex);
          numSubframes--;
        }
      i++;
    }
  return subframeIndexes;
}

std::vector<uint64_t>
NbiotScheduler::GetUlSubframeRangeWithoutSystemResources (uint64_t overallSubframeNo,
                                                          uint64_t numSubframes, uint64_t carrier)
{
  std::vector<uint64_t> subframeIndexes;
  size_t i = 0;
  m_currenthyperindex = 1;
  while (numSubframes > 0)
    {
      size_t currentindex = overallSubframeNo + i;
      // Bound the scan to the grid. When the uplink grid is saturated (heavy
      // contention at high density), there may be no run of free subframes before
      // the grid ends; without this guard the loop walks off the end of the vector
      // (out-of-bounds operator[] reads garbage), push_backs unboundedly and
      // exhausts memory (std::bad_alloc). Return empty -> allocation fails for this
      // occasion and the caller defers the UE, as it already does for "no resources".
      if (currentindex >= m_uplink[carrier].size ())
        return std::vector<uint64_t> ();
      if ((m_uplink[carrier][currentindex] != -1))
        {
          subframeIndexes.push_back (currentindex);
          numSubframes--;
        }
      i++;
    }
  return subframeIndexes;
}
std::vector<uint64_t>
NbiotScheduler::CheckforNContiniousSubframesDl (std::vector<uint64_t> Subframes,
                                                uint64_t StartSubframe, uint64_t N)
{
  int startSubframeIndex = -1;
  std::vector<uint64_t> range;
  for (size_t i = 0; i < Subframes.size (); ++i)
    {
      if (Subframes[i] == StartSubframe)
        {
          startSubframeIndex = i;
          break;
        }
    }
  if (startSubframeIndex == -1)
  {
    return std::vector<uint64_t> ();
  }


  if (startSubframeIndex + N > Subframes.size ())
  {
    // when that happens N = 32, but Subframes contain only 8 elements.
   // std::cout << " Subframes:" << Subframes.size() << " N=" << N << " startSubframeIndex:" << startSubframeIndex << std::endl;

    return std::vector<uint64_t> ();
  }

  // Adjust N to avoid out-of-bounds access
  // BUG: this fix the segmentation fault, but the root cause should be investigated further.
  // There 3 places in nb-iot-scheduler where CheckforNContiniousSubframesDl is called -> verify them
  // N = std::min(N, Subframes.size() - startSubframeIndex);  // trick: don´t use it

  for (size_t i = 0; i < N; i++)
  {

    // BUG: m_downlink[Subframes[startSubframeIndex + i]] raises Segmentation fault in some cases.
    // It looks like N is too big (> Subframes.size() - startSubframeIndex).
    if (m_downlink[Subframes[startSubframeIndex + i]] > 0) // if > 0, then subframes are already used by user specific data
      {
        return std::vector<uint64_t> ();
      }
    else
      {
        range.push_back (Subframes[startSubframeIndex + i]);
      }
  }/*
  if (must_show)
  {
    std::cout << "range.size()=" << range.size() << std::endl;
  }*/
  return range;
}
std::vector<uint64_t>
NbiotScheduler::CheckforNContiniousSubframesUl (std::vector<uint64_t> Subframes,
                                                uint64_t StartSubframe, uint64_t N,
                                                uint64_t carrier)
{
  int startSubframeIndex = -1;
  std::vector<uint64_t> range;
  for (size_t i = 0; i < Subframes.size (); ++i)
    {
      if (Subframes[i] == StartSubframe)
        {
          startSubframeIndex = i;
          break;
        }
    }
  if (startSubframeIndex == -1)
    {
      return std::vector<uint64_t> ();
    }

  for (size_t i = 0; i < N; i++)
    {
      if (m_uplink[carrier][Subframes[startSubframeIndex + i]] > 0) // if > 0, then subframes are already used by user specific data
        {
          return std::vector<uint64_t> ();
        }
      else
        {
          range.push_back (Subframes[startSubframeIndex + i]);
        }
    }
  return range;
}
std::vector<uint64_t>
NbiotScheduler::GetNextAvailableSearchSpaceCandidate (uint32_t rnti, uint64_t SearchSpaceStartFrame,
                                                      uint64_t SearchSpaceStartSubframe,
                                                      uint64_t R_max, uint64_t R)
{
  uint64_t u_max = ((R_max / R) - 1);
  uint64_t overallSubframe = 10 * (SearchSpaceStartFrame) + SearchSpaceStartSubframe;
  std::vector<uint64_t> subframes =
      GetDlSubframeRangeWithoutSystemResources (overallSubframe, R_max);
  for (size_t i = 0; i <= u_max; ++i)
    {
      // Calculate start of dci candidate
      std::vector<uint64_t> subframes_to_use =
          CheckforNContiniousSubframesDl (subframes, subframes[i * R], R);

      if (subframes_to_use.size () > 0)
        {
          // Half-duplex guard: the UE cannot monitor NPDCCH while transmitting its
          // pending NPUSCH allocation [lastUlStart, lastUl] (+3 sf guard band). The
          // old form (candidate must start AFTER lastUl+3) also rejected candidates
          // completing well BEFORE the queued UL even starts: under a deep NPUSCH
          // backlog (cold-start herd) lastUl sits many seconds ahead, which froze
          // ALL downlink to this UE (incl. the 4-byte RLC-AM status/ACK) for the
          // whole backlog depth -- the UE's t-PollRetransmit then forced a duplicate
          // retransmission the eNB drops below the receive window. Accept candidates
          // that clear the UL window on EITHER side.
          uint64_t ulStart = m_rntiUeConfigMap[rnti].lastUlStart;
          uint64_t ulEnd = m_rntiUeConfigMap[rnti].lastUl;
          // The NPDSCH ride follows the NPDCCH candidate after the minimum DCI->DL
          // scheduling delay; keep that pipeline tail clear of the UL window too.
          uint64_t dlTailEnd =
              subframes_to_use.back () + m_minSchedulingDelayDci2Downlink + 8;
          bool clearsAfterUl = subframes_to_use.front () > ulEnd + 3;
          bool clearsBeforeUl = (ulStart > 3) && (dlTailEnd + 3 < ulStart);
          if (clearsAfterUl || clearsBeforeUl)
            {
              return subframes_to_use;
            }
        }
    }
  return std::vector<uint64_t> ();
}

NbIotRrcSap::NpdcchMessage
NbiotScheduler::CreateDciNpdcchMessage (uint16_t rnti, NbIotRrcSap::NpdcchMessage::DciType dci_type)
{
  double correction_factor =
      10 *
      log10 (
          1.0 /
          12.0); // correctionfactor applied to rsrp because it's for earch subcarrier and tx power is for full spectrum

  //NS_BUILD_DEBUG (std::cout << "MCL of " << rnti << " is " << m_rntiRsrpMap[rnti] - 43.0 - correction_factor << std::endl);

  NbIotRrcSap::DciN1::DciRepetitions dciN1Repetitions;
  NbIotRrcSap::DciN0::DciRepetitions dciN0Repetitions;

  NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel ceLevel = m_rntiUeConfigMap[rnti].searchSpaceConfig.ce;

  if (ceLevel == m_ce2.coverageEnhancementLevel)
  {
    dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r256;
    dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r256;
  }
  else if (ceLevel == m_ce1.coverageEnhancementLevel)
  {
    dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r32;
    dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r32;
  }
  else
  {
    dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r2;
    dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r2;
  }

  /*if (m_rntiRsrpMap[rnti] < m_sib2config.radioResourceConfigCommon.nprachConfig
                                .rsrpThresholdsPrachInfoList.ce2_lowerbound)
    {
      dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r256;
      dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r256;
      ceLevel = m_ce2.coverageEnhancementLevel;
    }
  else if (m_rntiRsrpMap[rnti] < m_sib2config.radioResourceConfigCommon.nprachConfig
                                     .rsrpThresholdsPrachInfoList.ce1_lowerbound)
    {
      if (NbIotDebugTrace ())
        std::cout << "[SCHED-CE1] rnti=" << rnti << " rsrp=" << m_rntiRsrpMap[rnti]
                  << " < ce1_lowerbound=" << m_sib2config.radioResourceConfigCommon.nprachConfig
                                                 .rsrpThresholdsPrachInfoList.ce1_lowerbound
                  << std::endl;
      dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r32;
      dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r32;
      ceLevel = m_ce1.coverageEnhancementLevel;
    }
  else if (m_rntiRsrpMap[rnti] > m_sib2config.radioResourceConfigCommon.nprachConfig
                                     .rsrpThresholdsPrachInfoList.ce1_lowerbound)
    {
      dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r2;
      dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r2;
      ceLevel = m_ce0.coverageEnhancementLevel;
    }
  else
    {
      dciN1Repetitions = NbIotRrcSap::DciN1::DciRepetitions::r2;
      dciN0Repetitions = NbIotRrcSap::DciN0::DciRepetitions::r2;
      ceLevel = m_ce0.coverageEnhancementLevel;
    }*/

  NbIotRrcSap::NpdcchMessage msg;
  msg.isRar = false;
  msg.rnti = rnti;
  msg.ce = ceLevel;

  if (dci_type == NbIotRrcSap::NpdcchMessage::DciType::n1)
    {
      uint64_t tbs = 0;

      if (m_rntiUeConfigMap[rnti].rlcDlBuffer * 8 > 680)
        { // max TBS Downlink Rel. 13
          tbs = 680;
        }
      else
        {
          tbs = (m_rntiUeConfigMap[rnti].rlcDlBuffer) * 8;
        }
      std::pair<NbIotRrcSap::DciN1, uint64_t> dci_tbs = m_Amc.getBareboneDciN1 (
          m_rntiRsrpMap[rnti] - 43.0 - correction_factor, tbs, "standalone");

      NbIotRrcSap::DciN1 dci = dci_tbs.first;

      dci.mCS = NbIotRrcSap::DciN1::MCS::one;
      dci.tbs = dci_tbs.second;
      dci.NDI = true;
      dci.dciRepetitions = dciN1Repetitions;

      msg.dciType = NbIotRrcSap::NpdcchMessage::DciType::n1;
      msg.dciN1 = dci;
      msg.tbs = dci_tbs.second;
    }
  else if (dci_type == NbIotRrcSap::NpdcchMessage::DciType::n0)
    {
      uint64_t tbs = 0;
      //std::cout << m_rntiRsrpMap[rnti] << std::endl;
      //NpuschMeasurementValues val = m_Amc.getMaxTbsforCl(m_rntiRsrpMap[rnti] - 43.0 - correction_factor,15000,15);

      if (m_rntiUeConfigMap[rnti].rlcUlBuffer * 8 >= 1000)
        { // max TBS Uplink Rel. 13
          tbs = 1000;
        }
      else
        {
          tbs = (10+m_rntiUeConfigMap[rnti].rlcUlBuffer) * 8;
        }

      std::pair<NbIotRrcSap::DciN0, uint64_t> dci_tbs =
          m_Amc.getBareboneDciN0 (m_rntiRsrpMap[rnti] - 43.0 - correction_factor, tbs, 15000, 15);

      NbIotRrcSap::DciN0 dci = dci_tbs.first;
      dci.mCS = NbIotRrcSap::DciN0::MCS::one;
      dci.tbs = dci_tbs.second;
      dci.NDI = true;
      dci.dciRepetitions = dciN0Repetitions;

      msg.dciType = NbIotRrcSap::NpdcchMessage::DciType::n0;
      msg.dciN0 = dci;
      msg.tbs = dci_tbs.second;
      msg.lcid = 0;
    }

  return msg;
}

//void
//NbiotScheduler::ScheduleMsg5Req (uint64_t rnti)
//{
//  m_RntiRlcUlBuffer[NbIotRrcSap::NpdcchMessage::SearchSpaceType::type2][rnti] = 14;
//}
void
NbiotScheduler::ScheduleDlRlcBufferReq (
    uint64_t rnti, std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> lcids)
{
  // Calculate RLC Buffer Size
  std::map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
  uint64_t buffer_size = 0;
  std::map<uint8_t, uint64_t> buffers;
  for (it = lcids.begin (); it != lcids.end (); ++it)
    {
      if (it->second.txQueueSize != 0 || it->second.retxQueueSize != 0 ||
          it->second.statusPduSize != 0)
        {
          buffer_size +=
              it->second.txQueueSize + it->second.retxQueueSize + it->second.statusPduSize;
          if (it->second.txQueueSize > 0)
            {
              buffer_size += 4; // RLC Header
            }
          if (it->second.retxQueueSize > 0)
            {
              buffer_size += 4; // RLC Header for retransmissions
            }
        }
    }


  m_rntiUeConfigMap[rnti].rlcDlBuffer = buffer_size;
  SearchSpaceConfig searchSpace = m_rntiUeConfigMap[rnti].searchSpaceConfig;
  std::vector<uint16_t>::iterator findit = std::find (
      m_searchSpaceRntiMap[searchSpace].begin (), m_searchSpaceRntiMap[searchSpace].end (), rnti);
  if (NbIotDebugTrace ())
    std::cout << "[SCHED-DLREQ] rnti=" << rnti << " dlBytes=" << buffer_size
              << " inSearchSpace=" << (findit != m_searchSpaceRntiMap[searchSpace].end ())
              << std::endl;
  if (findit == m_searchSpaceRntiMap[searchSpace].end ())
    {
      m_searchSpaceRntiMap[searchSpace].push_back (rnti);
    }
}
void
NbiotScheduler::ScheduleUlRlcBufferReq (uint64_t rnti, uint64_t dataSize)
{
  bool freshCfg = (m_rntiUeConfigMap.find (rnti) == m_rntiUeConfigMap.end ());
  m_rntiUeConfigMap[rnti].rlcUlBuffer = dataSize;
  SearchSpaceConfig searchSpace = m_rntiUeConfigMap[rnti].searchSpaceConfig;
  if (NbIotDebugTrace ())
    std::cout << "[SCHED-ULREQ] rnti=" << rnti << " bytes=" << dataSize
              << " freshCfg=" << freshCfg << " ssRmax=" << searchSpace.R_max
              << " ssStartSf=" << searchSpace.startSf << " ssOffset=" << searchSpace.offset
              << std::endl;
  std::vector<uint16_t>::iterator it = std::find (m_searchSpaceRntiMap[searchSpace].begin (),
                                                  m_searchSpaceRntiMap[searchSpace].end (), rnti);
  if (it == m_searchSpaceRntiMap[searchSpace].end ())
    {
      m_searchSpaceRntiMap[searchSpace].push_back (rnti);
    }
}

void
NbiotScheduler::AddToUlBufferReq (uint64_t rnti, uint64_t dataSize)
{
  if(m_rntiUeConfigMap.find(rnti) != m_rntiUeConfigMap.end()){
    m_rntiUeConfigMap[rnti].rlcUlBuffer += dataSize;
    SearchSpaceConfig searchSpace = m_rntiUeConfigMap[rnti].searchSpaceConfig;
    std::vector<uint16_t>::iterator it = std::find (m_searchSpaceRntiMap[searchSpace].begin (),
                                                    m_searchSpaceRntiMap[searchSpace].end (), rnti);
    if (it == m_searchSpaceRntiMap[searchSpace].end ())
      {
        m_searchSpaceRntiMap[searchSpace].push_back (rnti);
      }
  }
}

SearchSpaceConfig
NbiotScheduler::ConvertNpdcchConfigDedicatedNb2SearchSpaceConfig (
    NbIotRrcSap::NpdcchConfigDedicatedNb configDedicated)
{
  SearchSpaceConfig ssc;
  ssc.R_max = NbIotRrcSap::ConvertNpdcchNumRepetitions2int (configDedicated);
  ssc.startSf = NbIotRrcSap::ConvertNpdcchStartSfUss2double (configDedicated);
  ssc.offset = NbIotRrcSap::ConvertNpdcchOffsetUss2double (configDedicated);
  // CE Level not used in USS, but for simplicity set to ce0
  ssc.ce = NbIotRrcSap::NprachParametersNb::CoverageEnhancementLevel::none;
  return ssc;
}
SearchSpaceConfig
NbiotScheduler::ConvertNprachParametersNb2SearchSpaceConfig (NbIotRrcSap::NprachParametersNb ce)
{
  SearchSpaceConfig ssc;
  ssc.R_max = NbIotRrcSap::ConvertNpdcchNumRepetitionsRa2int (ce);
  ssc.startSf = NbIotRrcSap::ConvertNpdcchStartSfCssRa2double (ce);
  ssc.offset = NbIotRrcSap::ConvertNpdcchOffsetRa2double (ce);
  ssc.ce = ce.coverageEnhancementLevel;
  return ssc;
}

void
NbiotScheduler::RemoveUe (uint16_t rnti)
{
  if (m_rntiUeConfigMap.find (rnti) == m_rntiUeConfigMap.end ())
    {
      // Ue was not added to Scheduler
      return;
    }
  UeConfig ue = m_rntiUeConfigMap[rnti];
  std::vector<uint16_t>::iterator it =
      std::find (m_searchSpaceRntiMap[ue.searchSpaceConfig].begin (),
                 m_searchSpaceRntiMap[ue.searchSpaceConfig].end (), rnti);
  if (it != m_searchSpaceRntiMap[ue.searchSpaceConfig].end ())
    {
      m_searchSpaceRntiMap[ue.searchSpaceConfig].erase (it);
    }
  m_rntiUeConfigMap.erase (rnti);
}

void
NbiotScheduler::ParkUe (uint16_t rnti)
{
  // FUG suspend: stop actively scheduling this UE (so it isn't granted/woken
  // while RRC-suspended) but KEEP its context so it can resume contention-free.
  auto cfgIt = m_rntiUeConfigMap.find (rnti);
  if (cfgIt == m_rntiUeConfigMap.end ()) { return; }
  if (NbIotDebugTrace ())
    std::cout << "[SCHED-PARK] t=" << Simulator::Now ().GetSeconds () << " rnti=" << rnti
              << " wiping ulBuf=" << cfgIt->second.rlcUlBuffer
              << " keeping dlBuf=" << cfgIt->second.rlcDlBuffer << std::endl;
  
  cfgIt->second.rlcUlBuffer = 0;
  if (cfgIt->second.rlcDlBuffer == 0)
    {
      auto &ss = m_searchSpaceRntiMap[cfgIt->second.searchSpaceConfig];
      auto it = std::find (ss.begin (), ss.end (), rnti);
      if (it != ss.end ()) { ss.erase (it); }
    }
}

void
NbiotScheduler::SetProactiveMode (bool enable)
{
  m_proactiveMode = enable;
}

void
NbiotScheduler::SetProactiveRoundRobin (bool enable)
{
  m_proactiveRoundRobin = enable;
}

void
NbiotScheduler::NotifyUlArrival (uint16_t rnti, uint64_t nowSf)
{
  // Feed the per-UE traffic predictor an observed UL arrival. EWMA (alpha=0.5)
  // of the inter-arrival gap estimates the period; nextGrantSf is the predicted
  // time of the NEXT arrival -> the proactive grant target. >=2 arrivals are
  // needed before any period exists (bootstrap packets are served reactively).
  auto it = m_rntiUeConfigMap.find (rnti);
  if (it == m_rntiUeConfigMap.end ())
    return;
  UeConfig & ue = it->second;
  if (!ue.proactive)
    return;
  // Filter intra-epoch follow-up PDUs (blind grant + BSR-driven grants for the
  // SAME packet land ms apart). A genuinely new arrival is separated by a large
  // gap; any realistic ambient inter-arrival (>= seconds) clears this threshold,
  // while same-epoch PDUs (< 2 s apart) do not.
  const uint64_t MIN_EPOCH_GAP_SF = 2000; // 2 s
  if (ue.arrivalCount >= 1 && (nowSf - ue.lastArrivalSf) < MIN_EPOCH_GAP_SF)
    return;
  // promptCatch: this arrival is ~one clean period after the last (a normal,
  // on-time catch) -> safe to re-anchor the phase on it. A missed-epoch catch
  // (gap ~2x period) must NOT re-anchor, or the phase locks onto the late catch
  // and the UE stays a full epoch behind forever (a cascading tail).
  bool promptCatch = false;
  if (ue.arrivalCount >= 1 && nowSf > ue.lastArrivalSf)
    {
      uint64_t gap = nowSf - ue.lastArrivalSf;
      // Robust period estimation by MINIMUM plausible gap. For periodic ambient
      // traffic, a *missed* epoch only ever lengthens an inter-arrival (a doubled
      // or tripled gap), never shortens it -- so the true period is the SMALLEST
      // genuine inter-arrival observed. Tracking the running minimum is therefore
      // immune both to missed-epoch outliers (which inflate gaps) and to a single
      // bad EWMA sample. A cold-start floor rejects sub-epoch artifacts: a UE's
      // first connection often yields a trailing setup/bootstrap PDU tens of
      // seconds after the first data PDU (e.g. a ~25 s gap), far below any
      // realistic ambient epoch; such gaps advance the anchor but are not learned.
      if (gap >= kProactiveColdStartFloorSf)
        {
          if (ue.predPeriodSf == 0)
            {
              ue.predPeriodSf = gap;   // initial estimate
              promptCatch = true;
            }
          else if (gap <= ue.predPeriodSf * 3 / 5)
            {
              // Gap SIGNIFICANTLY shorter than the estimate (<= 0.6x). The estimate was
              // too LONG -- the classic case is locking on a missed first epoch (Markov
              // skipped it), so the initial gap was ~2x the true period; the real period
              // then shows up at ~0.5x and the old EWMA band [0.6,1.6]x rejected it,
              // wedging the push cadence at 2x forever (delivered every other packet,
              // ~half-period late). Running-minimum recovery: re-lock to this shorter
              // genuine period -- a missed epoch only ever LENGTHENS a gap, never shortens
              // it, so a much-shorter gap is real. The 0.6x threshold sits far above the
              // ~1 s retry-jitter (which is why the old raw running-MINIMUM ratcheted
              // downward), so this shortens on a true sub-harmonic but not on jitter.
              ue.predPeriodSf = gap;
              promptCatch = true;
            }
          else if (gap < ue.predPeriodSf * 8 / 5)
            {
              promptCatch = true;
              // Robust EWMA (alpha = 1/4) over gaps within (0.6, 1.6)x the estimate: an
              // on-time (or slightly late/early) catch. Missed-epoch outliers (>= 1.6x)
              // are ignored (they would inflate the period).
              ue.predPeriodSf = (ue.predPeriodSf * 3 + gap) / 4;
            }
          // else (gap >= 1.6x): missed-epoch outlier -- ignore for period estimation
        }
      // else: cold-start artifact -- advance anchor only (handled below)
    }
  ue.lastArrivalSf = nowSf;
  ue.arrivalCount++;
  // A real UL transmission confirms the UE was served: close any open retry
  // window and predict the next push.
  ue.pushRetriesLeft = 0;
  if (ue.predPeriodSf > 0)
    {
      // PHASE-LOCKED anchor (replaces the free-running anchor, which let the period
      // estimate absorb the grant-wait and drift unboundedly -- EWMA late/compounding,
      // min-gap early/missing). Re-base the next occasion directly on THIS arrival
      // (nowSf) + one period, biased a fixed lead EARLIER. Why this does NOT compound
      // like a naive re-base: nowSf is the *catch*, which is at most one retry window
      // (kProactivePushRetries x kProactivePushRetrySf) after the occasion's first
      // push -- a BOUNDED reference, never the multi-epoch grant-wait. The early lead
      // makes the data land just inside the next retry window and be caught on an
      // early retry, so the catch latency settles to a small CONSTANT (~lead) instead
      // of feeding back. Net: the push phase relocks to the true data cadence every
      // epoch, so neither the phase nor the period can drift.
      if (promptCatch || ue.nextGrantSf == 0)
        {
          uint64_t next = nowSf + ue.predPeriodSf;   // clean catch -> phase-lock re-anchor
          ue.nextGrantSf = (next > kProactivePhaseLeadSf) ? (next - kProactivePhaseLeadSf) : next;
        }
      else
        {
          // Missed-epoch catch: do NOT re-anchor onto the late arrival (that would
          // drag the phase a full epoch behind and cascade). Free-run from the prior
          // prediction so the UE re-locks on the next on-time epoch.
          ue.nextGrantSf += ue.predPeriodSf;
          while (nowSf >= ue.nextGrantSf) ue.nextGrantSf += ue.predPeriodSf;
        }
    }
}

uint64_t
NbiotScheduler::GetProactiveGrantsIssued () const
{
  return m_proactiveGrantsIssued;
}


/*
- m_uplink is a 2D vector, where the first index is the subcarrier index and
the second index is the subframe index
actually, based on how it is resized, we can say that for

Level       Typical Value   Notes
Subframe    1 ms	          LTE basic time unit*
Frame       10 subframes    10 ms
Hyperframe  1024 frames     Used in some NB-IoT modes

then
j = 0  ==> Hyperframe 0, Frame 0, Subframe 0
j = 1  ==> Hyperframe 0, Frame 0, Subframe 1
...
j = 10 ==> Hyperframe 0, Frame 1, Subframe 0

* the loop below is based on the assumption that the subframe is always 1 ms!

m_uplink[i][j] == rnti → UE with RNTI rnti transmitting uplink at that time/carrier
if -1, there is no transmission at that time/carrier

- m_downlink is a 1D vector, where the index is the subframe index (i.e. Hyperframe-Frame-Subframe).
The value is the RNTI of the UE that is receiving downlink at that time or -1 MIB-NB, -2 NPSS, -3 NSSS, -4 SIB1-NB, -5 repetitions
*/
void
NbiotScheduler::LogUplinkGrid ()
{
  std::string logfile_path = m_logdir+"Spectral_Uplink.log";
  std::ofstream logfile;
  logfile.open (logfile_path, std::ios_base::app);
  // for (size_t i = 0; i < m_uplink.size (); i++)
  //   {
  //     for (int64_t j = 0; j < Simulator::Now ().GetMilliSeconds (); j++)
  //       {
  //         logfile << m_uplink[i][j] << ",";
  //       }
  //     logfile << "\n";
  //   }
    for (int64_t j = 0; j < Simulator::Now ().GetMilliSeconds (); j++){
      for (size_t i = 0; i < m_uplink.size (); i++){
        logfile << m_uplink[i][j] << ",";
      }
      logfile << "\n";
    }


  logfile.close ();
}

void NbiotScheduler::SetLogDir(std::string logdir){
  m_logdir = logdir;
  m_logging = !logdir.empty();
}

void
NbiotScheduler::LogDownlinkGrid ()
{
  std::string logfile_path = m_logdir+"Spectral_Downlink.log";
  std::ofstream logfile;
  logfile.open (logfile_path, std::ios_base::app);
  for (int64_t i = 0; i < Simulator::Now ().GetMilliSeconds (); i++)
    {
      //if((i/10)% 16 == 0 && (i%10) == 0){
      //  logfile << "\n";
      //}
      if (m_downlink[i] > -1)
        {
          logfile << " ";
        }
      //logfile  << " "<< m_downlink[i]<< ",";
      logfile << " " << m_downlink[i] << "\n";
    }

  logfile.close ();
}

// Stuff that could be useful later... could(!!!!!)

//std::vector<std::pair<uint64_t, uint64_t>>
//NbiotScheduler::GetAllPossibleSearchSpaceCandidates (std::vector<uint64_t> subframes, uint64_t R_max)
//{
//  std::vector<std::pair<uint64_t, uint64_t>> candidates;
//  m_currenthyperindex = 1;
//  uint64_t start_sf;
//  uint64_t length = 0;
//  uint64_t i = 0;
//  start_sf = subframes[0];
//  while (R_max > 0)
//    {
//      if (m_downlink[subframes[i]] != m_currenthyperindex)
//        {
//          length++;
//        }
//      else
//        {
//          if (length > 0)
//            {
//              candidates.push_back (std::make_pair (start_sf, length));
//            }
//          start_sf = subframes[i];
//          length = 0;
//        }
//      R_max--;
//      i++;
//    }
//  if (candidates.size () == 0 && length > 0)
//    {
//      candidates.push_back (std::make_pair (start_sf, length));
//    }
//  return candidates;
//}

} // namespace ns3

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * C-RNTI MAC Control Element tag (TS 36.321 6.1.3.2). Carried in Msg3 by a
 * connected UE performing Random Access (e.g. UL out-of-sync after PSM): it
 * conveys the UE's existing C-RNTI so the eNB resolves contention by C-RNTI
 * (5.1.5) and the UE keeps its identity instead of adopting the Temporary
 * C-RNTI. Modelled as a packet tag analogous to BufferStatusReportTag.
 */

#ifndef NBIOT_CRNTI_MAC_CE_TAG_H
#define NBIOT_CRNTI_MAC_CE_TAG_H

#include "ns3/tag.h"

namespace ns3 {

class CRntiMacCeTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  CRntiMacCeTag ();
  CRntiMacCeTag (uint16_t crnti);

  void SetCRnti (uint16_t crnti);   ///< the UE's existing C-RNTI to signal in Msg3
  uint16_t GetCRnti () const;

  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual uint32_t GetSerializedSize () const;
  virtual void Print (std::ostream &os) const;

private:
  uint16_t m_crnti;
};

} // namespace ns3

#endif /* NBIOT_CRNTI_MAC_CE_TAG_H */

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * UE Contention Resolution Identity tag (TS 36.321 5.1.5, 6.1.3.4). When two or
 * more UEs select the same NPRACH preamble in the same occasion they share one
 * Temporary C-RNTI and all receive the same Msg4 (RRC Connection Setup). To
 * resolve contention, Msg4 echoes the identity (here the IMSI carried by the
 * winning Msg3 RRC Connection Request); a UE that does not match discards Msg4
 * and re-RACHes when its T300 expires. Modelled as a DL packet tag carried on
 * the Msg4 packet, analogous to CRntiMacCeTag for Msg3.
 */

#ifndef NBIOT_CONTENTION_RESOLUTION_TAG_H
#define NBIOT_CONTENTION_RESOLUTION_TAG_H

#include "ns3/tag.h"

namespace ns3 {

class ContentionResolutionIdTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  ContentionResolutionIdTag ();
  ContentionResolutionIdTag (uint64_t imsi);

  void SetImsi (uint64_t imsi);   ///< identity (IMSI) of the winning Msg3 echoed in Msg4
  uint64_t GetImsi () const;

  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual uint32_t GetSerializedSize () const;
  virtual void Print (std::ostream &os) const;

private:
  uint64_t m_imsi;
};

} // namespace ns3

#endif /* NBIOT_CONTENTION_RESOLUTION_TAG_H */

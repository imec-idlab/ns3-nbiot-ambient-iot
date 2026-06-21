/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * UE Contention Resolution Identity tag (TS 36.321 5.1.5). See header.
 */

#include "nb-iot-contention-resolution-tag.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ContentionResolutionIdTag);

TypeId
ContentionResolutionIdTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ContentionResolutionIdTag")
    .SetParent<Tag> ()
    .SetGroupName ("Lte")
    .AddConstructor<ContentionResolutionIdTag> ()
    .AddAttribute ("imsi", "",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ContentionResolutionIdTag::GetImsi),
                   MakeUintegerChecker<uint64_t> ());
  return tid;
}

TypeId
ContentionResolutionIdTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

ContentionResolutionIdTag::ContentionResolutionIdTag () : m_imsi (0) {}
ContentionResolutionIdTag::ContentionResolutionIdTag (uint64_t imsi) : m_imsi (imsi) {}

void     ContentionResolutionIdTag::SetImsi (uint64_t imsi) { m_imsi = imsi; }
uint64_t ContentionResolutionIdTag::GetImsi () const { return m_imsi; }

uint32_t ContentionResolutionIdTag::GetSerializedSize (void) const { return 8; } // 64-bit IMSI
void     ContentionResolutionIdTag::Serialize (TagBuffer i) const { i.WriteU64 (m_imsi); }
void     ContentionResolutionIdTag::Deserialize (TagBuffer i) { m_imsi = i.ReadU64 (); }
void     ContentionResolutionIdTag::Print (std::ostream &os) const { os << "CR-Id(IMSI)=" << m_imsi; }

} // namespace ns3

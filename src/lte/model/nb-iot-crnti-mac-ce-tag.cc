/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>
 */


/*
 * C-RNTI MAC Control Element tag (TS 36.321 6.1.3.2). See header.
 */

#include "nb-iot-crnti-mac-ce-tag.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (CRntiMacCeTag);

TypeId
CRntiMacCeTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CRntiMacCeTag")
    .SetParent<Tag> ()
    .SetGroupName ("Lte")
    .AddConstructor<CRntiMacCeTag> ()
    .AddAttribute ("crnti", "",
                   UintegerValue (0),
                   MakeUintegerAccessor (&CRntiMacCeTag::GetCRnti),
                   MakeUintegerChecker<uint16_t> ());
  return tid;
}

TypeId
CRntiMacCeTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

CRntiMacCeTag::CRntiMacCeTag () : m_crnti (0) {}
CRntiMacCeTag::CRntiMacCeTag (uint16_t crnti) : m_crnti (crnti) {}

void     CRntiMacCeTag::SetCRnti (uint16_t crnti) { m_crnti = crnti; }
uint16_t CRntiMacCeTag::GetCRnti () const { return m_crnti; }

uint32_t CRntiMacCeTag::GetSerializedSize (void) const { return 2; } // 16-bit C-RNTI (TS 36.321 6.1.3.2)
void     CRntiMacCeTag::Serialize (TagBuffer i) const { i.WriteU16 (m_crnti); }
void     CRntiMacCeTag::Deserialize (TagBuffer i) { m_crnti = i.ReadU16 (); }
void     CRntiMacCeTag::Print (std::ostream &os) const { os << "C-RNTI=" << m_crnti; }

} // namespace ns3

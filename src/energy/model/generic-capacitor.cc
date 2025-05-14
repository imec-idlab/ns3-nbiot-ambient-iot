/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Andrea Sacco
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
 * Author: Andrea Sacco <andrea.sacco85@gmail.com>
 */

#include "generic-capacitor.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"

#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GenericCapacitor");

NS_OBJECT_ENSURE_REGISTERED (GenericCapacitor);

TypeId
GenericCapacitor::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GenericCapacitor")
    .SetParent<EnergySource> ()
    .SetGroupName ("Energy")
    .AddConstructor<GenericCapacitor> ()
    .AddAttribute ("GenericCapacitorInitialEnergyJ",
                   "Initial energy stored in basic energy source.",
                   DoubleValue (31752.0),  // in Joules
                   MakeDoubleAccessor (&GenericCapacitor::SetInitialEnergy,
                                       &GenericCapacitor::GetInitialEnergy),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("GenericCapacitorEnergyLowThreshold",
                   "Low energy threshold for capacitor.",
                   DoubleValue (0.10), // as a fraction of the initial energy
                   MakeDoubleAccessor (&GenericCapacitor::m_lowBatteryTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("InitialCellVoltage",
                   "Initial (maximum) voltage of the cell (fully charged).",
                   DoubleValue (4.05), // in Volts
                   MakeDoubleAccessor (&GenericCapacitor::SetInitialSupplyVoltage,
                                       &GenericCapacitor::GetSupplyVoltage),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("InternalResistance",
                   "Internal resistance of the cell",
                   DoubleValue (0.083),  // in Ohms
                   MakeDoubleAccessor (&GenericCapacitor::m_internalResistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TypCurrent",
                   "Typical discharge current used to fit the curves",
                   DoubleValue (2.33), // in A
                   MakeDoubleAccessor (&GenericCapacitor::m_typCurrent),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ThresholdVoltage",
                   "Minimum threshold voltage to consider the battery depleted.",
                   DoubleValue (3.3), // in Volts
                   MakeDoubleAccessor (&GenericCapacitor::m_minVoltTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PeriodicEnergyUpdateInterval",
                   "Time between two consecutive periodic energy updates.",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&GenericCapacitor::SetEnergyUpdateInterval,
                                     &GenericCapacitor::GetEnergyUpdateInterval),
                   MakeTimeChecker ())
    .AddTraceSource ("RemainingEnergy",
                     "Remaining energy at BasicEnergySource.",
                     MakeTraceSourceAccessor (&GenericCapacitor::m_remainingEnergyJ),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

GenericCapacitor::GenericCapacitor ()
  : m_capacitanceF (1.0),
    m_drainedCapacity (0.0),
    m_lastUpdateTime (Seconds (0.0))
{
  NS_LOG_FUNCTION (this);
}

GenericCapacitor::~GenericCapacitor ()
{
  NS_LOG_FUNCTION (this);
}

void
GenericCapacitor::SetInitialEnergy (double initialEnergyJ)
{
  NS_LOG_FUNCTION (this << initialEnergyJ);
  NS_ASSERT (initialEnergyJ >= 0);
  m_initialEnergyJ = initialEnergyJ;
  // set remaining energy to be initial energy
  m_remainingEnergyJ = m_initialEnergyJ;
  // set the capacitor initial voltage
  m_eFull = std::sqrt (2 * m_initialEnergyJ / m_capacitanceF);
  m_supplyVoltageV = m_eFull;
}

double
GenericCapacitor::GetInitialEnergy (void) const
{
  NS_LOG_FUNCTION (this);
  return m_initialEnergyJ;
}

void
GenericCapacitor::SetInitialSupplyVoltage (double supplyVoltageV)
{
  NS_LOG_FUNCTION (this << supplyVoltageV);
  m_eFull = supplyVoltageV;
  m_supplyVoltageV = supplyVoltageV;
  // set energy based on the initial voltage
  m_initialEnergyJ = m_capacitanceF * m_eFull * m_eFull / 2;
  m_remainingEnergyJ = m_initialEnergyJ;
}

double
GenericCapacitor::GetSupplyVoltage (void) const
{
  NS_LOG_FUNCTION (this);
  return m_supplyVoltageV;
}

void
GenericCapacitor::SetEnergyUpdateInterval (Time interval)
{
  NS_LOG_FUNCTION (this << interval);
  m_energyUpdateInterval = interval;
}

Time
GenericCapacitor::GetEnergyUpdateInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_energyUpdateInterval;
}

double
GenericCapacitor::GetRemainingEnergy (void)
{
  NS_LOG_FUNCTION (this);
  // update energy source to get the latest remaining energy.
  UpdateEnergySource ();
  return m_remainingEnergyJ;
}

double
GenericCapacitor::GetEnergyFraction (void)
{
  NS_LOG_FUNCTION (this);
  // update energy source to get the latest remaining energy.
  UpdateEnergySource ();
  return m_remainingEnergyJ / m_initialEnergyJ;
}

void
GenericCapacitor::DecreaseRemainingEnergy (double energyJ)
{
  NS_LOG_FUNCTION (this << energyJ);
  NS_ASSERT (energyJ >= 0);
  m_remainingEnergyJ -= energyJ;

  // check if remaining energy is 0
  if (m_supplyVoltageV <= m_minVoltTh)
  {
    HandleEnergyDrainedEvent ();
  }
}

void
GenericCapacitor::IncreaseRemainingEnergy (double energyJ)
{
  NS_LOG_FUNCTION (this << energyJ);
  NS_ASSERT (energyJ >= 0);
  m_remainingEnergyJ += energyJ;
}

void
GenericCapacitor::UpdateEnergySource (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("GenericCapacitor:Updating remaining energy at node #" <<
                GetNode ()->GetId ());

  // do not update if simulation has finished
  if (Simulator::IsFinished ())
    {
      return;
    }

  m_energyUpdateEvent.Cancel ();

  CalculateRemainingEnergy ();

  m_lastUpdateTime = Simulator::Now ();

  if (m_remainingEnergyJ <= m_lowBatteryTh * m_initialEnergyJ)
    {
      HandleEnergyDrainedEvent ();
      return; // stop periodic update
    }

  m_energyUpdateEvent = Simulator::Schedule (m_energyUpdateInterval,
                                             &GenericCapacitor::UpdateEnergySource,
                                             this);
}

/*
 * Private functions start here.
 */
void
GenericCapacitor::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  UpdateEnergySource ();  // start periodic update
}

void
GenericCapacitor::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  BreakDeviceEnergyModelRefCycle ();  // break reference cycle
}


void
GenericCapacitor::HandleEnergyDrainedEvent (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("GenericCapacitor:Energy depleted at node #" <<
                GetNode ()->GetId ());
  NotifyEnergyDrained (); // notify DeviceEnergyModel objects
}


void
GenericCapacitor::CalculateRemainingEnergy (void)
{
  NS_LOG_FUNCTION (this);
  double totalCurrentA = CalculateTotalCurrent ();
  
  // TODO: update this method to discharge and charge a capacitor

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetSeconds () >= 0);
  // energy = current * voltage * time
  double energyToDecreaseJ = totalCurrentA * m_supplyVoltageV * duration.GetSeconds ();

  if (m_remainingEnergyJ < energyToDecreaseJ) 
    {
      m_remainingEnergyJ = 0; // energy never goes below 0
    } 
  else 
    {
      m_remainingEnergyJ -= energyToDecreaseJ;
    }  

  m_drainedCapacity += (totalCurrentA * duration.GetSeconds () / 3600);
  // update the supply voltage
  m_supplyVoltageV = GetVoltage (totalCurrentA);
  NS_LOG_DEBUG ("GenericCapacitor:Remaining energy = " << m_remainingEnergyJ);
}

double
GenericCapacitor::GetVoltage (double i) const
{
  NS_LOG_FUNCTION (this << i);

  // TODO: update this method to obtain the voltage of the capacitor

  double E = 0;

  // cell voltage
  double V = 0;

  NS_LOG_DEBUG ("Voltage: " << V << " with E: " << E);

  return V;
}

} // namespace ns3

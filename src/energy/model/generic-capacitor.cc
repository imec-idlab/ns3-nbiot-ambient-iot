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
 * Author: Henrique Duarte Moura <henrique.duartemoura@imec.be>
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
    .AddAttribute ("Capacitance",
                   "Capacitor size in Farads.",
                   DoubleValue (0.01),  // in Farads
                   MakeDoubleAccessor (&GenericCapacitor::m_capacitanceF),
                   MakeDoubleChecker<double> (1e-9, 100.0))
    .AddAttribute ("InitialCapacitorVoltage",
                   "Initial voltage of the capacitor.",
                   DoubleValue (3.3), // in Volts
                   MakeDoubleAccessor (&GenericCapacitor::SetInitialVoltage,
                                       &GenericCapacitor::GetInitialVoltage),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxCapacitorVoltage",
                   "Maximym voltage of the capacitor.",
                   DoubleValue (5.0), // in Volts
                   MakeDoubleAccessor (&GenericCapacitor::m_maxVoltage),
                   MakeDoubleChecker<double> ())                   
    .AddAttribute ("ThresholdVoltage",
                   "Minimum threshold voltage to consider the battery depleted.",
                   DoubleValue (2.0), // in Volts
                   MakeDoubleAccessor (&GenericCapacitor::m_minVoltTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("InternalResistance",
                   "Internal series resistance of the capacitor",
                   DoubleValue (0.005),  // in Ohms
                   MakeDoubleAccessor (&GenericCapacitor::m_internalResistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("LeakageResistance",
                   "Internal leakage resistance of the capacitor (in parallel)",
                   DoubleValue (1e5),  // in Ohms
                   MakeDoubleAccessor (&GenericCapacitor::m_leakageResistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("GenericCapacitorInitialEnergyJ",
                   "Initial energy stored in basic energy source.",
                   DoubleValue (0.05445),  // in Joules
                   MakeDoubleAccessor (&GenericCapacitor::SetInitialEnergy,
                                       &GenericCapacitor::GetInitialEnergy),
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
  : m_capacitanceF (0.01),
    m_initialVoltage (3.3),
    m_currentVoltage (3.3),
    m_maxVoltage (5.0),
    m_minVoltTh (2.0),
    m_internalResistance (0.005),
    m_leakageResistance (1e5),
    m_initialEnergyJ (0.05445),
    m_remainingEnergyJ (0.05445),
    m_lastUpdateTime (Seconds (0.0))
{
  NS_LOG_FUNCTION (this);
}

GenericCapacitor::~GenericCapacitor ()
{
  NS_LOG_FUNCTION (this);
}


/**
 * \returns Energy stored in capacitor in Joules given a voltage in Volts.
 *
 * \param voltageV the voltage in Volts
 *
 * The energy stored in a capacitor is 0.5 * C * V^2, where C is the capacitance in Farads and V is the voltage in Volts.
 *
 */
double
GenericCapacitor::GetCapacitorEnergy (double voltageV) const
{
  return m_capacitanceF * voltageV * voltageV / 2;
}

/**
 * \param initialEnergyJ Initial energy stored in energy source, in Joules.
 *
 * Sets the initial energy stored in the capacitor.
 * To be called only once.
 *
 * This method is used to set the initial energy of the capacitor.
 */
void
GenericCapacitor::SetInitialEnergy (double initialEnergyJ)
{
  NS_LOG_FUNCTION (this << initialEnergyJ);
  NS_ASSERT (initialEnergyJ >= 0);
  m_initialEnergyJ = initialEnergyJ;
  // set remaining energy to be initial energy
  m_remainingEnergyJ = m_initialEnergyJ;
  // set the capacitor initial voltage
  m_initialVoltage = std::sqrt (2 * m_initialEnergyJ / m_capacitanceF);
  m_currentVoltage = m_initialVoltage;
}

double
GenericCapacitor::GetInitialEnergy (void) const
{
  NS_LOG_FUNCTION (this);
  return m_initialEnergyJ;
}

/**
 * \param initialVoltageV Initial voltage of the capacitor in Volts.
 *
 * Sets the initial voltage of the capacitor. The energy stored in the capacitor is then
 * calculated by GetCapacitorEnergy().
 * 
 * This method allows you to set the energy of the capacitor by providing the initial voltage.
 */
void
GenericCapacitor::SetInitialVoltage (double initialVoltageV)
{
  NS_LOG_FUNCTION (this << initialVoltageV);
  m_initialVoltage = initialVoltageV;
  m_currentVoltage = initialVoltageV;
  // set energy based on the initial voltage
  m_initialEnergyJ = GetCapacitorEnergy(m_initialVoltage);
  m_remainingEnergyJ = m_initialEnergyJ;
}

/**
 * \returns Initial voltage of the capacitor in Volts.
 *
 * This method is used to access the initial voltage at the capacitor.
 */
double 
GenericCapacitor::GetInitialVoltage (void) const
{
  return m_initialVoltage;
}

/**
 * \returns Current voltage at the capacitor.
 *
 * This method is used to access the current voltage at the capacitor.
 * 
 */
double
GenericCapacitor::GetSupplyVoltage (void) const
{
  NS_LOG_FUNCTION (this);
  return m_currentVoltage;
}

/*
- The interval set by SetEnergyUpdateInterval determines when the energy source recalculates 
  the remaining power. If you set a short interval, updates happen more frequently in simulation time.
- The simulation time itself progresses based on events scheduled in the event queue. 
  The energy updates occur as scheduled events at regular intervals.
- A smaller interval leads to more precise tracking but may increase computational overhead. 
- A larger interval reduces update frequency, potentially making energy consumption tracking less granular.
 */
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
  // TODO: update the voltage based on the remaining energy

  // check if remaining energy is 0
  if (m_currentVoltage <= m_minVoltTh)
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

  // TODO: update the voltage based on the remaining energy  
}


/**
 * \returns Energy in Joules when the capacitor voltage is at the
 *          minimum threshold voltage.
 *
 * This method is used to access the energy stored in the capacitor
 * when the capacitor voltage is at the minimum threshold voltage.
 */
double
GenericCapacitor::LowCapacitorEnergyTh(void) const
{
  return GetCapacitorEnergy(m_minVoltTh);
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

  CalculateRemainingEnergy ();  // update remaining energy

  m_lastUpdateTime = Simulator::Now ();

  double m_lowBatteryTh = LowCapacitorEnergyTh();

  if (m_remainingEnergyJ <= m_lowBatteryTh)
    {
      HandleEnergyDrainedEvent ();
      return; // stop periodic update
    }

  // schedule next periodic update
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
  NS_LOG_DEBUG ("GenericCapacitor:Energy depleted at node #" << GetNode ()->GetId ());
  NotifyEnergyDrained (); // notify DeviceEnergyModel objects
  if (m_remainingEnergyJ <= 0)
  {
      m_remainingEnergyJ = 0;  // energy never goes below 0
  }
}

double
GenericCapacitor::GetCapacitorModelVoltage(double ih, double t) const
{
  double req = m_leakageResistance;
  double tau = req * m_capacitanceF;
  double Vc = req * ih * (1 - std::exp(-t / tau)) + m_currentVoltage * std::exp(-t / tau);
  Vc = (Vc > m_maxVoltage) ? m_maxVoltage : Vc;   // clip voltage to maximum
  Vc = (Vc < 0) ? 0 : Vc;  // no negative voltage for us
  return Vc;
}

void
GenericCapacitor::CalculateRemainingEnergy (void)
{
  NS_LOG_FUNCTION (this);
  // compute total current, i.e.,
  // all current harvest minus all current drained by the devices attached
  double totalCurrentA = CalculateTotalCurrent (); 
  
  Time now = Simulator::Now ();
  Time duration = now - m_lastUpdateTime;  
  NS_ASSERT (duration.GetSeconds () >= 0);
  
  // update the supply voltage
  m_currentVoltage = GetCapacitorModelVoltage(totalCurrentA, duration.GetSeconds ());
  m_remainingEnergyJ = GetCapacitorEnergy(m_currentVoltage);

  NS_LOG_DEBUG ("GenericCapacitor:Remaining energy = " << m_remainingEnergyJ << " with V: " << m_currentVoltage << std::fixed << " at " << now.GetSeconds());
}

/**
 * \returns Current voltage at the capacitor.
 *
 * This method is used to access the current voltage at the capacitor.
 */
double
GenericCapacitor::GetVoltage (double i) const
{
  NS_LOG_FUNCTION (this << i);
  NS_LOG_DEBUG ("At " << std::fixed << Simulator::Now ().GetSeconds() << std::defaultfloat << " Voltage: " << m_currentVoltage << " with E: " << m_remainingEnergyJ);

  return m_currentVoltage;
}

} // namespace ns3

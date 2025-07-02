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

#include <cmath>

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"

#include "energy-markov.h"

#define ENERGY_MARKOV_LOG_NAME "EnergyMarkovData.log"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EnergyMarkov");

NS_OBJECT_ENSURE_REGISTERED (EnergyMarkov);

TypeId
EnergyMarkov::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EnergyMarkov")
    .SetParent<EnergySource> ()
    .SetGroupName ("Energy")
    .AddConstructor<EnergyMarkov> ()
    .AddAttribute ("EnergyMarkovInitialEnergyJ",
                   "Initial energy stored in basic energy source.",
                   DoubleValue (31752.0),  // in Joules
                   MakeDoubleAccessor (&EnergyMarkov::SetInitialEnergy,
                                       &EnergyMarkov::GetInitialEnergy),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Delta0",
                   "Transition probability from INACTIVE to ACTIVE state.",
                   DoubleValue (0.7), // default value
                   MakeDoubleAccessor (&EnergyMarkov::UpdateDelta0),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("Delta1",
                   "Transition probability from ACTIVE to INACTIVE state.",
                   DoubleValue (0.1), // default value
                   MakeDoubleAccessor (&EnergyMarkov::UpdateDelta1),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("InitialCellVoltage",
                   "Initial (maximum) voltage of the cell (fully charged).",
                   DoubleValue (4.05), // in Volts
                   MakeDoubleAccessor (&EnergyMarkov::SetInitialVoltage,
                                       &EnergyMarkov::GetSupplyVoltage),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PeriodicEnergyUpdateInterval",
                   "Time between two consecutive periodic energy updates.",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&EnergyMarkov::SetEnergyUpdateInterval,
                                     &EnergyMarkov::GetEnergyUpdateInterval),
                   MakeTimeChecker ())
    .AddTraceSource ("RemainingEnergy",
                     "Remaining energy at BasicEnergySource.",
                     MakeTraceSourceAccessor (&EnergyMarkov::m_remainingEnergyJ),
                     "ns3::TracedValueCallback::Double")
    .AddTraceSource ("State",
                     "Current state of the energy source.",
                     MakeTraceSourceAccessor (&EnergyMarkov::m_state),
                     "ns3::TracedValueCallback::Uint32");
  ;
  return tid;
}

EnergyMarkov::EnergyMarkov ()
  : m_drainedCapacity (0.0),
    m_lastUpdateTime (Seconds (0.0)),
    stateMachine(delta0, delta1)
{
  NS_LOG_FUNCTION (this);

  SetStateMachine(State::ACTIVE); // initialize the state machine
}

EnergyMarkov::~EnergyMarkov ()
{
  NS_LOG_FUNCTION (this);
}

void
EnergyMarkov::SetLogDir(std::string logdir){
  m_logdir = logdir;
}

void EnergyMarkov::SetStateMachine(State state) {
  stateMachine.set_delta0(delta0);
  stateMachine.set_delta1(delta1);
  stateMachine.set_state(state); // start in ACTIVE state
  m_state = stateMachine.current_state(); // get the initial state

  NS_LOG_DEBUG (GetHeader() << "State machine initialized to " <<
    static_cast<int>(m_state) <<
    " with delta0 = " << delta0 <<
    ", delta1 = " << delta1 <<
    " at " << Simulator::Now ().GetSeconds () << " s");
}

void
EnergyMarkov::UpdateDelta0(double delta0) {
  NS_LOG_FUNCTION (delta0);
  this->delta0 = delta0;
  SetStateMachine(State::ACTIVE);
}

void
EnergyMarkov::UpdateDelta1(double delta1) {
  NS_LOG_FUNCTION (delta1);
  this->delta1 = delta1;
  SetStateMachine(State::ACTIVE);
}

void
EnergyMarkov::SetInitialEnergy (double initialEnergyJ)
{
  NS_LOG_FUNCTION (this << initialEnergyJ);
  NS_ASSERT (initialEnergyJ >= 0);
  m_initialEnergyJ = initialEnergyJ;
  // set remaining energy to zero because the source is never drained
  m_remainingEnergyJ = 0;
}

double
EnergyMarkov::GetInitialEnergy (void) const
{
  NS_LOG_FUNCTION (this);
  return m_initialEnergyJ;
}

void
EnergyMarkov::SetInitialVoltage (double supplyVoltageV)
{
  NS_LOG_FUNCTION (this << supplyVoltageV);
  m_supplyVoltageV = supplyVoltageV;
}

double
EnergyMarkov::GetSupplyVoltage (void) const
{
  NS_LOG_FUNCTION (this);
  return m_supplyVoltageV;
}

void
EnergyMarkov::SetEnergyUpdateInterval (Time interval)
{
  NS_LOG_FUNCTION (this << interval);
  m_energyUpdateInterval = interval;
}

Time
EnergyMarkov::GetEnergyUpdateInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_energyUpdateInterval;
}

double
EnergyMarkov::GetRemainingEnergy (void)
{
  NS_LOG_FUNCTION (this);
  // update energy source to get the latest remaining energy.
  UpdateEnergySource ();
  return m_remainingEnergyJ;
}

double
EnergyMarkov::GetEnergyFraction (void)
{
  NS_LOG_FUNCTION (this);
  // update energy source to get the latest remaining energy.
  UpdateEnergySource ();
  return m_remainingEnergyJ / m_initialEnergyJ;
}

void
EnergyMarkov::DecreaseRemainingEnergy (double energyJ)
{
  NS_LOG_FUNCTION (this << energyJ);
  NS_ASSERT (energyJ >= 0);
  m_remainingEnergyJ -= energyJ;
  if (energyJ > 0)
    NS_LOG_DEBUG (GetHeader() << "Decrease remaining energy by " << energyJ << " at " <<
                  Simulator::Now ().GetSeconds () << " s");

  // check if device should go to INACTIVE state
  if (stateMachine.current_state() == State::INACTIVE)
    {
      HandleEnergyDrainedEvent ();
    }
}

void
EnergyMarkov::IncreaseRemainingEnergy (double energyJ)
{
  NS_LOG_FUNCTION (this << energyJ);
  NS_ASSERT (energyJ >= 0);
  m_remainingEnergyJ += energyJ;
}

void
EnergyMarkov::UpdateEnergySource (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG (GetHeader() << "Updating remaining energy at " <<
                Simulator::Now ().GetSeconds () << " s");

  // do not update if simulation has finished
  if (Simulator::IsFinished ())
    {
      return;
    }

  m_energyUpdateEvent.Cancel ();

  CalculateRemainingEnergy ();

  m_lastUpdateTime = Simulator::Now ();

  State prevState = stateMachine.current_state();
  stateMachine.step(); // step the state machine
  m_state = stateMachine.current_state(); // get the initial state

  if (prevState == stateMachine.current_state()) {
    NS_LOG_DEBUG (GetHeader() <<
      "State remained as " << static_cast<int>(prevState) << " at " <<
      Simulator::Now ().GetSeconds () << " s");
  }
  else {
    NS_LOG_DEBUG (GetHeader() <<
      "State changed from " << static_cast<int>(prevState) <<
      " to " << m_state <<
      " at " << Simulator::Now ().GetSeconds () << " s");
  }

  m_energyUpdateEvent = Simulator::Schedule (m_energyUpdateInterval,
                                             &EnergyMarkov::UpdateEnergySource,
                                             this);

  // if the state is INACTIVE, it means that the energy is depleted
  if (stateMachine.current_state() == State::INACTIVE)
    {
      HandleEnergyDrainedEvent ();
    }
}

/*
 * Private functions start here.
 */
void
EnergyMarkov::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  UpdateEnergySource ();  // start periodic update
}

void
EnergyMarkov::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  BreakDeviceEnergyModelRefCycle ();  // break reference cycle
}


void
EnergyMarkov::HandleEnergyDrainedEvent (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG (GetHeader() << "Energy depleted at " << Simulator::Now ().GetSeconds () << " s");
  NotifyEnergyDrained (); // notify DeviceEnergyModel objects
}


void
EnergyMarkov::CalculateRemainingEnergy (void)
{
  NS_LOG_FUNCTION (this);
  double totalCurrentA = CalculateTotalCurrent ();
  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetSeconds () >= 0);
  // energy = current * voltage * time
  double energyToDecreaseJ = totalCurrentA * m_supplyVoltageV * duration.GetSeconds ();

  // infinite energy source
  // m_remainingEnergyJ becomes consumed energy
  m_remainingEnergyJ -= energyToDecreaseJ;

  m_drainedCapacity += (totalCurrentA * duration.GetSeconds () / 3600);

  if (energyToDecreaseJ > 0) {
    NS_LOG_DEBUG (GetHeader() << "Remaining energy = " << m_remainingEnergyJ);
  }
}

double
EnergyMarkov::GetVoltage (double i) const
{
  NS_LOG_FUNCTION (this << i);

  NS_LOG_DEBUG (GetHeader() << "Voltage: " << m_supplyVoltageV << " with E: " << m_initialEnergyJ << "J at "<< Simulator::Now ().GetSeconds () << " s");

  return m_supplyVoltageV;
}

/**
 * \returns A string to be used as a prefix for output messages.
 *
 * This function is typically used by the logging functions to provide a
 * context for the message being logged.
 *
 * \see Object::GetHeader
 */
std::string
EnergyMarkov::GetHeader(void) const {
  std::ostringstream msg;
  msg << "EnergyMarkov("<< (GetNode () ? GetNode ()->GetId () : 0) << "): ";
  return msg.str();
}

} // namespace ns3

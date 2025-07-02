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

#ifndef ENERGY_MARKOV_H
#define ENERGY_MARKOV_H

#include "ns3/traced-value.h"
#include "ns3/energy-source.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

#include <random>
#include "ns3/energy-state-machine.h"


namespace ns3 {

/**
 * \ingroup energy
 * \brief Model a constanted energy source that can be active or not, which means that
 * the device attached to it can be active or not.
 * The state is given by a Markov chain model proposed in Moons et al. [1] that are controlled by two parameters:
 * - delta0: transition probability from INACTIVE to ACTIVE state
 * - delta1: transition probability from ACTIVE to INACTIVE state
 *
 *
 * References:
 * [1] Moons, L., Nasser, S., Sabovic, A., Singh, R.K. and Famaey, J., 2024, October. Evaluating Fast and Grant-Free Uplink Access in Next-Generation Cellular IoT Networks. In 2024 3rd International Conference on 6G Networking (6GNet) (pp. 19-24). IEEE.
 *
 */
class EnergyMarkov : public EnergySource
{
public:
  static TypeId GetTypeId (void);
  EnergyMarkov ();
  virtual ~EnergyMarkov ();

  /**
   * \return Initial energy stored in energy source, in Joules.
   *
   * Implements GetInitialEnergy.
   */
  virtual double GetInitialEnergy (void) const;

  /**
   * \param initialEnergyJ Initial energy, in Joules
   *
   * Implements SetInitialEnergy. Note that initial energy is assumed to be set
   * before simulation starts and is set only once per simulation.
   */
  void SetInitialEnergy (double initialEnergyJ);

  /**
   * \returns Supply voltage at the energy source.
   *
   * Implements GetSupplyVoltage.
   */
  virtual double GetSupplyVoltage (void) const;

  /**
   * \param supplyVoltageV Initial Supply voltage at the energy source, in Volts.
   *
   * Sets the initial supply voltage of the energy source.
   * To be called only once.
   */
  void SetInitialVoltage (double supplyVoltageV);

  /**
   * \return Remaining energy in energy source, in Joules
   *
   * Implements GetRemainingEnergy.
   */
  virtual double GetRemainingEnergy (void);

  /**
   * \returns Energy fraction.
   *
   * Implements GetEnergyFraction.
   */
  virtual double GetEnergyFraction (void);

  /**
   * \param energyJ Amount of energy (in Joules) to decrease from energy source.
   *
   * Implements DecreaseRemainingEnergy.
   */
  virtual void DecreaseRemainingEnergy (double energyJ);

  /**
   * \param energyJ Amount of energy (in Joules) to increase from energy source.
   *
   * Implements IncreaseRemainingEnergy.
   */
  virtual void IncreaseRemainingEnergy (double energyJ);

  /**
   * Implements UpdateEnergySource.
   */
  virtual void UpdateEnergySource (void);

  /**
   * \param interval Energy update interval.
   *
   * This function sets the interval between each energy update.
   */
  void SetEnergyUpdateInterval (Time interval);

  /**
   * \returns The interval between each energy update.
   */
  Time GetEnergyUpdateInterval (void) const;


  void SetLogDir(std::string logfile);

private:
  void DoInitialize (void);
  void DoDispose (void);

  /**
   * Handles the remaining energy going to zero event. This function notifies
   * all the energy models aggregated to the node about the energy being
   * depleted. Each energy model is then responsible for its own handler.
   */
  void HandleEnergyDrainedEvent (void);

  /**
   * Calculates remaining energy. This function uses the total current from all
   * device models to calculate the amount of energy to decrease. The energy to
   * decrease is given by:
   *    energy to decrease = total current * supply voltage * time duration
   * This function subtracts the calculated energy to decrease from remaining
   * energy.
   */
  void CalculateRemainingEnergy (void);

  /**
   *  \param current the actual discharge current value.
   *
   *  Get the cell voltage in function of the discharge current.
   *  It consider different discharge curves for different discharge currents
   *  and the remaining energy of the cell.
   */
  double GetVoltage (double current) const;

  void UpdateDelta0(double delta0);
  void UpdateDelta1(double delta1);

private:
  double delta0 = 0.7;                   // transition probability from INACTIVE to ACTIVE
  double delta1 = 0.1;                   // transition probability from ACTIVE to INACTIVE

  double m_initialEnergyJ;                // initial energy, in Joules
  TracedValue<double> m_remainingEnergyJ; // remaining energy, in Joules
  double m_drainedCapacity;               // capacity drained from the cell, in Ah
  double m_supplyVoltageV;                // actual voltage of the cell
  EventId m_energyUpdateEvent;            // energy update event
  Time m_lastUpdateTime;                  // last update time
  Time m_energyUpdateInterval;            // energy update interval

  EnergyStateMachine stateMachine;        // energy state machine
  TracedValue<uint32_t> m_state;

  std::string m_logdir = "";              // log directory for the capacitor traces

  std::string GetHeader(void) const;
  void SetStateMachine(State state);
};

} // namespace ns3



#endif /* ENERGY_MARKOV_H */

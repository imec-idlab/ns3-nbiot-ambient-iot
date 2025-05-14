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

#ifndef LI_ION_ENERGY_SOURCE_H
#define LI_ION_ENERGY_SOURCE_H

#include "ns3/traced-value.h"
#include "ns3/energy-source.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

namespace ns3 {

/**
 * \ingroup energy
 * \brief Model a generic capacitor.
 *
 *
 */
class GenericCapacitor : public EnergySource
{
public:
  static TypeId GetTypeId (void);
  GenericCapacitor ();
  virtual ~GenericCapacitor ();

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
  void SetInitialSupplyVoltage (double supplyVoltageV);

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

private:
  double m_capacitanceF;                  // capacity of the cell, in Farads
  double m_initialEnergyJ;                // initial energy, in Joules
  TracedValue<double> m_remainingEnergyJ; // remaining energy, in Joules
  double m_drainedCapacity;               // capacity drained from the cell, in Ah
  double m_supplyVoltageV;                // actual voltage of the cell
  double m_lowBatteryTh;                  // low battery threshold, as a fraction of the initial energy
  EventId m_energyUpdateEvent;            // energy update event
  Time m_lastUpdateTime;                  // last update time
  Time m_energyUpdateInterval;            // energy update interval
  double m_eFull;                         // initial voltage of the cell, in Volts
  double m_eNom;                          // nominal voltage of the cell, in Volts
  double m_internalResistance;            // internal resistance of the cell, in Ohms
  double m_typCurrent;                    // typical discharge current used to fit the curves
  double m_minVoltTh;                     // minimum threshold voltage to consider the battery depleted
};

} // namespace ns3

#endif /* LI_ION_ENERGY_SOURCE_H */

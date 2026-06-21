/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SOLAR_ENERGY_HARVESTER_H
#define SOLAR_ENERGY_HARVESTER_H

#include "energy-harvester.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/traced-value.h"

namespace ns3 {

/**
 * \ingroup energy
 *
 * SolarEnergyHarvester delivers power to its EnergySource according
 * to a diurnal sin^2 profile:
 *
 *     P_h(t) = PeakPower * sin^2( pi * (t / DayPeriod + PhaseOffset) )
 *
 * It owns its own periodic update event and computes the analytic
 * envelope value at each tick — no external retune callback or
 * RandomVariableStream is needed. The class is self-contained and
 * does not inherit from BasicEnergyHarvester; it implements the
 * EnergyHarvester contract directly.
 */
class SolarEnergyHarvester : public EnergyHarvester
{
public:
  static TypeId GetTypeId (void);
  SolarEnergyHarvester  ();
  virtual ~SolarEnergyHarvester ();

  void   SetUpdateInterval (Time interval);
  Time   GetUpdateInterval (void) const;

  void   SetPeakPower      (double watts);
  double GetPeakPower      (void) const;

  void   SetDayPeriod      (Time period);
  Time   GetDayPeriod      (void) const;

  void   SetPhaseOffset    (double phase);
  double GetPhaseOffset    (void) const;

private:
  virtual void   DoInitialize (void);
  virtual void   DoDispose    (void);

  // Implements EnergyHarvester::DoGetPower.
  virtual double DoGetPower   (void) const;

  // Periodic update: integrate the previous-interval power into the
  // accumulated harvested energy, refresh m_harvestedPower from the
  // sin^2 envelope at the new instant, push to the energy source,
  // re-arm the timer.
  void   UpdateHarvestedPower (void);

  // Analytic envelope, in Watts, evaluated at Simulator::Now().
  double CurrentEnvelopeValue (void) const;

  TracedValue<double> m_harvestedPower;        ///< current harvest power (W)
  TracedValue<double> m_totalEnergyHarvestedJ; ///< accumulated energy (J)

  EventId m_event;          ///< periodic update event
  Time    m_lastUpdate;     ///< wall time of previous update
  Time    m_updateInterval; ///< periodic update cadence

  double m_peakPower;       ///< Pmax (W)
  Time   m_dayPeriod;       ///< T_day (one full sin^2 hump)
  double m_phaseOffset;     ///< [0, 1)
};

} // namespace ns3

#endif /* SOLAR_ENERGY_HARVESTER_H */

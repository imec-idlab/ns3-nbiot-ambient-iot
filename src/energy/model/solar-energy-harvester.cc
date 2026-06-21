/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "solar-energy-harvester.h"

#include "energy-source.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SolarEnergyHarvester");
NS_OBJECT_ENSURE_REGISTERED (SolarEnergyHarvester);

TypeId
SolarEnergyHarvester::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SolarEnergyHarvester")
    .SetParent<EnergyHarvester> ()
    .SetGroupName ("Energy")
    .AddConstructor<SolarEnergyHarvester> ()
    .AddAttribute ("UpdateInterval",
                   "Cadence at which the harvested power is refreshed and "
                   "energy delivered to the source.",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&SolarEnergyHarvester::SetUpdateInterval,
                                     &SolarEnergyHarvester::GetUpdateInterval),
                   MakeTimeChecker (Seconds (1e-3)))
    .AddAttribute ("PeakPower",
                   "Peak harvestable power Pmax of the sin^2 envelope (W).",
                   DoubleValue (0.005),
                   MakeDoubleAccessor (&SolarEnergyHarvester::SetPeakPower,
                                       &SolarEnergyHarvester::GetPeakPower),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("DayPeriod",
                   "Period of one full sin^2 hump (one diurnal cycle).",
                   TimeValue (Seconds (3600.0)),
                   MakeTimeAccessor (&SolarEnergyHarvester::SetDayPeriod,
                                     &SolarEnergyHarvester::GetDayPeriod),
                   MakeTimeChecker (Seconds (1e-3)))
    .AddAttribute ("PhaseOffset",
                   "Initial phase offset, in fractions of one DayPeriod "
                   "(0 = sunrise, 0.25 = morning, 0.5 = noon).",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SolarEnergyHarvester::SetPhaseOffset,
                                       &SolarEnergyHarvester::GetPhaseOffset),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddTraceSource ("HarvestedPower",
                     "Instantaneous harvested power (W).",
                     MakeTraceSourceAccessor (&SolarEnergyHarvester::m_harvestedPower),
                     "ns3::TracedValueCallback::Double")
    .AddTraceSource ("TotalEnergyHarvested",
                     "Total energy harvested into the energy source (J).",
                     MakeTraceSourceAccessor (&SolarEnergyHarvester::m_totalEnergyHarvestedJ),
                     "ns3::TracedValueCallback::Double");
  return tid;
}

SolarEnergyHarvester::SolarEnergyHarvester ()
  : m_harvestedPower        (0.0),
    m_totalEnergyHarvestedJ (0.0),
    m_lastUpdate            (Seconds (0)),
    m_updateInterval        (Seconds (0.1)),
    m_peakPower             (0.005),
    m_dayPeriod             (Seconds (3600.0)),
    m_phaseOffset           (0.0)
{
  NS_LOG_FUNCTION (this);
}

SolarEnergyHarvester::~SolarEnergyHarvester () {}

void   SolarEnergyHarvester::SetUpdateInterval (Time t)   { m_updateInterval = t; }
Time   SolarEnergyHarvester::GetUpdateInterval (void) const { return m_updateInterval; }
void   SolarEnergyHarvester::SetPeakPower      (double w) { m_peakPower      = w; }
double SolarEnergyHarvester::GetPeakPower      (void) const { return m_peakPower; }
void   SolarEnergyHarvester::SetDayPeriod      (Time t)   { m_dayPeriod      = t; }
Time   SolarEnergyHarvester::GetDayPeriod      (void) const { return m_dayPeriod; }
void   SolarEnergyHarvester::SetPhaseOffset    (double p) { m_phaseOffset    = p; }
double SolarEnergyHarvester::GetPhaseOffset    (void) const { return m_phaseOffset; }

double
SolarEnergyHarvester::CurrentEnvelopeValue (void) const
{
  const double t_sec = Simulator::Now ().GetSeconds ();
  const double T     = m_dayPeriod.GetSeconds ();
  const double phase = std::fmod (t_sec / T + m_phaseOffset, 1.0);
  const double s     = std::sin (M_PI * phase);
  return m_peakPower * s * s;
}

double
SolarEnergyHarvester::DoGetPower (void) const
{
  return m_harvestedPower;
}

void
SolarEnergyHarvester::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  m_lastUpdate     = Simulator::Now ();
  m_harvestedPower = CurrentEnvelopeValue ();
  m_event = Simulator::Schedule (m_updateInterval,
                                 &SolarEnergyHarvester::UpdateHarvestedPower,
                                 this);
}

void
SolarEnergyHarvester::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_event.Cancel ();
}

void
SolarEnergyHarvester::UpdateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);
  if (Simulator::IsFinished ()) return;

  Time now      = Simulator::Now ();
  Time duration = now - m_lastUpdate;
  NS_ASSERT (duration.GetNanoSeconds () >= 0);

  // Energy delivered over the previous interval at the previously-held
  // power (zero-order hold). With a 100 ms tick and a smooth sin^2
  // envelope this approximation has sub-percent error.
  double energyHarvested = duration.GetSeconds () * m_harvestedPower;
  m_totalEnergyHarvestedJ += energyHarvested;

  // Refresh the harvested power from the analytic envelope for the
  // upcoming interval, then push to the source.
  m_harvestedPower = CurrentEnvelopeValue ();
  if (GetEnergySource () != nullptr)
    {
      GetEnergySource ()->UpdateEnergySource ();
    }

  m_lastUpdate = now;
  m_event = Simulator::Schedule (m_updateInterval,
                                 &SolarEnergyHarvester::UpdateHarvestedPower,
                                 this);
}

} // namespace ns3

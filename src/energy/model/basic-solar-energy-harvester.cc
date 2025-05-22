/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Wireless Communications and Networking Group (WCNG),
 * University of Rochester, Rochester, NY, USA.
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
 * Author: Henrique Moura <henrique.duartemoura@imec.be>
 */

#include "basic-solar-panel-patterns.h"
#include "basic-solar-energy-harvester.h"

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BasicSolarEnergyHarvester");

NS_OBJECT_ENSURE_REGISTERED (BasicSolarEnergyHarvester);

TypeId
BasicSolarEnergyHarvester::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BasicSolarEnergyHarvester")
  .SetParent<EnergyHarvester> ()
  .SetGroupName ("Energy")
  .AddConstructor<BasicSolarEnergyHarvester> ()
  .AddAttribute ("PeriodicHarvestedPowerUpdateInterval",
                 "Time between two consecutive periodic updates of the harvested power. By default, the value is updated every 1 s",
                 TimeValue (Seconds (1.0)),
                 MakeTimeAccessor (&BasicSolarEnergyHarvester::SetHarvestedPowerUpdateInterval,
                                   &BasicSolarEnergyHarvester::GetHarvestedPowerUpdateInterval),
                 MakeTimeChecker ())
  .AddAttribute ("StartSecondOfDay",
                  "Initial second of the day of the simulation.",
                  UintegerValue (0),  // 12am
                  MakeUintegerAccessor (&BasicSolarEnergyHarvester::m_startSecondOfDay),
                  MakeUintegerChecker<uint32_t> ())
  .AddTraceSource ("HarvestedPower",
                   "Harvested power by the BasicSolarEnergyHarvester.",
                   MakeTraceSourceAccessor (&BasicSolarEnergyHarvester::m_harvestedPower),
                   "ns3::TracedValueCallback::Double")
  .AddTraceSource ("TotalEnergyHarvested",
                   "Total energy harvested by the harvester.",
                   MakeTraceSourceAccessor (&BasicSolarEnergyHarvester::m_totalEnergyHarvestedJ),
                   "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

BasicSolarEnergyHarvester::BasicSolarEnergyHarvester ()
{
  NS_LOG_FUNCTION (this);

  m_daily_harvested = generate_day(SECONDS_IN_A_DAY);  // initialize the harvesting of one day
}

BasicSolarEnergyHarvester::BasicSolarEnergyHarvester (Time updateInterval)
{
  NS_LOG_FUNCTION (this << updateInterval);
  m_harvestedPowerUpdateInterval = updateInterval;
}

BasicSolarEnergyHarvester::~BasicSolarEnergyHarvester ()
{
  NS_LOG_FUNCTION (this);
}

int64_t
BasicSolarEnergyHarvester::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  return 1;
}

void
BasicSolarEnergyHarvester::SetHarvestedPowerUpdateInterval (Time updateInterval)
{
  NS_LOG_FUNCTION (this << updateInterval);
  m_harvestedPowerUpdateInterval = updateInterval;
}

Time
BasicSolarEnergyHarvester::GetHarvestedPowerUpdateInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPowerUpdateInterval;
}

/*
 * Private functions start here.
 */

void
BasicSolarEnergyHarvester::UpdateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds ()
                << "s BasicSolarEnergyHarvester(" << GetNode ()->GetId () << "): Updating harvesting power.");

  Time duration = Simulator::Now () - m_lastHarvestingUpdateTime;

  NS_ASSERT (duration.GetNanoSeconds () >= 0); // check if duration is valid

  double energyHarvested = 0.0;

  // do not update if simulation has finished
  if (Simulator::IsFinished ())
  {
    NS_LOG_DEBUG ("BasicSolarEnergyHarvester: Simulation Finished.");
    return;
  }

  m_energyHarvestingUpdateEvent.Cancel ();

  CalculateHarvestedPower ();  // make sure m_lastHarvestingUpdateTime is updated only after CalculateHarvestedPower

  energyHarvested = duration.GetSeconds () * m_harvestedPower;

  // update total energy harvested
  m_totalEnergyHarvestedJ += energyHarvested;

  // notify energy source
  GetEnergySource ()->UpdateEnergySource ();

  // update last harvesting time stamp
  m_lastHarvestingUpdateTime = Simulator::Now ();

  m_energyHarvestingUpdateEvent = Simulator::Schedule (m_harvestedPowerUpdateInterval,
                                                       &BasicSolarEnergyHarvester::UpdateHarvestedPower,
                                                       this);
}

void
BasicSolarEnergyHarvester::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);

  m_lastHarvestingUpdateTime = Simulator::Now ();

  UpdateHarvestedPower ();  // start periodic harvesting update
}

void
BasicSolarEnergyHarvester::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
}

void
BasicSolarEnergyHarvester::CalculateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);

  // TODO: to be generic, this method should handle fraction of seconds

  // obtain the energy harvested in the interval
  // need to loop on the harvested power in `m_daily_harvested` based on last update:
  //    (a) number of seconds in the day of last update
  //    (b) if interval is more than one day, get those intermediate days, obtain the total harvesting energy for each day updating `m_daily_harvested`
  //    (c) get `m_daily_harvested` for this last day, calculate the number of seconds in the last day of the interval, sum the energy harvested in the interval
  //    Usually, (b) will be zero.
  //
  m_harvestedPower = 0;
  Time now = Simulator::Now ();


  // m_startSecondOfDay shifts the start of the simulation a certain initial second
  long sec_1 = std::lround(m_lastHarvestingUpdateTime.GetSeconds () + m_startSecondOfDay) % SECONDS_IN_A_DAY;
  long sec_2 = std::lround(now.GetSeconds () + m_startSecondOfDay) % SECONDS_IN_A_DAY;

  long ndays = std::lround(now.GetSeconds () + m_startSecondOfDay) / SECONDS_IN_A_DAY -
  std::lround(m_lastHarvestingUpdateTime.GetSeconds ()) / SECONDS_IN_A_DAY;

  std::cout << "Enter CalculateHarvestedPower @ " << now.GetSeconds ()
    << " last update: " << m_lastHarvestingUpdateTime.GetSeconds ()
    << " sec_1: " << sec_1 << " sec_2: " << sec_2 << " ndays: " << ndays
    << std::endl;

  if (ndays == 0)
  {
    // same day, calculate the energy between start and end (sec_1 and sec_2)
    for (long i = sec_1; i < sec_2; i++)
    {
      m_harvestedPower += (*m_daily_harvested)[i].mean;
    }
  }
  else
  {
    // different day
    // update the rest of the first day (sec_1 until SECONDS_IN_A_DAY)
    for (long i = sec_1; i < SECONDS_IN_A_DAY; i++)
    {
      m_harvestedPower += (*m_daily_harvested)[i].mean;
    }

    // update the days in between
    for (long i = 0; i < ndays; i++)
    {
      delete m_daily_harvested;
      m_daily_harvested = generate_day(SECONDS_IN_A_DAY);  // initialize the harvesting of one day
      for(long j = 0; j < SECONDS_IN_A_DAY; j++)
      {
        m_harvestedPower += (*m_daily_harvested)[i].mean;
      }
    }
    delete m_daily_harvested;
    m_daily_harvested = generate_day(SECONDS_IN_A_DAY);  // initialize the harvesting of one day

    // update the initial seconds of the last day (from 0 to sec_2)
    m_daily_harvested = generate_day(SECONDS_IN_A_DAY);  // initialize the harvesting of one day
    for (long i = 0; i < sec_2; i++)
    {
      m_harvestedPower += (*m_daily_harvested)[i].mean;
    }
  }

  NS_LOG_DEBUG (now.GetSeconds ()
                << "s BasicSolarEnergyHarvester:Harvested energy = " << m_harvestedPower);
}

double
BasicSolarEnergyHarvester::DoGetPower (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPower;
}

} // namespace ns3

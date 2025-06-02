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
 * Author: Henrique Duarte Moura <henrique.duartemoura@imec.be>
 */

#include "trace-replay-harvester.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TraceReplayHarvester");

NS_OBJECT_ENSURE_REGISTERED (TraceReplayHarvester);

TypeId
TraceReplayHarvester::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TraceReplayHarvester")
  .SetParent<EnergyHarvester> ()
  .SetGroupName ("Energy")
  .AddConstructor<TraceReplayHarvester> ()
  .AddAttribute ("PeriodicHarvestedPowerUpdateInterval",
                 "Time between two consecutive periodic updates of the harvested power. Should match the period from the csv data.",
                 TimeValue (Seconds (1.0)),
                 MakeTimeAccessor (&TraceReplayHarvester::SetHarvestedPowerUpdateInterval,
                                   &TraceReplayHarvester::GetHarvestedPowerUpdateInterval),
                 MakeTimeChecker ())
  .AddTraceSource ("HarvestedPower",
                   "Harvested power by the TraceReplayHarvester.",
                   MakeTraceSourceAccessor (&TraceReplayHarvester::m_harvestedPower),
                   "ns3::TracedValueCallback::Double")
  .AddTraceSource ("TotalEnergyHarvested",
                   "Total energy harvested by the harvester.",
                   MakeTraceSourceAccessor (&TraceReplayHarvester::m_totalEnergyHarvestedJ),
                   "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

TraceReplayHarvester::TraceReplayHarvester ()
{
  NS_LOG_FUNCTION (this);
  m_harvestedPowerUpdateInterval = Seconds(1);
  csv_filename = "./data.csv";
}

TraceReplayHarvester::TraceReplayHarvester (std::string filename, Time updateInterval)
{
  NS_LOG_FUNCTION (this);
  m_harvestedPowerUpdateInterval = updateInterval;
  csv_filename = filename;
  readCsvData(col_num);
}

TraceReplayHarvester::~TraceReplayHarvester ()
{
  NS_LOG_FUNCTION (this);
}


bool
TraceReplayHarvester::readCsvData(int col_num){
  // Helper vars
  std::string line, colname;
  double val;

  // Create an input filestream
  std::ifstream csvFile(csv_filename);
  // Make sure the file is open
  if(!csvFile.is_open()) throw std::runtime_error("Could not open file");

  // Reads a CSV file into a vector of <double>
  m_data.clear();  // be sure to start with an empty vector


    // Read data, line by line
    while(std::getline(csvFile, line))
    {
        // Create a stringstream of the current line
        std::stringstream ss(line);

        // Keep track of the current column index
        int colIdx = 0;

        // Extract each integer
        while(ss >> val){

            if (col_num == colIdx) {
                // Add the current integer to the 'colIdx' column's values vector
                m_data.push_back(val);
                break;  // don't need to read the rest of the columns
            }

            // If the next token is a comma, ignore it and move on
            if(ss.peek() == ',') ss.ignore();

            // Increment the column index
            colIdx++;
        }
    }

    // Close file
    csvFile.close();
    NS_LOG_INFO("Read " << m_data.size() << " values from " << csv_filename << std::endl);
    return true;
}



void
TraceReplayHarvester::SetHarvestedPowerUpdateInterval (Time updateInterval)
{
  NS_LOG_FUNCTION (this << updateInterval);
  std::cout << "Cannot change! Keep update interval to " << m_harvestedPowerUpdateInterval << std::endl;
}

Time
TraceReplayHarvester::GetHarvestedPowerUpdateInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPowerUpdateInterval;
}

/*
 * Private functions start here.
 */

void
TraceReplayHarvester::UpdateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds ()
                << "s TraceReplayHarvester(" << GetNode ()->GetId () << "): Updating harvesting power.");

  Time duration = Simulator::Now () - m_lastHarvestingUpdateTime;

  NS_ASSERT (duration.GetNanoSeconds () >= 0); // check if duration is valid

  double energyHarvested = 0.0;

  // do not update if simulation has finished
  if (Simulator::IsFinished ())
  {
    NS_LOG_DEBUG ("TraceReplayHarvester: Simulation Finished.");
    return;
  }

  m_energyHarvestingUpdateEvent.Cancel ();

  CalculateHarvestedPower ();

  energyHarvested = duration.GetSeconds () * m_harvestedPower;

  // update total energy harvested
  m_totalEnergyHarvestedJ += energyHarvested;

  // notify energy source
  GetEnergySource ()->UpdateEnergySource ();

  // update last harvesting time stamp
  m_lastHarvestingUpdateTime = Simulator::Now ();

  m_energyHarvestingUpdateEvent = Simulator::Schedule (m_harvestedPowerUpdateInterval,
                                                       &TraceReplayHarvester::UpdateHarvestedPower,
                                                       this);
}

void
TraceReplayHarvester::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);

  m_lastHarvestingUpdateTime = Simulator::Now ();

  UpdateHarvestedPower ();  // start periodic harvesting update
}

void
TraceReplayHarvester::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
}

void
TraceReplayHarvester::CalculateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);

  Time now = Simulator::Now ();

  // calculate the harvested power between m_lastHarvestingUpdateTime and now

  m_harvestedPower = 0;
  long i1 = (static_cast<long>(m_lastHarvestingUpdateTime.GetSeconds() / m_harvestedPowerUpdateInterval.GetSeconds()) + m_offset) % m_data.size();
  long i2 = (static_cast<long>(now.GetSeconds() / m_harvestedPowerUpdateInterval.GetSeconds()) + m_offset) % m_data.size();

  for(long i = i1; i < i2; i++) {
    m_harvestedPower += m_data[i];
  }

  std::cout << "m_harvestedPowerUpdateInterval: " << m_harvestedPowerUpdateInterval << " m_lastHarvestingUpdateTime: " << i1 << " Now: " << i2 << std::endl;

  NS_LOG_DEBUG (Simulator::Now ().GetSeconds ()
                << "s TraceReplayHarvester:Harvested energy = " << m_harvestedPower);
}

double
TraceReplayHarvester::DoGetPower (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPower;
}

} // namespace ns3

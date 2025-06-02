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
  .AddAttribute ("Offset",
                 "Index offset of the first data in the csv file. Allows to start the trace replay from a specific index.",
                 UintegerValue (0),
                 MakeUintegerAccessor (&TraceReplayHarvester::SetOffset, &TraceReplayHarvester::GetOffset),
                 MakeUintegerChecker<uint32_t> ())
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
  csv_filename = TRACEREPLAYHARVESTER_CSV_FILENAME_DEFAULT;
  readCsvData(col_num);
}

TraceReplayHarvester::TraceReplayHarvester (std::string filename, Time updateInterval)
{
  NS_LOG_FUNCTION (this);
  m_harvestedPowerUpdateInterval = updateInterval;
  csv_filename = filename;
  readCsvData(col_num);
}

/**
 * Constructor
 *
 * \param filename filename of file which contains the csv data.
 * \param updateInterval Energy harvesting update interval.
 * \param col_num column number of the data in the csv file to be used.
 */
TraceReplayHarvester::TraceReplayHarvester (std::string filename, Time updateInterval, int col_num)
{
  NS_LOG_FUNCTION (this);
  m_harvestedPowerUpdateInterval = updateInterval;
  csv_filename = filename;
  col_num = col_num;
  readCsvData(col_num);
}

TraceReplayHarvester::~TraceReplayHarvester ()
{
  NS_LOG_FUNCTION (this);
}


/**
 * \brief Set the index offset for the data replay.
 *
 * This function sets the offset which determines the starting index
 * in the data for the trace replay. The offset is adjusted to ensure
 * it is within the bounds of the data size.
 *
 * \param new_offset The desired index offset to be set.
 */

void
TraceReplayHarvester::SetOffset(unsigned long new_offset){
  m_offset = new_offset % m_data.size();  // make sure the offset is less than the size of the data
}

unsigned long TraceReplayHarvester::GetOffset(void) const {
  return m_offset;
}

/**
 * \brief Reads a CSV file into a vector of <double>.
 * \param col_num The column number to be read.
 * \return True if the file could be read, false otherwise.
 *
 * The file is read line by line and each line is split into columns.
 * The column at the index col_num is added to the vector m_data.
 * The rest of the columns are ignored.
 * If the file could not be read, an error is thrown.
 * The method returns true if the file could be read, false otherwise.
 */
bool
TraceReplayHarvester::readCsvData(int col_num){
  // Helper vars
  std::string line, colname;
  double val;

  // Create an input filestream
  std::ifstream csvFile(csv_filename);
  // Make sure the file is open
  if(!csvFile.is_open()) throw std::runtime_error("Could not open file " + csv_filename);

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
  NS_LOG_INFO(GetHeader() <<"Read " << m_data.size() << " values from " << csv_filename << std::endl);
  return true;
}

/* ---------------------------------------------------------------------
 *
 * Private functions start here.
 *
 * ---------------------------------------------------------------------
 */


/**
 * \brief This function does not modify the harvested power update interval.
 *
 * The TraceReplayHarvester class uses the interval set at construction time
 * and ignores any attempt to change it.
 *
 * \param updateInterval Energy harvesting update interval.
 */
void
TraceReplayHarvester::SetHarvestedPowerUpdateInterval (Time updateInterval)
{
  NS_LOG_FUNCTION (this << updateInterval);
  std::cout << "Cannot change! Keep update interval to " << m_harvestedPowerUpdateInterval << std::endl;
}

/**
 * \returns The interval between each update of the harvested power.
 *
 * This function returns the interval between each update of the value of the
 * power harvested by this energy harvester.
 */
Time
TraceReplayHarvester::GetHarvestedPowerUpdateInterval (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPowerUpdateInterval;
}


/**
 * \brief Updates the harvested power and notifies the EnergySource.
 *
 * This function is scheduled periodically by the ScheduleUpdateHarvestedPower
 * function. It calculates the energy harvested since the last update, updates
 * the total energy harvested and notifies the EnergySource.
 *
 * \see ScheduleUpdateHarvestedPower
 */
void
TraceReplayHarvester::UpdateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG (GetHeader() << "Updating harvesting power at " << Simulator::Now ().GetSeconds() << ".");

  Time duration = Simulator::Now () - m_lastHarvestingUpdateTime;

  NS_ASSERT (duration.GetNanoSeconds () >= 0); // check if duration is valid

  double energyHarvested = 0.0;

  // do not update if simulation has finished
  if (Simulator::IsFinished ())
  {
    NS_LOG_DEBUG (GetHeader() << "Simulation Finished.");
    return;
  }

  m_energyHarvestingUpdateEvent.Cancel ();

  CalculateHarvestedPower ();

  energyHarvested = duration.GetSeconds () * m_harvestedPower;  // in Joules

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

/**
 * \brief Called at time t=0 to initialize the TraceReplayHarvester.
 *
 * This function is called automatically by the ns-3 simulation engine at
 * time t=0. It is used to initialize the TraceReplayHarvester,
 * and to start the periodic harvesting update.
 *
 * \see UpdateHarvestedPower
 */
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

/**
 * \brief Calculate the harvested power between the last harvesting update
 * and the current time.
 *
 * This function is called periodically by the ns-3 simulation engine to
 * update the harvested power of this energy harvester.
 *
 * \see UpdateHarvestedPower
 */
void
TraceReplayHarvester::CalculateHarvestedPower (void)
{
  NS_LOG_FUNCTION (this);

  // calculate the harvested power between m_lastHarvestingUpdateTime and now
  Time now = Simulator::Now ();
  m_harvestedPower = 0;

  // obtain the range of indices of the data to be used
  long i1 = (static_cast<long>(m_lastHarvestingUpdateTime.GetSeconds() / m_harvestedPowerUpdateInterval.GetSeconds()) + m_offset) % m_data.size();
  long i2 = (static_cast<long>(now.GetSeconds() / m_harvestedPowerUpdateInterval.GetSeconds()) + m_offset) % m_data.size();

  for(long i = i1; i < i2; i++) {
    m_harvestedPower += m_data[i];
  }

  NS_LOG_DEBUG(GetHeader() <<
      "Data range: " << i1 << ", " << i2 << " : Harvester power: " << m_harvestedPower << " J at " << now.GetSeconds() << "s");

}

/**
 * \returns The current harvested power of this energy harvester.
 *
 * This function is pure virtual and must be implemented by each
 * derived class. It is called by EnergyHarvester::GetPower to
 * obtain the current harvested power of this energy harvester.
 *
 * \see EnergyHarvester::GetPower
 */
double
TraceReplayHarvester::DoGetPower (void) const
{
  NS_LOG_FUNCTION (this);
  return m_harvestedPower;
}


/**
 * \returns A string to be used as a prefix for output messages.
 *
 * This function is typically used by the logging functions to provide a
 * context for the message being logged.
 *
 * \see Object::GetHeader
 */
std::string TraceReplayHarvester::GetHeader(void) const {
  std::ostringstream msg;
  msg << "TraceReplayHarvester("<< (GetNode () ? GetNode ()->GetId () : 0) << "): ";
  return msg.str();
}


} // namespace ns3

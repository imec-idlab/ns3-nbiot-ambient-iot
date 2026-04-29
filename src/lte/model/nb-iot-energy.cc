/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Communication Networks Institute at TU Dortmund University
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
 * Author: Tim Gebauer <tim.gebauer@tu-dortmund.de>
 */
#include <fstream>


#include "nb-iot-energy.h"
namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (NbiotEnergyModel);

void NbiotChip::SetDownlinkPower(double power){
    m_downlinkPower = power;
}
void NbiotChip::SetUplinkPower(double power){
    m_uplinkPower = power;
}
void NbiotChip::SetEdrxPower(double power){
    m_edrxPower = power;
}
void NbiotChip::SetDrxPower(double power){
    m_drxPower = power;
}
void NbiotChip::SetPsmPower(double power){
    m_psmPower = power;
}
void NbiotChip::SetIdlePower(double power){
    m_idlePower = power;
}
double NbiotChip::GetDownlinkPower(){
    return m_downlinkPower;
}
double NbiotChip::GetUplinkPower(){
    return m_uplinkPower;
}
double NbiotChip::GetEdrxPower(){
    return m_edrxPower;
}
double NbiotChip::GetDrxPower(){
    return m_drxPower;
}
double NbiotChip::GetPsmPower(){
    return m_psmPower;
}
double NbiotChip::GetIdlePower(){
    return m_idlePower;
}
NbiotEnergyModel::~NbiotEnergyModel(){
    DoNotifyStateChange(PowerState::OFF);
}

NbiotEnergyModel::PowerState
NbiotEnergyModel::DoGetState(){
    return m_lastState;
}

void NbiotEnergyModel::SetImsi(uint32_t imsi){
    m_imsi = imsi;
}

void NbiotEnergyModel::EnableLogging(){
  m_logging = true;
}

void NbiotEnergyModel::SetLogDir(std::string dirname){
  m_logdir = dirname;
}

void NbiotEnergyModel::SetModule(NbiotChip module) { m_module = module; }

void NbiotEnergyModel::DoNotifyStateChange(PowerState newState){
    Time stateTime = (Simulator::Now()-m_lastStateChange); // subframes are [ms] and we need [s]
    double lostEnergy = 0; // Energy in [Ws] or [J]
    m_states.push_back(std::pair<PowerState,uint32_t>(newState,Simulator::Now().GetMilliSeconds()));
    switch (m_lastState)
    {
    case PowerState::RRC_CONNECTED_IDLE:
        lostEnergy = m_module.GetIdlePower()*stateTime.GetSeconds(); // Energy in [Ws] or [J]
        break;
    case PowerState::RRC_CONNECTED_RECEIVING_NPDCCH:
    case PowerState::RRC_CONNECTED_RECEIVING_NPDSCH:
        lostEnergy = m_module.GetDownlinkPower()*stateTime.GetSeconds();
        break;
    case PowerState::RRC_CONNECTED_SENDING_NPRACH:
    case PowerState::RRC_CONNECTED_SENDING_NPUSCH:
    case PowerState::RRC_CONNECTED_SENDING_NPUSCH_F2:
        lostEnergy = m_module.GetUplinkPower()*stateTime.GetSeconds();
        break;
    case PowerState::RRC_SUSPENDED_EDRX:
        lostEnergy = m_module.GetEdrxPower()*stateTime.GetSeconds();
        break;
    case PowerState::RRC_SUSPENDED_DRX:
        lostEnergy = m_module.GetDrxPower()*stateTime.GetSeconds();
        break;
    case PowerState::RRC_SUSPENDED_PSM:
        lostEnergy = m_module.GetPsmPower()*stateTime.GetSeconds();
    default:
        break;
    }

    // Log the energy comsumption per state
    if (m_logging){
        std::ofstream logfile;
        logfile.open(m_logdir + "nbiot_energy.log", std::ios_base::app);
        logfile << Simulator::Now().GetMilliSeconds() << ", " << m_imsi << ", " << PowerStateToString( m_lastState )<< ", " << lostEnergy << std::endl;
        logfile.close();
    }

    m_lastStateChange = Simulator::Now();

    // Brown-out gate. While browned out we do not drain the battery and do
    // not accumulate dwell time toward the duty-cycle numerator/denominator —
    // mirrors a real chip whose regulator has cut the modem off. The harvester
    // continues to charge the cap; once GetRemainingEnergy() rises above the
    // recovery threshold we exit brown-out and resume normal accounting on
    // the *next* state transition.
    if (m_brownedOut)
    {
        if (m_battery->GetRemainingEnergy() >= m_recoveryEnergyJ
            && m_recoveryEnergyJ > 0.0)
        {
            m_brownedOut = false;
            if (!m_brownoutCb.IsNull()) m_brownoutCb(m_imsi, false);
        }
        // While browned out, don't drain and don't accumulate dwell time.
        // We still advance m_lastStateChange so post-recovery timing is clean.
        m_lastState = newState;
        return;
    }

    // Skip the battery call if there was no time in the previous state — some
    // EnergySource implementations (GenericCapacitor) divide by duration in
    // DecreaseRemainingEnergy and produce NaN when it's zero.
    if (lostEnergy > 0.0 && stateTime.GetSeconds() > 0.0)
    {
        m_battery->DecreaseRemainingEnergy(lostEnergy);
        if (!m_depleted && m_battery->GetRemainingEnergy() <= 0.0)
        {
            m_depleted = true;
            m_depletionTime = Simulator::Now();
        }
    }
    m_timeSpendInState[m_lastState] += stateTime.GetMilliSeconds();
    m_energySpendInState[m_lastState] += lostEnergy;

    // Brown-out trigger: this drain may have pushed us below the threshold.
    if (m_brownoutEnergyJ > 0.0
        && m_battery->GetRemainingEnergy() <= m_brownoutEnergyJ)
    {
        m_brownedOut = true;
        ++m_brownoutCount;
        if (!m_brownoutCb.IsNull()) m_brownoutCb(m_imsi, true);
    }

    m_lastState = newState;
}

void NbiotEnergyModel::SetBrownoutThresholds(double brownoutJ, double recoveryJ){
    m_brownoutEnergyJ = brownoutJ;
    m_recoveryEnergyJ = recoveryJ;
}

void NbiotEnergyModel::SetBrownoutCallback(BrownoutCb cb){
    m_brownoutCb = cb;
}

void NbiotEnergyModel::PollBrownoutRecovery(){
    // Mirrors the recovery branch inside DoNotifyStateChange but without
    // requiring an LTE state change. Needed for RA mode where the UE parks in
    // PSM during brown-out and the event-driven path stops firing entirely.
    if (m_brownedOut
        && m_recoveryEnergyJ > 0.0
        && m_battery
        && m_battery->GetRemainingEnergy() >= m_recoveryEnergyJ)
    {
        m_brownedOut = false;
        if (!m_brownoutCb.IsNull()) m_brownoutCb(m_imsi, false);
    }
}

double NbiotEnergyModel::GetEnergyRemaining(){
    // Update Power when reading
    DoNotifyStateChange(m_lastState);
    return m_battery->GetRemainingEnergy();
}

/**
 * Return the fraction of energy remaining in the device.
 *
 * \return the fraction of energy remaining in the device.
 */
double NbiotEnergyModel::GetEnergyRemainingFraction(){
    // Update Power when reading
    DoNotifyStateChange(m_lastState);
    return m_battery->GetEnergyFraction();
}

/**
 * Set the energy source for this device.
 *
 * \param new_battery the new energy source.
 */
void NbiotEnergyModel::SetEnergySource(Ptr<EnergySource> new_battery){
    m_battery = nullptr;  // to force deallocation
    m_battery = new_battery;
}

void NbiotEnergyModel::FlushStateTime() {
    Time stateTime = Simulator::Now() - m_lastStateChange;
    if (stateTime.GetMilliSeconds() > 0) {
        m_timeSpendInState[m_lastState] += stateTime.GetMilliSeconds();
        m_lastStateChange = Simulator::Now();
    }
}

double NbiotEnergyModel::GetActiveTimeMs() const {
    double t = 0.0;
    for (const auto& kv : m_timeSpendInState) {
        switch (kv.first) {
            case PowerState::RRC_CONNECTED_RECEIVING_NPDCCH:
            case PowerState::RRC_CONNECTED_RECEIVING_NPDSCH:
            case PowerState::RRC_CONNECTED_SENDING_NPRACH:
            case PowerState::RRC_CONNECTED_SENDING_NPUSCH:
            case PowerState::RRC_CONNECTED_SENDING_NPUSCH_F2:
                t += kv.second;
                break;
            default:
                break;
        }
    }
    return t;
}

double NbiotEnergyModel::GetTotalAccountedTimeMs() const {
    double t = 0.0;
    for (const auto& kv : m_timeSpendInState) {
        t += kv.second;
    }
    return t;
}

double NbiotEnergyModel::GetDutyCycle() const {
    double total = GetTotalAccountedTimeMs();
    return total > 0 ? GetActiveTimeMs() / total : 0.0;
}


std::string NbiotEnergyModel::PowerStateToString(PowerState state) {
  switch (state)
  {
    case PowerState::RRC_CONNECTED_IDLE:
      return "RRC_CONNECTED_IDLE";
    case PowerState::RRC_CONNECTED_RECEIVING_NPDSCH:
      return "RRC_CONNECTED_RECEIVING_NPDSCH";
    case PowerState::RRC_CONNECTED_RECEIVING_NPDCCH:
      return "RRC_CONNECTED_RECEIVING_NPDCCH";
    case PowerState::RRC_CONNECTED_SENDING_NPRACH:
      return "RRC_CONNECTED_SENDING_NPRACH";
    case PowerState::RRC_CONNECTED_SENDING_NPUSCH:
      return "RRC_CONNECTED_SENDING_NPUSCH";
    case PowerState::RRC_CONNECTED_SENDING_NPUSCH_F2:
      return "RRC_CONNECTED_SENDING_NPUSCH_F2";
    case PowerState::RRC_SUSPENDED_DRX:
      return "RRC_SUSPENDED_DRX";
    case PowerState::RRC_SUSPENDED_EDRX:
      return "RRC_SUSPENDED_EDRX";
    case PowerState::RRC_SUSPENDED_PSM:
      return "RRC_SUSPENDED_PSM";
    case PowerState::OFF:
      return "OFF";
    default:
      return "UNKNOWN";
  }
}

}

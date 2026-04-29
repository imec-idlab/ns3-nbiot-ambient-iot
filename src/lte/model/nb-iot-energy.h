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


#ifndef NBIOT_ENERGY_H
#define NBIOT_ENERGY_H

#include <ns3/object.h>
#include <ns3/simulator.h>
#include <ns3/li-ion-energy-source.h>
#include <cmath>

namespace ns3 {

class NbiotChip {
protected:
    double m_psmPower;
    double m_drxPower;
    double m_edrxPower;
    double m_uplinkPower;
    double m_downlinkPower;
    double m_idlePower;

public:
    NbiotChip():
        m_psmPower(0),
        m_drxPower(0),
        m_edrxPower(0),
        m_uplinkPower(0),
        m_downlinkPower(0),
        m_idlePower(0){}
    NbiotChip(double psmPower, double drxPower, double edrxPower, double uplinkPower, double downlinkPower, double idlePower):
        m_psmPower(psmPower),
        m_drxPower(drxPower),
        m_edrxPower(edrxPower),
        m_uplinkPower(uplinkPower),
        m_downlinkPower(downlinkPower),
        m_idlePower(idlePower){}
    ~NbiotChip(){}

    void SetPsmPower(double Power);
    void SetDrxPower(double Power);
    void SetEdrxPower(double Power);
    void SetUplinkPower(double Power);
    void SetDownlinkPower(double Power);
    void SetIdlePower(double Power);

    double GetPsmPower();
    double GetDrxPower();
    double GetEdrxPower();
    double GetUplinkPower();
    double GetDownlinkPower();
    double GetIdlePower();
};

class BG96 : public NbiotChip{
public:
    BG96(){
        // Compare Joerke nbiot-nidd-ciotopt
        m_psmPower = 3.8* 3.9*std::pow(10,-6);
        m_drxPower = 3.8* 1.56*std::pow(10,-3);
        m_edrxPower = 3.8* 0.81*std::pow(10,-3);
        m_uplinkPower = 3.8* 155*std::pow(10,-3);
        m_downlinkPower = 80*std::pow(10,-3);
        m_idlePower = 3.8 * 0.81*std::pow(10,-3);
    };
};

class NbiotEnergyModel : public Object
{
public:
    enum class PowerState{
        RRC_CONNECTED_IDLE,
        RRC_CONNECTED_RECEIVING_NPDSCH,
        RRC_CONNECTED_RECEIVING_NPDCCH,
        RRC_CONNECTED_SENDING_NPRACH,
        RRC_CONNECTED_SENDING_NPUSCH,
        RRC_CONNECTED_SENDING_NPUSCH_F2,
        RRC_SUSPENDED_DRX,
        RRC_SUSPENDED_EDRX,
        RRC_SUSPENDED_PSM,
        OFF
    }powerState;

    NbiotEnergyModel(NbiotChip module, uint32_t imsi):
        m_module(module), m_imsi(imsi), m_lastState(PowerState::OFF), m_lastStateChange(Time(0))
        {
        m_battery = CreateObject<LiIonEnergySource>();
        m_battery->SetInitialEnergy(18000.0);
        }
    ~NbiotEnergyModel();


    void DoNotifyStateChange(PowerState newState);
    PowerState DoGetState();
    double GetEnergyRemaining();
    double GetEnergyRemainingFraction();
    void SetImsi(uint32_t imsi);

    void SetEnergySource(Ptr<EnergySource> new_battery);
    Ptr<EnergySource> GetEnergySource() const { return m_battery; }

    void DecreaseRemainingEnergy(double lostEnergy);

    bool IsDepleted() const { return m_depleted; }
    Time GetDepletionTime() const { return m_depletionTime; }

    // Duty-cycle: sum of NPDCCH/NPDSCH/NPUSCH/NPUSCH_F2/NPRACH dwell time over
    // total accounted time. FlushStateTime() folds the current state's elapsed
    // time into m_timeSpendInState WITHOUT touching the battery (avoids
    // zero-duration divides on periodic energy sources). Call it before reading
    // GetDutyCycle() at simulation end.
    void   FlushStateTime();
    double GetActiveTimeMs() const;
    double GetTotalAccountedTimeMs() const;
    double GetDutyCycle() const;

    // Brown-out gating: when remaining energy drops below m_brownoutEnergyJ the
    // model freezes — DoNotifyStateChange records timing for completeness but
    // does not drain the battery and does not accumulate active-state time.
    // Hysteresis: model stays browned out until energy rises above
    // m_recoveryEnergyJ. Disabled by default (thresholds 0).
    void   SetBrownoutThresholds(double brownoutJ, double recoveryJ);
    bool   IsBrownedOut() const { return m_brownedOut; }
    uint32_t GetBrownoutCount() const { return m_brownoutCount; }
    typedef Callback<void, uint32_t /*imsi*/, bool /*entering*/> BrownoutCb;
    void   SetBrownoutCallback(BrownoutCb cb);
    // Standalone recovery probe: must be called periodically from outside
    // (e.g. the per-UE polling task in the scenario). The brown-out gate's
    // recovery check normally rides on LTE state-change events, but during
    // brown-out the LTE stack often parks in PSM with no transitions, which
    // would deadlock recovery detection. This method runs the same check
    // without requiring a state change.
    void   PollBrownoutRecovery();

    void EnableLogging();
    void SetLogDir(std::string dirname);
    void SetModule(NbiotChip module);
    std::string PowerStateToString(PowerState state);
private:
    Ptr<EnergySource> m_battery; // Battery model
    NbiotChip m_module; // Current Nbiot Module
    uint32_t m_imsi;
    PowerState m_lastState; // Current Powerstate
    Time m_lastStateChange;
    std::map<PowerState, double> m_timeSpendInState; // Statistics
    std::map<PowerState, double> m_energySpendInState; // Statistics
    std::vector<std::pair<PowerState,uint32_t>> m_states;
    std::string m_logdir;
    bool m_logging = false;
    bool m_depleted = false;
    Time m_depletionTime = Time::Max();

    // Brown-out gate state
    double  m_brownoutEnergyJ = 0.0;     // freeze threshold (in J)
    double  m_recoveryEnergyJ = 0.0;     // resume threshold (J), > brownoutJ
    bool    m_brownedOut      = false;
    uint32_t m_brownoutCount  = 0;       // number of brown-out entries
    BrownoutCb m_brownoutCb;             // optional, fires on entry/exit
};
}
#endif /* FF_MAC_SCHEDULER_H */

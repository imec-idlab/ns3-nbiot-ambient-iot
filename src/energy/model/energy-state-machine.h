#ifndef __ENERGY_STATE_MACHINE_H__
#define __ENERGY_STATE_MACHINE_H__

enum class State { ACTIVE, INACTIVE };


class EnergyStateMachine {
private:
    double delta0;  // transition probability from ACTIVE to INACTIVE
    double delta1;  // transition probability from INACTIVE to ACTIVE
    std::mt19937 generator;
    State state;
    std::uniform_real_distribution<double> distribution;

public:

    EnergyStateMachine(double d0, double d1);
    State current_state() const;
    void set_state(State new_state);
    State step();

};


#endif // __ENERGY_STATE_MACHINE_H__
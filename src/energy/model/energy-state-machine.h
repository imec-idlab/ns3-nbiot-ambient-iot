#ifndef __ENERGY_STATE_MACHINE_H__
#define __ENERGY_STATE_MACHINE_H__

enum class State { INACTIVE, ACTIVE };  // keep this order, so ACTIVE = 1


class EnergyStateMachine {
private:
    double delta0;  // transition probability from INACTIVE to ACTIVE
    double delta1;  // transition probability from ACTIVE to INACTIVE
    std::mt19937 generator;
    State state;
    std::uniform_real_distribution<double> distribution;

public:

    EnergyStateMachine(double d0, double d1);
    State current_state() const;
    void set_state(State new_state);
    State step();
    void set_delta0(double d0);
    void set_delta1(double d1);
};


#endif // __ENERGY_STATE_MACHINE_H__
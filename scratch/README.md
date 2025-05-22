# Summary of the code in this scratch folder



> For all scenarios, 3*X minutes of simulation time are simulated, but only the intermediate X minutes are evaluated. The first X minutes produce no significant results since devices at the beginning are scheduled in an empty cell and experience very good transmission conditions.
> After X minutes, new devices will find ongoing transmissions of previous devices, which enables a more realistic situation and produces significant results. Since devices that have started transmissions within the intermediate X minutes of the simulation may not complete their transmissions in this intermediate time slot, additional X minutes are simulated with more new transmissions to keep the channels busy and let the intermediate devices complete their transmissions.


# [nb-scenario1.cc](nb-scenario1.cc)

This code simulates a simple IoT scenario with NB-IoT devices (default is one. see `num_ues`) connected to a base station. The devices are equipped with a Li-ion energy source (`LiIonEnergySource`) with no harvester.
The devices transmit packets according to a Poisson distribution.
The code measures the energy consumption of the devices and the packet loss ratio.

```bash
cd ns3-nbiot
./waf --run nb-scenario1.cc
```


# [nb-scenario2.cc](nb-scenario2.cc)

This code is a sample simulation script for LTE+EPC using the ns-3 simulator.
It uses a similar scenario than `nb-scenario1.cc`.
However, the UE devices are equipped with a capacitor energy source (`GenericCapacitor`).
The capacitor is charged by a harvester.
Current it uses `BasicEnergyHarvester`, which is a random harvester generator.

> TODO: implement a solar panel harvester.

```bash
cd ns3-nbiot
./waf --run nb-scenario2.cc
```
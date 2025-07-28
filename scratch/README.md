# Summary of the code in this scratch folder


> For all scenarios, 3*X minutes of simulation time are simulated, but only the intermediate X minutes are evaluated. The first X minutes produce no significant results since devices at the beginning are scheduled in an empty cell and experience very good transmission conditions.
> After X minutes, new devices will find ongoing transmissions of previous devices, which enables a more realistic situation and produces significant results. Since devices that have started transmissions within the intermediate X minutes of the simulation may not complete their transmissions in this intermediate time slot, additional X minutes are simulated with more new transmissions to keep the channels busy and let the intermediate devices complete their transmissions.


## Board

The default board is ['BG96'](https://www.quectel.com/content/uploads/2025/03/Quectel_Product_Brochure_V8.3.pdf).
The energy chip (class `NbiotChip`) is defined in `src/lte/model/nb-iot-energy.h`.

This board is evaluate by [Khan et al.](http://dx.doi.org/10.36227/techrxiv.12738725.v1).



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
./waf --run "nb-scenario2 \
    --harvesterType=0 \
    --ns3::BasicEnergyHarvester::PeriodicHarvestedPowerUpdateInterval=1ms"
```

The command above selects the BasicEnergyHarvester (type 0) and assigns its update interval to 1 ms.


# [nb-scenario3.cc](nb-scenario3.cc)

This scenario creates a BS and N UE around in a circle of radius C (coverage).
It is used to simulated the environment where the energy supplied to the device is controlled by a two-state Markov chain.

```bash
cd ns3-nbiot
./waf --run "nb-scenario3 \
    --num_ues=10 \
    --coverage=100 \
    --ns3::EnergyMarkov::Delta0=0.9 \
    --ns3::EnergyMarkov::Delta1=0.15 \
    --ns3::LteEnbMac::NumberOfRaPreambles=10 \
    --ns3::EnergyMarkov::LogDir=./logs/markov" 2>&1 | tee nb-scenario3.log
```

# [nb-scenario4.cc](nb-scenario4.cc)

This scenario creates a BS and N UE around in a circle of radius C (coverage).
It is used to simulated the environment described in Moons et al. (2024).
The devices are feed by an unlimited energy source.
The traffic model of each device is defined by a two-state Markov chain as shown below:

<img src="markov-chain.png" alt="Markov Chain" width="400">

- The steady state probability of the active state $p$ is:

$p = \frac{\delta_1}{\delta_0 + \delta_1}$


- RU duration $T_{RU} = 8 ms$.
- the time slot duration $T_s$ can be calculated as follows:

$T_s = N_{rep} × N_{RU} × T_{RU}$

where $N_{rep}$ is the number of repetitions, and $N_{RU}$ is the number of resource units.

- consider an uplink bandwidth of 180 kHz containing 12 subcarriers with a subcarrier spacing of 15 kHz. 6 subcarriers for each channel: NPUSCH and NPRACH.
- TBS of 72 bits
- scheduler assigns 1 RU and 2 repetitions per transmission, which results in a time slot duration of $T_s = 16 ms$.
- we consider QPSK modulation, two receive antennas for the base station and a Rayleigh fading channel.
- The noise in this channel is characterized by Additive White Gaussian Noise (AWGN).

- Number of UEs: variable (from 1 to 640)


```bash
cd ns3-nbiot
./waf --run "nb-scenario4 \
    --simTime=180 \
    --num_ues=10 \
    --coverage=100 \
    --ns3::MarkovUdpClient::PacketSize=512 \
    --ns3::MarkovUdpClient::MaxPackets=10000 \
    --ns3::LteEnbMac::NumberOfRaPreambles=10 \
    --ns3::MarkovUdpClient::ActiveInterval=10 \
    --ns3::MarkovUdpClient::InactiveInterval=10
    --ns3::MarkovUdpClient::TransitionProbabilityInactiveToActive=0.7 \
    --ns3::MarkovUdpClient::TransitionProbabilityActiveToInactive=0.2" 2>&1 | tee nb-scenario3.log
```

**Reference**:

- Moons, L., Nasser, S., Sabovic, A., Singh, R.K. and Famaey, J., 2024, October. Evaluating Fast and Grant-Free Uplink Access in Next-Generation Cellular IoT Networks. In 2024 3rd International Conference on 6G Networking (6GNet) (pp. 19-24). IEEE.


### Comments

- The `NbiotEnergyModel` object is instantiated in lte-ue-rrc.cc.
You can use `Ptr<LteUeRrc> ueRrc = ueLteDevice->GetRrc();` to access the energy model or create a new one if needed in your scenario.

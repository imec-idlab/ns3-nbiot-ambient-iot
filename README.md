# NB-IoT for Ambient IoT Networks

This fork extends [Lena-NB](https://github.com/tudo-cni/ns3-lena-nb) into a system-level evaluation platform for **energy-harvesting ("ambient") IoT devices** on NB-IoT. It adds, among other features, an energy front-end, solar harvesting with a diurnal profile, supercapacitor storage, and a hysteretic brown-out model that suspends and recovers devices with their connection context retained, with per-radio-state energy accounting based on measured the Quectel BG96 power draws. On the protocol side, it implements and compares five uplink access schemes: contention-based random access (with Rel-15 Early Data Transmission), a contention-free Scheduling Request (SR) on reserved NPRACH resources used as a resume trigger from PSM, a hybrid SR combining reserved and opportunistic contention-based requests, a predictive Fast Uplink Grant (FUG) scheme, and an oracle-scheduled idealized FUG bound. A Markov-modulated traffic generator, sweep tooling (GNU parallel), and analysis scripts for energy-per-bit, delay, and brown-out metrics complete the pipeline.

The original README file moved to [here](README_ORIGINAL.md).


## Configure and Build

**Requires Python 3.11 or lower**

- Follow the instructions on [ns-3 Installation Guide](https://www.nsnam.org/wiki/Installation) to prepare all dependencies.
- Clone the project
- Change directory into the ns3-nbiot-ambient-iot/ directory
- Configure with
```shell
/waf configure --disable-static --disable-gtk --disable-test --disable-examples --cxx-standard=17
```
- Build with
```shell
./waf build
```

## Reproduce results of the paper

- Parallel execution using GNU-Parallel
  - Change directory into output/gnu-parallel/
  - Run with
    ```shell
    ./wrapper.sh params/nbiot_input_ambient.csv
    ```
This runs the simulation in `scratch/nb-iot-ambient6g/nb-iot-ambient6g.cc`. Explanations of the parameters can be found in the said file.
At the end of the simulation, data is outputed in the output directory.
- To plot, run (more recent python version is preferable here)
```shell
python plotter.py
```
With the figures saved in the fig directory

# Ambient-6G

This repository was forked from [Lena-NB](https://github.com/tudo-cni/ns3-lena-nb).
The original README file can be found [here](README_ORIGINAL.md).
It is based on NS3 version 3.32 (needed for Lena-NB).
Some minor changes (error corrections) were made to the original code to make it run.

The changes focus on experimenting with different configurations of the NB-IoT network, particularly in the context of Ambient 6G research.
The main contributions include:
- Creation of a [capacitor source model](src/energy/model/generic-capacitor.cc) to simulate energy harvesting from ambient sources.
- Implementation of some [energy harvesting models](src/energy/model) to simulate energy harvesting from solar panels or replay CSV traces.
- Implementation of a [Markov UDP client](src/markov-traffic/model/README.md) that can switch between active and inactive states, allowing for the traffic patterns proposed in Moons et al. (2024).

Look at [scratch/README.md](scratch/README.md) for more information on how to run the simulations. There are some examples of how to use the capacitor source model and the energy harvesting models in this folder.
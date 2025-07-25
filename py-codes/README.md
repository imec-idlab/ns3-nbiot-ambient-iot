# README


This folder contains Python code for various tasks and projects added in this fork by the user.
They don't belong to the original repository.

There are several scripts for analyzing network simulation data, particularly focusing on spectral uplink usage, collision events, MAC layer events, and state changes of UDP clients in a network simulation.
<pre>
py-codes/
├── check_energy.py
├── check_spectral_downlink.py
├── check_spectral_uplink.py
├── collisions.py
├── mac_events.py
├── state_changes.py
└── plot_all.py
</pre>

Use the `plot_all.py` script to process and visualize data from network simulation logs. It automatically finds relevant log files in a specified directory and generates plots for spectral uplink usage, collision events, MAC layer events, and state changes of UDP clients.

```bash
python plot_all.py --dir /path/to/logs [--show]
```

`--show` option will display the plots interactively; if omitted, the plots will be saved as PNG files (the default behavior).


# State Changes

`nb-scenario4.cc` generates a log file that records time-stamped transitions between INACTIVE and ACTIVE states for Markov-based UDP clients across multiple nodes in a network simulation. Each entry includes the simulation time, the node identifier, and the change in state, allowing detailed analysis of temporal traffic dynamics.

`state_changes.py` processes this log file to extract and analyze the state changes, providing insights into the behavior of the UDP clients over time. The script reads the log file, identifies transitions between states, and can be used to visualize or summarize the state change patterns across different nodes in the simulation.

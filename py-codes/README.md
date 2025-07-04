# README


This folder contains Python code for various tasks and projects added in this fork by the user.
They don't belong to the original repository.


# State Changes

`nb-scenario4.cc` generates a log file that records time-stamped transitions between INACTIVE and ACTIVE states for Markov-based UDP clients across multiple nodes in a network simulation. Each entry includes the simulation time, the node identifier, and the change in state, allowing detailed analysis of temporal traffic dynamics.
`state_changes.py` processes this log file to extract and analyze the state changes, providing insights into the behavior of the UDP clients over time. The script reads the log file, identifies transitions between states, and can be used to visualize or summarize the state change patterns across different nodes in the simulation.

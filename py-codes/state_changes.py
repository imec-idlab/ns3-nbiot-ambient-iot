import argparse
import os
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

from utilities import find_files


def process_state_changes_file(file_path: str) -> pd.DataFrame:
    """
    Reads a state change log file and converts it into a pandas DataFrame.

    Args:
        file_path (str): The path to the state change log file to read.

    Returns:
        pd.DataFrame: A DataFrame containing the data from the file. It has the following columns:
            - Time (float): The time in seconds
            - Node (int): The node identifier
            - State (int): The new state
    """
    pattern = re.compile(
        r"(?P<time>\d+\.\d+)s\s+\[/NodeList/(?P<node>\d+)/ApplicationList/\d+/State\]\s+State changed from (?P<old>\d+) to (?P<new>\d+)"
    )

    data = []

    # Read and parse the file
    with open(file_path, "r") as f:
        for line in f:
            match = pattern.search(line)
            if match:
                time = float(match.group("time"))
                node = int(match.group("node"))
                new_state = int(match.group("new"))
                data.append((time, node, new_state))

    # Create a DataFrame
    df = pd.DataFrame(data, columns=["Time", "Node", "State"])

    # Find the final time in the entire dataset
    max_time = df["Time"].max()

    # For each node, get the last entry
    last_entries = df.groupby("Node").tail(1)

    # Filter nodes that end before max_time
    need_extension = last_entries[last_entries["Time"] < max_time].copy()
    need_extension["Time"] = max_time  # Set to final time

    # Append these extension rows to the original dataframe
    df_extended = pd.concat([df, need_extension], ignore_index=True).sort_values(by=["Node", "Time"])

    return df_extended


def plot_state_changes(filename, show: bool = False):
    """
    Process a state change log file and plot the state transitions for each node over time.

    :param filename: path to the state change log file
    :param show: whether to show the plot instead of saving it (default: False)
    """
    df = process_state_changes_file(filename)

    # Sort values for proper plotting
    df.sort_values(by=["Node", "Time"], inplace=True)

    # Get all node IDs
    node_ids = sorted(df["Node"].unique())
    num_nodes = len(node_ids)

    # Set up subplots
    fig, axes = plt.subplots(nrows=num_nodes, ncols=1, figsize=(10, 2.5 * num_nodes), sharex=True)

    if num_nodes == 1:
        axes = [axes]  # Ensure it's iterable

    for ax, node_id in zip(axes, node_ids):
        node_df = df[df["Node"] == node_id]
        ax.step(node_df["Time"], node_df["State"], where="post")
        ax.set_yticks([0, 1])
        ax.set_yticklabels(["INACTIVE", "ACTIVE"])
        ax.set_ylabel(f"Node {node_id}")
        ax.grid(True, linestyle="--", linewidth=0.5)

    axes[-1].set_xlabel("Time (s)")
    fig.suptitle("Markov State Transitions per Node", fontsize=14)
    plt.tight_layout(rect=[0, 0, 1, 0.97])
    if show:
        plt.show()
    else:
        plt.savefig(filename.replace('.log', '.png'))

    plt.close()


def compute_transition_probs(group):
    """
    Computes transition probabilities from a group of state transitions.

    Parameters
    ----------
    group : pd.DataFrame
        The group of state transitions. It should have columns 'Node', 'Time', 'State', and 'PrevState'.

    Returns
    -------
    pd.Series
        A Series containing the transition probabilities 'Δ₁ (active→inactive)' and 'Δ₀ (inactive→active)'.

    Notes
    -----
    This function assumes that the sampling interval is constant and that there are enough data points to find at least one transition that represents the sampling interval.
    """
    trans_counts = group['Transition'].value_counts()
    prev_counts = group['PrevState'].value_counts()

    delta1 = trans_counts.get('1.0->0.0', 0) / prev_counts.get(1, 0) if prev_counts.get(1, 0) > 0 else 0
    delta0 = trans_counts.get('0.0->1.0', 0) / prev_counts.get(0, 0) if prev_counts.get(0, 0) > 0 else 0

    return pd.Series({'δ₀ (inactive→active)': delta0, 'δ₁ (active→inactive)': delta1, })


def compute_transition_probabilities(filename, show: bool = False):
    df = process_state_changes_file(filename)

    # We are considering the there are enough data points to find at least one transition
    # that represents the sampling interval (i.e, the minimum time difference between two consecutive state changes)
    sampling_interval = np.round(df.groupby("Node")["Time"].diff().dropna().min())

    # Generate full time index per node (10s intervals)
    full_df = []
    for node, group in df.groupby('Node'):
        time_min, time_max = group['Time'].min(), group['Time'].max()
        time_range = pd.DataFrame({'Time': np.arange(time_min, time_max + sampling_interval, sampling_interval)})
        time_range['Node'] = node
        merged = pd.merge(time_range, group, on=['Time', 'Node'], how='left')
        merged['State'] = merged['State'].ffill()  # fill missing states forward
        full_df.append(merged)

    df_full = pd.concat(full_df).sort_values(by=['Node', 'Time'])

    # Find the previous state for each transition
    df_full['PrevState'] = df_full.groupby('Node')['State'].shift(1)
    # Identify transition type 1.0 -> 0.0 or 0.0 -> 1.0
    df_full['Transition'] = df_full['PrevState'].astype(str) + '->' + df_full['State'].astype(str)

    transition_probs = df_full.groupby('Node').apply(compute_transition_probs, include_groups=False).reset_index()

    # transition_probs = df_full.groupby('Node').apply(lambda g: compute_transition_probs(g.drop(columns=['Node']))).reset_index()

    print(transition_probs)



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="State Changes")
    parser.add_argument("options", type=str, default="d", choices=['d', 'f', 'c'],
                        help="Select which operation to perform: d: search files, f: process a file, c: compute transition probabilities from a log file")
    parser.add_argument("--dir", type=str, default=None, help="Folder to search for state change files")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot")
    args = parser.parse_args()

    # Validate combinedarguments
    if args.options == "d" and args.dir is None:
        print("Use --dir to search for files.")
        parser.print_help()
        exit(1)

    elif args.options in ["f", "c"] and args.fname is None:
        print("Use --fname to process a specific file.")
        parser.print_help()
        exit(1)

    # Call the appropriate function based on the options
    if args.options == "d":
        state_change_files = find_files(args.dir, target_suffix="state-changes.log")
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.options == "f":
        plot_state_changes(args.fname, show=args.show)

    elif args.options == "c":
        compute_transition_probabilities(args.fname, show=args.show)

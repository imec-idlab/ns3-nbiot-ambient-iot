import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt


def find_state_changes_files(directory: str, target_suffix = "state-changes.log") -> list:

    files_found = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(target_suffix):
                rel_path = os.path.relpath(os.path.join(root, file))
                files_found.append(rel_path)
    return files_found


def process_state_changes_file(file_path: str) -> pd.DataFrame:
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="State Changes")
    parser.add_argument("--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot")
    args = parser.parse_args()

    if args.dir is not None:
        state_change_files = find_state_changes_files(args.dir)
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.fname is not None:
        df = process_state_changes_file(args.fname)

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
        if args.show:
            plt.show()
        else:
            plt.savefig(args.fname.replace('.log', '.png'))

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt


def find_mac_files(directory: str, target_suffix = "MAC.log") -> list:

    files_found = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(target_suffix):
                rel_path = os.path.relpath(os.path.join(root, file))
                files_found.append(rel_path)
    return files_found


def process_mac_file_for_collisions(file_path: str) -> list:
    data = []

    # Read and parse the file
    with open(file_path, "r") as f:
        log = f.read()

    # Extract all PreambleCollision times (in ms)
    collision_times_ms = [
        int(match.group(1))
        for match in re.finditer(r",PreambleCollision,(\d+)", log)
    ]

    # Convert to seconds
    collision_times_s = [t / 1000.0 for t in collision_times_ms]

    return collision_times_s


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="State Changes")
    parser.add_argument("--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--plot-fname", type=str, default="collisions.png", help="Output plot file name")
    parser.add_argument("--show", action="store_true", help="Show the plot")
    args = parser.parse_args()

    if args.dir is not None:
        state_change_files = find_mac_files(args.dir)
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.fname is not None:
        collision_times_s = process_mac_file_for_collisions(args.fname)

        # Plot the collisions as events
        plt.figure(figsize=(10, 4))
        plt.eventplot(
            collision_times_s,
            orientation='horizontal',
            lineoffsets=0.5,      # y=0
            linelengths=1.0,
            colors='red')
        plt.axhline(
            y=0,
            color='black',
            linewidth=1
        )

        plt.xlabel("Time (seconds)")
        plt.title("Preamble Collision Events Over Time")
        plt.grid(True)
        plt.yticks([1], ["Collision"])
        plt.ylim(0, 1.1)
        plt.tight_layout()
        if args.show:
            plt.show()
        else:
            print(f"Saving plot to {args.plot_fname}")
            plt.savefig(args.plot_fname)
        plt.close()

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

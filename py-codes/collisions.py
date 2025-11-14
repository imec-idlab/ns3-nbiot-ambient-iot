import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt

from utilities import find_files


def process_mac_file_for_collisions(file_path: str) -> list:
    """
    Processes a MAC log file and returns a list of times (in seconds) of collisions.
    Each line contains: CellRnti, Event, Time
    Event = {PreambleReceived, PreambleCollision}
    
    The times are extracted from the log file by searching for the string
    ",PreambleCollision," followed by a number (in milliseconds).

    :param file_path: The path to the MAC log file
    :return: A list of times (in seconds) of collisions
    """
    data = []

    # Read and parse the file
    with open(file_path, "r") as f:
        log = f.read()

    # Extract all PreambleCollision times (in ms)
    collision_times_ms = [
        int(match.group(1))
        for match in re.finditer(r",PreambleCollision,(\d+),.*", log)
    ]

    # Convert to seconds
    collision_times_s = [t / 1000.0 for t in collision_times_ms]

    return collision_times_s


def plot_collisions(filename, show: bool = False):
    """
    Process a MAC log file for preamble collision events and plot them as events.

    :param filename: path to the MAC log file
    :param show: whether to show the plot instead of saving it (default: False)
    """
    collision_times_s = process_mac_file_for_collisions(filename)

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
    if show:
        plt.show()
    else:
        plt_filename = filename.replace("MAC.log", "Collisions.png")
        print(f"Saving plot to {plt_filename}")
        plt.savefig(plt_filename)
    plt.close()


# This script processes MAC log files to extract preamble collision events and plot them.

# Example usage:
# python py-codes/collisions.py --fname logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_MAC.log

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="State Changes")
    parser.add_argument('-d', "--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument('-f', "--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot")
    args = parser.parse_args()

    if args.dir is not None:
        state_change_files = find_files(args.dir, target_suffix="MAC.log")
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.fname is not None:
        plot_collisions(args.fname, args.show)

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt

from utilities import find_files


def process_energy_file(file_path: str) -> pd.DataFrame:
    """
    Reads an energy log file and converts it into a pandas DataFrame.

    Args:
        file_path (str): The path to the energy log file to read.

    Returns:
        pd.DataFrame: A DataFrame containing the data from the file. It has the following columns:
            - time_s (float): The time in seconds
            - imsi (int): The IMSI of the device
            - ce_level (int): The CE level of the device
            - energy (float): The current energy level of the device in J
            - fraction (float): The fraction of the battery that is left
    """
    pattern = re.compile(
        r"(?P<time>\d+\.\d+)s\s+\[/NodeList/(?P<node>\d+)/ApplicationList/\d+/State\]\s+State changed from (?P<old>\d+) to (?P<new>\d+)"
    )

    data = []

    # Read and parse the file
    with open(file_path, "r") as f:
        for line in f:
            fields = line.strip().split(',')
            time_ns = float(fields[0].replace('+', '').replace('ns', ''))
            time_s = time_ns / 1e9  # Convert ns to seconds
            imsi = int(fields[1])
            ce_level = int(fields[2])
            energy = float(fields[3])
            fraction = float(fields[4])
            data.append([time_s, imsi, ce_level, energy, fraction])

    # Create a DataFrame
    df = pd.DataFrame(data, columns=['time_s', 'imsi', 'ce_level', 'energy', 'fraction'])

    return df


def plot_energy(filename, show=False):
    """
    Reads an energy log file and plots the energy remaining over time for each device.

    Args:
        filename (str): The path to the energy log file to read.
        show (bool): If True, the plot will be shown. Defaults to False.

    Returns:
        None
    """
    df = process_energy_file(filename)

    # Plot energy vs. time for each device
    plt.figure(figsize=(10, 6))
    for imsi_id, group in df.groupby('imsi'):
        plt.plot(group['time_s'], group['energy'], label=f'Device {imsi_id}')

    plt.xlabel("Time (seconds)")
    plt.ylabel("Energy Remaining")
    plt.title("Energy vs Time per Device (IMSI)")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()

    if show:
        plt.show()
    else:
        plt.savefig(filename.replace('.log', '.png'))
    plt.close()


# Example usage:
# 1. To find energy files in a directory and subdirectories:
# python energy.py --dir /path/to/directory
#
# 2. To process a specific energy file and plot the results:
# python energy.py --fname /path/to/energy.log --show
#
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument("--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    if args.dir is not None:
        state_change_files = find_files(args.dir, target_suffix="Energy.log")
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.fname is not None:
        plot_energy(args.fname, args.show)

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

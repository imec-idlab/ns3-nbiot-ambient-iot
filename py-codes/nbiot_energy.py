import argparse
import matplotlib.pyplot as plt
import pandas as pd

from utilities import find_files


# Mapping from numeric states to enum names
# These values are defined in ns3/nb-iot-energy.h in an enum called PowerState
state_map = {
    0: 'OFF',
    1: 'CONNECTED_IDLE',
    2: 'CONNECTED_RECEIVING_NPDSCH',
    3: 'CONNECTED_RECEIVING_NPDCCH',
    4: 'CONNECTED_SENDING_NPRACH',
    5: 'CONNECTED_SENDING_NPUSCH',
    6: 'CONNECTED_SENDING_NPUSCH_F2',
    7: 'SUSPENDED_DRX',
    8: 'SUSPENDED_EDRX',
    9: 'SUSPENDED_PSM'
}

import pandas as pd
import matplotlib.pyplot as plt

# Define if cumulative plot or discrete vertical lines
use_cumulative = False

# State group mapping
group_map = {
    0: "OFF",
    1: "IDLE",
    2: "RX", 3: "RX",
    4: "TX", 5: "TX", 6: "TX",
    7: "SUSPEND", 8: "SUSPEND", 9: "SUSPEND"
}


def plot_energy_usage_consolidated(log_file, use_cumulative=True, show=False, xmin=None, xmax=None):
    # Read log file
    df = pd.read_csv(log_file, header=None, names=["Time", "IMSI", "State", "Energy"])

    # Map states to groups
    df['StateGroup'] = df['State'].map(group_map)
    df['Time'] = df['Time'] / 1000  # Convert time from milliseconds to seconds

    N = len(df['IMSI'].unique())
    # Plot setup
    fig, axs = plt.subplots(N, 1, figsize=(14, 7 * N), sharex=True)
    color_list = ["blue", "green", "red", "orange", "purple"]  # For five groups
    if N == 1:
        axs = [axs]

    if use_cumulative:
        max_time = df['Time'].max()

        for ax, [imsi, group_imsi] in zip(axs, df.groupby('IMSI')):
            for idx, (state, group_data) in enumerate(group_imsi.groupby("StateGroup")):
                group_sorted = group_data.sort_values("Time")
                energy_cumsum = group_sorted['Energy'].cumsum()

                # Extend the last time point to max_time if necessary
                times = list(group_sorted['Time'])
                energies = list(energy_cumsum)

                if times[-1] < max_time:
                    times.append(max_time)
                    energies.append(energies[-1])

                ax.plot(times, energies, label=state, color=color_list[idx % len(color_list)])
                ax.set_ylabel("Cumulative Energy (Joules)")
                ax.set_title(f"IMSI: {imsi}")

    else:
        for ax, [imsi, group_imsi] in zip(axs, df.groupby('IMSI')):
            for idx, (state, group_data) in enumerate(group_imsi.groupby("StateGroup")):
                group_sorted = group_data.sort_values("Time")
                color = color_list[idx % len(color_list)]
                ax.scatter(group_sorted['Time'], group_sorted['Energy'], label=state, color=color, alpha=0.5)
                ax.set_ylabel("Energy (Joules)")
                ax.set_title(f"IMSI: {imsi}")

        ax.set_ylim(bottom=0)

    # Common aesthetics
    ax.set_xlabel("Time (seconds)")
    # ax.set_title("Energy Usage by Grouped Power States")
    ax.grid(True)

    # # Place legend outside and avoid duplicate labels
    # handles, labels = ax.get_legend_handles_labels()
    # unique = dict(zip(labels, handles))
    # ax.legend(unique.values(), unique.keys(), loc='center left', bbox_to_anchor=(1, 0.5))

    plt.tight_layout()
    if show:
        plt.show()
    else:
        output_file = log_file.replace('.log', '_cons_acc.png' if use_cumulative else '_cons.png')
        plt.savefig(output_file)
        print(f"Plot saved as {output_file}")
    plt.close(fig)


def plot_energy_usage(log_file, use_cumulative = True, show=False, xmin=None, xmax=None):
    # Read log file assuming CSV format
    df = pd.read_csv(log_file, header=None, names=["Time", "IMSI", "State", "Energy"])
    # Map states to readable names
    df['StateName'] = df['State'].map(state_map)
    df['Time'] = df['Time'] / 1000  # Convert time from milliseconds to seconds

    # Should group energy per timestamp?
    # df = df.groupby(['Time', 'StateName'])['Energy'].sum().reset_index()

    N = len(df['IMSI'].unique())
    # Plot energy vs time for each state
    fig, axs = plt.subplots(N, 1, figsize=(14, 6 * N), sharex=True)
    if N == 1:
        axs = [axs]

    # Colors for each state
    colors = [
        "blue",
        "green",
        "red",
        "cyan",
        "magenta",
        "yellow",
        "black",
        "orange",
        "purple",
        "brown"
    ]
    if use_cumulative:
        # Line plot with cumulative energy
        max_time = df['Time'].max()
        for ax, [imsi, group_imsi] in zip(axs, df.groupby('IMSI')):
            for state, group in group_imsi.groupby('StateName'):
                group_sorted = group.sort_values('Time')
                energy_cumsum = group_sorted['Energy'].cumsum()

                # Extend the last time point to max_time if necessary
                times = list(group_sorted['Time'])
                energies = list(energy_cumsum)

                if times[-1] < max_time:
                    times.append(max_time)
                    energies.append(energies[-1])
                ax.plot(times, energies, label=state, color=colors[group_sorted['State'].iloc[0] % len(colors)])
                ax.set_title(f"IMSI: {imsi}")
    else:
        # Event plot for energy bursts
        for ax, [imsi, group_imsi] in zip(axs, df.groupby('IMSI')):
            for state, group in df.groupby('StateName'):
                group_sorted = group.sort_values('Time')
                color = colors[group_sorted['State'].iloc[0]]
                ax.scatter(group_sorted['Time'], group_sorted['Energy'], label=state, color=color, alpha=0.5)
                ax.set_title(f"IMSI: {imsi}")

    plt.xlabel("Time (seconds)")
    plt.ylabel(f"{'Cumulative ' if use_cumulative else ''}Energy (Joules)")
    # plt.title("Energy Usage by Power State Over Time")
    plt.legend(loc="best")
    plt.grid(True)

    _min = 0 if xmin is None else max(0, df['Time'].min())
    _max = df['Time'].max() if xmax is None else min(df['Time'].max(), xmax)
    plt.xlim(_min, _max)

    # Place legend outside and avoid duplicate labels
    handles, labels = ax.get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    ax.legend(unique.values(), unique.keys(), loc='center left', bbox_to_anchor=(1, 0.5))

    plt.tight_layout()
    if show:
        plt.show()
    else:
        output_file = log_file.replace('.log', '_acc.png' if use_cumulative else '.png')
        plt.savefig(output_file)
        print(f"Plot saved as {output_file}")
    plt.close(fig)


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
    parser.add_argument('-f', "--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")

    parser.add_argument("--min", type=float, default=None, help="Minimum time in seconds to consider for plotting")
    parser.add_argument("--max", type=float, default=None, help="Maximum time in seconds to consider for plotting")

    parser.add_argument("--cumulative", action="store_true", help="Use cumulative energy for plotting")
    parser.add_argument("--consolidate", action="store_true", help="Use consolidated energy for plotting")

    args = parser.parse_args()

    if args.dir is not None:
        energy_log_files = find_files(args.dir, target_suffix="nbiot_energy.log")
        print("Found NB-IoT energy files:")
        for file in energy_log_files:
            print(file)

    elif args.fname is not None:
        if not args.consolidate:
            plot_energy_usage(args.fname, args.cumulative, args.show, args.min, args.max)
        else:
            plot_energy_usage_consolidated(args.fname, args.cumulative, args.show, args.min, args.max)

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

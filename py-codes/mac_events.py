import os
import argparse
from tqdm import tqdm

from collections import defaultdict
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import matplotlib.colors as mcolors

from utilities import find_files
from rnti_imsi_map import map_rnti_imsi_from_log


# Cycled list of marker styles for auto assignment
available_markers = ['^', 's', 'o', 'D', 'v', '*', 'P', 'h', '8', '<', '>']
null_marker = 'X'  # Default marker for None


def read_mac_events(filename):
    """
    Read events from a MAC log file and return them as a list of tuples along with
    a set of unique RNTIs.

    The log file is expected to have the following format:
        RNTI,Event,Time_in_ms

    The function returns a tuple containing a list of events and a set of unique RNTIs.
    Each event is represented as a tuple containing the RNTI (or None if not present),
    the event name, and the time of the event in seconds.

    :param filename: Path to the MAC log file
    :return: A tuple containing a list of events and a set of unique RNTIs
    """
    events = []
    rnti_set = set()

    # Parse log file
    with open(filename, "r") as file:
        for line in file:
            parts = line.strip().split(",")
            # Detect RNTI and event info
            try:
                rnti = int(parts[0].strip()) if parts[0].strip() else None
                event = parts[1].strip()
                time = int(parts[-1].strip())
                time /= 1000  # convert to seconds
                if rnti is not None:
                    rnti_set.add(int(rnti))
                events.append((rnti, event, time))
            except Exception:
                continue  # Skip malformed lines

    return events, [int(x) if x is not None else x for x in rnti_set]


def plot_events(events, identity_set, ylabel, filename: str = None, show_legend: bool = True):
    """
    Plot MAC events from a log file against time.

    :param events: A list of events from the log file, where each event is a tuple containing the RNTI (or None if not present),
                   the event name, and the time of the event in seconds.
    :param identity_set: A list of unique RNTIs or IMSIs.
    :param ylabel: The y-axis label, either "RNTI" or "IMSI".
    :param filename: The file name to save the plot to, or None to show the plot instead of saving it.
    :param show_legend: Whether to show the legend or not.
    """

    # Plot setup
    plt.figure(figsize=(12, 6))

    # identify the unique UEs to assign colors
    ues = set([idx for idx, _, _ in events])
    ues = sorted({ue for ue in ues if ue is not None})  # list of unique UEs (in order)
    cmap = matplotlib.colormaps.get_cmap('tab10').resampled(len(ues))  # or 'viridis', 'plasma', etc.
    ue_colors = {ue: cmap(i) for i, ue in enumerate(ues)}

    # Plot each event
    for ue_idx, event, time in tqdm(events, desc=f"Events with {ylabel}"):
        marker = null_marker if ue_idx is None else available_markers[identity_set.index(ue_idx) % len(available_markers)]
        if ue_idx is not None:
            plt.scatter(time, int(ue_idx), marker=marker,
                        label=event if event != 'PreambleReceived' else f"{event}: {ue_idx}",
                        edgecolors='black',
                        color=ue_colors[ue_idx]
                        )
        else:
            # Plot across all RNTIs
            for r in identity_set:
                plt.scatter(time, r, marker=marker, label=event, edgecolors='gray', color="black",alpha=0.5)

    plt.gca().yaxis.set_major_locator(MaxNLocator(integer=True))

    plt.xlabel("Time (s)")
    plt.ylabel(ylabel)
    plt.title("NB-IoT MAC Events vs Time")
    plt.grid(True)

    # Deduplicate legend entries
    if show_legend:
        handles, labels = plt.gca().get_legend_handles_labels()
        unique = dict(zip(labels, handles))
        plt.legend(unique.values(), unique.keys(), loc='upper right')

    plt.tight_layout()
    if filename is None:
        plt.show()
    else:
        plt.savefig(filename)
        print(f"Plot saved as {filename}")
    plt.close()



def plot_mac_events(filename, show: bool = False, show_legend: bool = False):
    # Will contain event-to-marker mapping
    """
    Process a MAC log file for events and plot them vs time.

    :param filename: path to the MAC log file
    :param show: whether to show the plot instead of saving it (default: False)
    :param show_legend: Whether to show the legend or not.
    """
    events, rnti_set = read_mac_events(filename)

    ylabel = "UE IMSI" if "ueMAC" in filename else "RNTI"
    img_filename = None if show else filename.replace("MAC.log", "MAC_Events.png")

    plot_events(events, rnti_set, ylabel, img_filename, show_legend)


def plot_mac_events_imsi(filename, show: bool = False):
    """
    Process a MAC log file for events and plot them vs time, using IMSIs from a cell connection log file.

    :param filename: path to the MAC log file
    :param show: whether to show the plot instead of saving it (default: False)
    """
    events, rnti_set = read_mac_events(filename)
    map_files = find_files(os.path.dirname(filename), target_suffix="cell_connection.log")
    if not map_files:
        print(f"No mapping file found for {filename}. Skipping IMSI mapping.")
        return

    map_file = map_files[0]
    rnti_imsi_map = map_rnti_imsi_from_log(map_file)
    dict_rnti_to_imsi = dict(zip(rnti_imsi_map["RNTI"], rnti_imsi_map["IMSI"]))

    # Filter events to only those with RNTIs in the mapping
    filtered_map = rnti_imsi_map[rnti_imsi_map["RNTI"].isin(rnti_set)]
    imsi_set = list(set(filtered_map["IMSI"]))

    # Map the events
    converted_events = []
    for rnti, event, time in events:
        imsi = None if rnti is None else dict_rnti_to_imsi.get(int(rnti), rnti)
        converted_events.append((imsi, event, time))

    # Plot setup
    img_filename = None if show else filename.replace("MAC.log", "MAC_Events_IMSI.png")
    plot_events(converted_events, imsi_set, ylabel="IMSI", filename=img_filename)


# This compiles MAC events from a log file and plots them against time.
# It can be used for eNB or UE MAC logs.

# Example usage:
# 1. for eNB MAC log:
# python py-codes/mac_operations.py --fname logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_MAC.log

# 2. for UE MAC log:
# python py-codes/mac_operations.py -f logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_ueMAC.log

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument('-f', "--fname", type=str,
                        default=None,
                        # required=True,
                        help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    if args.fname is None or not os.path.exists(args.fname):
        print(f"File '{args.fname}' does not exist.")
        exit(1)

    plot_mac_events(args.fname, show=args.show, show_legend=args.fname.endswith("ueMAC.log"))
    if not args.fname.endswith("ueMAC.log"):
        # ueMAC.log already plots values for IMSI
        # there is no need to try the conversion provided in plot_mac_events_imsi()
        plot_mac_events_imsi(args.fname, show=args.show)

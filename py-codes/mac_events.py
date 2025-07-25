import os
import argparse
import itertools
from collections import defaultdict
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

from utilities import find_files
from rnti_imsi_map import map_rnti_imsi_from_log


# Cycled list of marker styles for auto assignment
available_markers = ['^', 's', 'o', 'D', 'v', '*', 'P', 'h', '8', '<', '>']
null_marker = 'X'  # Default marker for None


def read_mac_events(filename):
    # Parse log file
    events = []
    rnti_set = set()

    with open(filename, "r") as file:
        for line in file:
            parts = line.strip().split(",")
            # Detect RNTI and event info
            try:
                rnti = int(parts[0].strip()) if parts[0].strip() else None
                event = parts[1].strip()
                time = int(parts[-1].strip())
                if rnti is not None:
                    rnti_set.add(int(rnti))
                events.append((rnti, event, time))
            except Exception:
                continue  # Skip malformed lines

    return events, [int(x) if x is not None else x for x in rnti_set]


def plot_events(events, identity_set, ylabel, filename: str = None):
    # Plot setup
    plt.figure(figsize=(12, 6))

    # Plot each event
    for _idx, event, time in events:
        # marker = event_markers.get(event, "*")
        marker = null_marker if _idx is None else available_markers[identity_set.index(_idx) % len(available_markers)]
        if _idx is not None:
            plt.scatter(time, int(_idx), marker=marker, label=event if event != 'PreambleReceived' else f"{event}: {_idx}", edgecolors='black')
        else:
            # Plot across all RNTIs
            for r in identity_set:
                plt.scatter(time, r, marker=marker, label=event, edgecolors='gray', alpha=0.5)

    plt.gca().yaxis.set_major_locator(MaxNLocator(integer=True))

    plt.xlabel("Time (ms)")
    plt.ylabel(ylabel)
    plt.title("NB-IoT MAC Events vs Time")
    plt.grid(True)

    # Deduplicate legend entries
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



def plot_mac_events(filename, show: bool = False):
    # Will contain event-to-marker mapping
    """
    Process a MAC log file for events and plot them vs time.

    :param filename: path to the MAC log file
    :param show: whether to show the plot instead of saving it (default: False)
    """
    events, rnti_set = read_mac_events(filename)

    ylabel = "UE IMSI" if "ueMAC" in filename else "RNTI"
    img_filename = None if show else filename.replace("MAC.log", "MAC_Events.png")

    plot_events(events, rnti_set, ylabel, img_filename)


def plot_mac_events_imsi(filename, show: bool = False):
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
                        default="logs/markov/u3_t60000000000_c0_e0/25_07_2025_11_35_44/w0_s1_MAC.log",
                        # required=True,
                        help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    if not os.path.exists(args.fname):
        print(f"File '{args.fname}' does not exist.")
        exit(1)

    plot_mac_events(args.fname, show=args.show)
    plot_mac_events_imsi(args.fname, show=args.show)

import argparse
import itertools
from collections import defaultdict
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator


# Cycled list of marker styles for auto assignment
available_markers = itertools.cycle(['^', 's', 'X', 'o', 'D', 'v', '*', 'P', 'h', '8'])


def plot_mac_events(filename, show: bool = False):
    # Will contain event-to-marker mapping
    """
    Process a MAC log file for events and plot them vs time.

    :param filename: path to the MAC log file
    :param show: whether to show the plot instead of saving it (default: False)
    """
    event_markers = {}

    # Parse log file
    events = []
    rnti_set = set()

    with open(filename, "r") as file:
        for line in file:
            parts = line.strip().split(",")
            # Detect RNTI and event info
            try:
                rnti = parts[0].strip() if parts[0].strip() else None
                event = parts[1].strip()
                time = int(parts[-1].strip())
                if rnti is not None:
                    rnti_set.add(int(rnti))
                events.append((rnti, event, time))
            except Exception:
                continue  # Skip malformed lines

    # Plot setup
    plt.figure(figsize=(12, 6))

    # Plot each event
    for rnti, event, time in events:
        marker = event_markers.get(event, "*")
        if rnti is not None:
            plt.scatter(time, int(rnti), marker=marker, label=event, edgecolors='black')
        else:
            # Plot across all RNTIs
            for r in rnti_set:
                plt.scatter(time, r, marker=marker, label=event, edgecolors='gray', alpha=0.5)

    plt.gca().yaxis.set_major_locator(MaxNLocator(integer=True))

    plt.xlabel("Time (ms)")
    if "ueMAC" in filename:
        plt.ylabel("UE IMSI")
    else:
        plt.ylabel("RNTI")
    plt.title("NB-IoT MAC Events vs Time")
    plt.grid(True)

    # Deduplicate legend entries
    handles, labels = plt.gca().get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    plt.legend(unique.values(), unique.keys(), loc='upper right')
    plt.tight_layout()
    if show:
        plt.show()
    else:
        plt_filename = filename.replace("MAC.log", "MAC_Events.png")
        plt.savefig(plt_filename)
        print(f"Plot saved as {plt_filename}")
    plt.close()


# This compiles MAC events from a log file and plots them against time.
# It can be used for eNB or UE MAC logs.

# Example usage:
# 1. for eNB MAC log:
# python py-codes/mac_operations.py --fname logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_MAC.log

# 2. for UE MAC log:
# python py-codes/mac_operations.py --fname logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_ueMAC.log

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    plot_mac_events(args.fname, show=args.show)

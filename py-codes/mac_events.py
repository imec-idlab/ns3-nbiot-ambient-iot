import matplotlib.pyplot as plt
from collections import defaultdict
import itertools


# Example usage:
# python py-codes/mac_operations.py --fname logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_MAC.log


# Cycled list of marker styles for auto assignment
available_markers = itertools.cycle(['^', 's', 'X', 'o', 'D', 'v', '*', 'P', 'h', '8'])

# Will contain event-to-marker mapping
event_markers = {}

# Parse log file
log_file = "logs/markov/u3_t180000000000_c0_e0/22_07_2025_13_59_49/w0_s1_MAC.log"
events = []
rnti_set = set()

with open(log_file, "r") as file:
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

plt.xlabel("Time (ms)")
plt.ylabel("RNTI")
plt.title("NB-IoT MAC Events vs Time")
plt.grid(True)

# Deduplicate legend entries
handles, labels = plt.gca().get_legend_handles_labels()
unique = dict(zip(labels, handles))
plt.legend(unique.values(), unique.keys(), loc='upper right')
plt.tight_layout()
filename = log_file.replace("MAC.log", "MAC_Events.png")
plt.savefig(filename)
print(f"Plot saved as {filename}")
plt.close()

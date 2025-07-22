import argparse
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap


def plot_uplink_grid(filename):
    with open(filename, 'r') as f:
        data = []
        for line in f:
            row = [int(x) for x in line.strip().split(',') if x != '']
            data.append(row)

    grid = np.array(data)
    # print(f"Grid shape: {grid.shape}")
    vmin = np.min(grid)
    vmax = np.max(grid)
    num_classes = vmax - vmin + 1

    base_cmap = plt.colormaps['tab20']
    base_colors = base_cmap(np.linspace(0, 1, num_classes))

    # Offset for negative values
    offset = -vmin

    # Set -1 to light gray and 0 to white
    base_colors[-1 + offset] = [0.85, 0.85, 0.85, 1.0]  # light gray
    base_colors[0 + offset]  = [1.0, 1.0, 1.0, 1.0]     # white

    # Create custom colormap
    cmap = ListedColormap(base_colors)

    plt.figure(figsize=(12, 6))
    im = plt.imshow(grid.T, aspect='auto', interpolation='nearest', cmap=cmap, vmin=vmin, vmax=vmax)

    cbar = plt.colorbar(im, ticks=range(vmin, vmax+1))
    cbar.set_label("RNTI (UE ID) or Status")
    # cbar.set_ticklabels([str(i) for i in range(vmin, vmax+1)])

    # Custom tick labels
    tick_labels = []
    for val in range(vmin, vmax + 1):
        if val == -1:
            tick_labels.append("No Tx")
        # elif val == 0:
        #     tick_labels.append("Idle")
        else:
            tick_labels.append(str(val))  # UE RNTI

    cbar.set_ticklabels(tick_labels)

    ax = plt.gca()
    # print(f"Number of classes: {num_classes}")
    ax.yaxis.set_major_locator(plt.MaxNLocator(grid.shape[1], integer=True))
    for y in range(grid.shape[1]):
        plt.axhline(y = y + 0.5, color='gray', linestyle='-')

    plt.title("Spectral Uplink Usage (Time vs Carrier/Resource Unit)")
    plt.ylabel("Carrier")
    plt.xlabel("Time Step (e.g., Subframe Index)")
    plt.tight_layout()
    img_filename = filename.replace('.log', '.png')
    plt.savefig(img_filename)
    print(f"Plot saved as '{os.path.basename(img_filename)}'")
    plt.close()


# To run this script with a log file as an argument.
#
# Usage:
# python check_spectral_uplink.py <filename.log>


# Example usage
# python check_spectral_uplink.py "/home/h3dema/ns3-nbiot/logs/markov/u3_t20000000000_c0_e0/22_07_2025_11_57_42/w0_s1_Spectral_Uplink.log"
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot spectral uplink usage from log file.")
    parser.add_argument("filename", type=str, help="Path to the log file containing spectral uplink data.")
    args = parser.parse_args()

    if not os.path.exists(args.filename):
        print(f"File '{args.filename}' does not exist.")
        exit(1)

    # For testing purposes, you can call the function directly with a specific file
    plot_uplink_grid(args.filename)
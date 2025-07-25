import argparse
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

from utilities import find_files
from rnti_imsi_map import map_rnti_imsi_from_log


def read_grid(filename):
    """
    Reads a 2D grid of integers from a text file and returns it as a 2D numpy array.

    The file is expected to contain a 2D array of integers, where each row is a line in the file
    and each cell is separated by a comma. The function will return a 2D numpy array of shape
    (number of rows, number of columns) where each cell contains the integer value read from the file.

    The log file is expected to contain a 2D array of integers, where
    - each row represents a subframe (time step),
    - each column represents a carrier (resource unit),
    - each cell contains the RNTI (UE ID) or status of the UE transmitting uplink in that time/carrier:
        - -1: No transmission in that time/carrier (light gray)
        - 0: Idle in that time/carrier (white)
        - UE RNTI (positive integer): UE transmitting uplink in that time/carrier (color based on RNTI)

    :param filename: path to the file containing the grid data
    :return: 2D numpy array of shape (number of rows, number of columns) containing the grid data
    """
    with open(filename, 'r') as f:
        data = []
        for line in f:
            row = [int(x) for x in line.strip().split(',') if x != '']
            data.append(row)
    grid = np.array(data)
    return grid


def plot_uplink_usage(filename, show=False):
    """
    Plot spectral uplink usage as an image (2D array) from the given log file.

    The plot is saved as a PNG image with the same name as the log file, but with a .png extension.

    :param filename: path to the log file containing spectral uplink data
    :param show: If True, the plot will be shown. Defaults to False.
    """
    grid = read_grid(filename)
    img_filename = None if show else filename.replace('.log', '.png')

    plot_grid(grid, var_name="RNTI",img_filename=img_filename)


def plot_uplink_usage_imsi(filename, show=False):
    grid = read_grid(filename)
    img_filename = None if show else filename.replace('.log', '.png')

    map_files = find_files(os.path.dirname(filename), target_suffix="cell_connection.log")
    if not map_files:
        print(f"No mapping file found for {filename}. Skipping IMSI mapping.")
        return

    map_file = map_files[0]
    rnti_imsi_map = map_rnti_imsi_from_log(map_file)

    # Build the mapping dictionary
    dict_rnti_to_imsi = dict(zip(rnti_imsi_map["RNTI"], rnti_imsi_map["IMSI"]))

    # Create an empty array for the new grid
    imsi_grid = np.copy(grid)

    # Map RNTI values in the grid to IMSI values
    # Vectorized replacement
    positive_mask = grid >= 0  # only positive values are RNTI values
    imsi_grid[positive_mask] = np.vectorize(lambda r: dict_rnti_to_imsi.get(r, r))(grid[positive_mask])

    img_filename = None if show else filename.replace('Spectral_Uplink.log', 'Spectral_Uplink_IMSI.png')
    plot_grid(imsi_grid, var_name="IMSI", img_filename=img_filename)


def plot_grid(grid, var_name: str, img_filename: str = None):
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
    cbar.set_label(f"{var_name} or Status")
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

    if img_filename is None:
        plt.show()
    else:
        # Save the plot as a PNG file
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
    parser.add_argument('-f', "--fname",
                        type=str,
                        required=True,
                        help="Path to the log file containing spectral uplink data.")
    args = parser.parse_args()

    if not os.path.exists(args.fname):
        print(f"File '{args.fname}' does not exist.")
        exit(1)

    # plot the uplink information from the log file
    plot_uplink_usage(args.fname)
    plot_uplink_usage_imsi(args.fname)
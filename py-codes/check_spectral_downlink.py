import argparse
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

from utilities import find_files
from rnti_imsi_map import map_rnti_imsi_from_log


def read_downlink_log(filename):
    """
    Reads a file containing a list of RNTI values (or special negative values for special signals)
    into a numpy array.

    Parameters
    ----------
    filename : str
        Name of the file to read.

    Returns
    -------
    numpy.array
        Array of RNTI values.
    """

    # Read file into a list from processing
    with open(filename, 'r') as f:
        data = [int(line.strip()) for line in f if line.strip() != '']
    # Convert list to numpy array
    return np.array(data)


def plot_downlink_info(arr, filename, ylabel, show=False):
    """
    Plot spectral downlink usage from log file as an image (single row).

    Parameters
    ----------
    arr : numpy.array
        Array of RNTI values (or special negative values for special signals)
    filename : str
        Output filename for the plot (or None for show-only)
    ylabel : str
        Y-axis label for the plot
    show : bool, optional
        Whether to show the plot (default: False, save to file instead)

    Notes
    -----
    The special negative values are:
        -1: MIB-NB (gray)
        -2: NPSS (blue)
        -3: NSSS (dark blue)
        -4: SIB1-NB (orange)
        -5: Repetition (yellow)
        0: UE RNTI 0 = white
    """
    vmin = np.min(arr)
    vmax = np.max(arr)
    num_classes = vmax - vmin + 1
    offset = -vmin

    # Create base colormap
    base_cmap = plt.colormaps['tab20']
    base_colors = base_cmap(np.linspace(0, 1, num_classes))

    # Custom colors for special signals
    base_colors[-1 + offset] = [0.7, 0.7, 0.7, 1.0]   # MIB-NB (gray)
    base_colors[-2 + offset] = [0.3, 0.6, 1.0, 1.0]   # NPSS (blue)
    base_colors[-3 + offset] = [0.1, 0.3, 0.7, 1.0]   # NSSS (dark blue)
    base_colors[-4 + offset] = [1.0, 0.6, 0.1, 1.0]   # SIB1-NB (orange)
    base_colors[-5 + offset] = [1.0, 1.0, 0.4, 1.0]   # Repetition (yellow)
    base_colors[0 + offset]  = [1.0, 1.0, 1.0, 1.0]   # UE RNTI 0 = white

    cmap = ListedColormap(base_colors)

    # Plot the data as an image (single row)
    fig, ax = plt.subplots(figsize=(12, 6))
    cax = plt.imshow(arr[np.newaxis, :], aspect='auto', cmap=cmap, vmin=vmin, vmax=vmax)

    ax.set_title("Spectral Downlink Usage")
    ax.set_yticks([])  # Hide Y-axis ticks
    ax.set_xlabel("Subframe Index")

    # Add colorbar with custom ticks
    cbar = plt.colorbar(cax, ax=ax, orientation='horizontal', pad=0.2, ticks=range(vmin, vmax + 1))
    cbar.set_label(f"{ylabel} / Signal Type")  # , labelpad=10)

    # Custom tick labels
    tick_labels = []
    for val in range(vmin, vmax + 1):
        if val == -1:
            tick_labels.append("MIB-NB")
        elif val == -2:
            tick_labels.append("NPSS")
        elif val == -3:
            tick_labels.append("NSSS")
        elif val == -4:
            tick_labels.append("SIB1-NB")
        elif val == -5:
            tick_labels.append("Repetition")
        elif val == 0:
            tick_labels.append("Idle/UE 0")
        else:
            tick_labels.append(f"{val}")
    cbar.set_ticklabels(tick_labels, rotation=45)

    # plt.subplots_adjust(bottom=0.5)  # Make space for colorbar and labels

    plt.subplots_adjust(bottom=0.3)
    plt.tight_layout()
    if show:
        plt.show()
    else:
        # Save the plot as a PNG file
        plt.savefig(filename)
        print(f"Plot saved as '{os.path.basename(filename)}'")
    plt.close(fig)


def plot_downlink_usage(filename, show=False):
    """
    Plot the spectral downlink usage as an image (single row) from the given log file.

    The log file is expected to contain a single integer value per line, which is the RNTI of the UE
    that is using the subframe, or a special negative value for special signals:
        -1: MIB-NB
        -2: NPSS
        -3: NSSS
        -4: SIB1-NB
        -5: Repetition

    The plot is saved as a PNG image with the same name as the log file, but with a .png extension.
    """
    arr = read_downlink_log(filename)
    img_filename = None if show else filename.replace('.log', '.png')
    plot_downlink_info(arr, img_filename, ylabel="RNTI", show=show)


def plot_downlink_usage_imsi(filename, show=False):
    arr = read_downlink_log(filename)
    img_filename = None if show else filename.replace('.log', '_imsi.png')

    # Read the RNTI to IMSI mapping from another log file
    map_files = find_files(os.path.dirname(filename), target_suffix="cell_connection.log")
    if not map_files:
        print(f"No mapping file found for {filename}. Skipping IMSI mapping.")
        return

    map_file = map_files[0]
    rnti_imsi_map = map_rnti_imsi_from_log(map_file)
    dict_rnti_to_imsi = dict(zip(rnti_imsi_map["RNTI"], rnti_imsi_map["IMSI"]))

    def convert(val):
        if val >= 0 and val in dict_rnti_to_imsi:
            return dict_rnti_to_imsi[val]
        else:
            return val
    # Use np.vectorize to apply conversion element-wise
    arr_imsi = np.vectorize(convert)(arr)

    plot_downlink_info(arr_imsi, img_filename, ylabel="IMSI", show=show)


# Each NbiotScheduler instance is attached to a specific eNB (base station).
# In NS-3’s NB-IoT implementation, NbiotScheduler::LogDownlinkGrid logs a single value per subframe because
# it’s primarily designed to reflect the base station's (eNB's) perspective of downlink resource allocation.
# Thus, the BS's scheduler logs only its own downlink resource allocation.
# When you enable tracing (like Spectral_Downlink.log), NS-3 writes to separate files per node or per scheduler instance.

# To run this script with a log file as an argument.
#
# Usage:
# python check_spectral_downlink.py <filename.log>

# Example usage
# python check_spectral_downlink.py "/home/h3dema/ns3-nbiot/logs/markov/u3_t20000000000_c0_e0/22_07_2025_11_57_42/w0_s1_Spw0_s1_Spectral_Downlink.log"
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot spectral downlink usage from log file.")
    parser.add_argument('-f', "--filename", type=str, help="Path to the log file containing spectral downlink data.")
    args = parser.parse_args()

    if not os.path.exists(args.filename):
        print(f"File '{args.filename}' does not exist.")
        exit(1)

    plot_downlink_usage(args.filename)
    plot_downlink_usage_imsi(args.filename)

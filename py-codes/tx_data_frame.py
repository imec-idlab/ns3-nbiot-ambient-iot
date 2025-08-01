import argparse
import re
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import pandas as pd

from utilities import find_files


def read_log(log_file):
    """
    Read a log file from the ns-3 script and parse its lines.

    The log file is expected to contain lines with the following format:
    StartTxDataFrame: time <float>, duration +<int>ns, size <int>, npackets <int>, cell <int>, node <int>

    The time is the time in seconds when the transmission starts.
    The duration is the duration of the transmission in nanoseconds.
    The size is the size of the data frame in bytes.
    The npackets is the number of packets in the data frame.
    The cell is the number of the cell used for transmission.
    The node is the number of the node which transmitted the data frame.

    The function returns a pandas DataFrame with the following columns:
    - duration_s: the duration of the transmission in seconds
    - node: the number of the node which transmitted the data frame
    - size: the size of the data frame in bytes
    - npackets: the number of packets in the data frame
    - cell: the number of the cell used for transmission
    - time: the time in seconds when the transmission starts
    """

    data = []

    # Regular expression to parse each line
    pattern = r'StartTxDataFrame: time (.*), duration \+(\d+)ns, size (\d+), npackets (\d+), cell (\d+), node (\d+)'

    # Load and parse log
    with open(log_file, 'r') as f:
        timestamp = 0  # initialize cumulative time in seconds
        for line in f:
            match = re.search(pattern, line)
            if match:
                time = float(match.group(1))
                duration_ns = int(match.group(2))
                duration_s = duration_ns / 1e9
                size = int(match.group(3))
                npackets = int(match.group(4))
                cell = int(match.group(5))
                node = int(match.group(6))

                data.append({'duration_s': duration_s, 'node': node, 'size': size, 'npackets': npackets, 'cell': cell, 'time': time})

    # Create DataFrame
    df = pd.DataFrame(data)

    return df



def plot_tx_data_frame(log_file, show=False, xmin=None, xmax=None):
    """
    Plot data frame from given log file.

    Parameters
    ----------
    log_file : str
        Name of the log file containing the data to plot.
    show : bool, optional
        If True, show the plot. Otherwise, save to a file.
    xmin : float, optional
        Minimum x-axis value (i.e. time) to plot. If None, set to 0.
    xmax : float, optional
        Maximum x-axis value (i.e. time) to plot. If None, set to max time in data.

    Returns
    -------
    None
    """
    df = read_log(log_file)

    N = len(df["node"].unique())

    fig, axs = plt.subplots(N, 2, figsize=(10, 10), sharex=True)

    for i, [node, group] in enumerate(df.groupby('node')):
        # Plot npackets
        axs[i, 0].vlines(x=group['time'], ymin=0, ymax=group['npackets'], label=f'Node {node}')
        # Plot size
        axs[i, 1].vlines(x=group['time'], ymin=0, ymax=group['size'], label=f'Node {node}')

        if i == 0:
            axs[i, 0].set_title(f'Number of Packets Over Time\nNode {node}')
            axs[i, 1].set_title(f'Packet Size (bytes) Over Time\nNode {node}')

        else:
            axs[i, 0].set_title(f'Node {node}')
            axs[i, 1].set_title(f'Node {node}')

        if i == N - 1:
            axs[i, 0].set_xlabel('Time (s)')
            axs[i, 1].set_xlabel('Time (s)')

        axs[i, 0].set_ylim(0, df["npackets"].max() + 1)
        axs[i, 1].set_ylim(0, df["size"].max() + 1)

    for ax in axs.flatten():
        # ax.legend()
        ax.legend().set_visible(False)
        ax.grid(True)
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))  # there is no fraction of packet or size

   # define the x-axis limits (window for plotting)
    _min = 0 if xmin is None else max(0.0, xmin)
    _max = df['time'].max() if xmax is None else min(df['time'].max(), xmax)
    plt.xlim(_min, _max)

    plt.tight_layout()
    if show:
        plt.show()
    else:
        img_fname = log_file.replace('.log', f'-{_min}-{_max}.png')
        print("Saved plot to", img_fname)
        plt.savefig(img_fname)
    plt.close(fig)


def plot_tx_data_frame_step(log_file, show=False, xmin=None, xmax=None):
    """
        **NOTE**: We are not using this function, because it is very slow and compute intensive.
    """
    df = read_log(log_file)

    N = len(df["node"].unique())

    fig, axs = plt.subplots(N, 2, figsize=(10, 10), sharex=True)

    for node_idx, [node, group] in enumerate(df.groupby('node')):
        # Initialize step lists
        step_times = []
        step_sizes = []
        step_npackets = []

        # Add zero at t=0 if missing
        if df.loc[0, 'time'] != 0:
            step_times.append(0)
            step_times.append(df.loc[0, 'time'])
            step_npackets.append(0)
            step_npackets.append(df.loc[0, 'npackets'])
            step_sizes.append(0)
            step_sizes.append(0)

        # Build step signal
        for i in range(len(df)):
            t_start = df.loc[i, 'time']
            s = df.loc[i, 'size']
            p = df.loc[i, 'npackets']
            t_end = t_start + df.loc[i, 'duration_s']

            # Pulse start
            step_times.append(t_start)
            step_sizes.append(s)
            step_npackets.append(p)

            # Pulse end
            step_times.append(t_end)
            step_sizes.append(s)
            step_npackets.append(p)

            # Drop to zero (if not last pulse)
            if i < len(df) - 1:
                t_next = df.loc[i + 1, 'time']
                step_times.append(t_end)
                step_sizes.append(0)
                step_npackets.append(0)

                step_times.append(t_next)
                step_sizes.append(0)
                step_npackets.append(0)

            # Plot
            axs[node_idx, 0].step(step_times, step_npackets, where='post', color='blue')
            axs[node_idx, 1].step(step_times, step_sizes, where='post', color='blue')

    for ax in axs.flatten():
        # ax.legend()
        ax.legend().set_visible(False)
        ax.grid(True)
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))  # there is no fraction of packet or size

   # define the x-axis limits (window for plotting)
    _min = 0 if xmin is None else max(0.0, xmin)
    _max = df['time'].max() if xmax is None else min(df['time'].max(), xmax)
    plt.xlim(_min, _max)

    plt.tight_layout()
    if show:
        plt.show()
    else:
        img_fname = log_file.replace('.log', f'-{_min}-{_max}.png')
        print("Saved plot to", img_fname)
        plt.savefig(img_fname)
    plt.close(fig)

"""
    This scripts reads a log file created from the NS_LOG.
    The data is generated by `LteSpectrumPhy::StartTxDataFrame`.
    You need to enable it using `ns3::LogComponentEnable ("LteSpectrumPhy", LOG_LEVEL_INFO);`.

"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument('-d', "--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument('-f', "--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")

    parser.add_argument("--min", type=float, default=None, help="Minimum time in seconds to consider for plotting")
    parser.add_argument("--max", type=float, default=None, help="Maximum time in seconds to consider for plotting")

    args = parser.parse_args()

    if args.dir is not None:
        log_files = find_files(args.dir, target_suffix="ns3_log_output.log")
        print("Found log files:")
        for file in log_files:
            print(file)

    elif args.fname is not None:
        plot_tx_data_frame(args.fname, args.show, args.min, args.max)
        # plot_tx_data_frame_step(args.fname, args.show, args.min, args.max)

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

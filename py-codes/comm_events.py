import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt

from utilities import find_files

# Plot communication events: transmitted and received data over time
# for each device (IMSI) from the perspective of the base station (BS).

def plot_comm(folder_name, show=False, xmin=None, xmax=None, accumulate=True, only_received=True):
    tx_fname = os.path.join(folder_name, "DataTrans.log")
    rx_fname = os.path.join(folder_name, "DataRecep.log")

    tx_data = pd.read_csv(tx_fname, sep=",", names=["imsi", "data_size_bytes", "time_ms"], header=0)
    rx_data = pd.read_csv(rx_fname, sep=",", names=["imsi", "data_size_bytes", "time_ms"], header=0)

    # convert time from ms to s
    tx_data['time_s'] = tx_data['time_ms'] / 1e3
    rx_data['time_s'] = rx_data['time_ms'] / 1e3

    # Sort by time to ensure proper accumulation
    tx_data = tx_data.sort_values(by=['imsi', 'time_s'])
    rx_data = rx_data.sort_values(by=['imsi', 'time_s'])

    # Compute cumulative data size per IMSI
    tx_data['cum_data'] = tx_data.groupby('imsi')['data_size_bytes'].cumsum()
    rx_data['cum_data'] = rx_data.groupby('imsi')['data_size_bytes'].cumsum()

    # Get unique IMSIs
    unique_imsis = pd.concat([tx_data['imsi'], rx_data['imsi']]).unique()
    n = len(unique_imsis)

    # Create subplots
    fig, axs = plt.subplots(nrows=n, ncols=1 if only_received else 2, figsize=(12, 4 * n), sharex=False)

    # Ensure axs is iterable
    if n == 1:
        axs = [axs]

    # Plot each IMSI's TX and RX data
    # Notice that the titles for RX are swapped to
    # reflect the correct direction from the perspective of the BS
    for i, imsi in enumerate(unique_imsis):
        tx_subset = tx_data[tx_data['imsi'] == imsi]
        rx_subset = rx_data[rx_data['imsi'] == imsi]

        if only_received:
            # RX plot
            if accumulate:
                axs[i].plot(rx_subset['time_s'], rx_subset['cum_data'], color='green')
                axs[i].set_title(f'Cumulative RX Data from IMSI {imsi}')
                axs[i].set_ylabel('Cumulative Data Size (bytes)')
            else:
                axs[i].plot(rx_subset['time_s'], rx_subset['data_size_bytes'], color='green')
                axs[i].set_title(f'RX Data from IMSI {imsi}')
                axs[i].set_ylabel('Data Size (bytes)')
            axs[i].set_xlabel('Time (s)')
            axs[i].grid(True)

        else:
            # TX plot
            if accumulate:
                axs[i][0].plot(tx_subset['time_s'], tx_subset['cum_data'], color='blue')
                axs[i][0].set_title(f'Cumulative TX Data to IMSI {imsi}')
                axs[i][0].set_ylabel('Cumulative Data Size (bytes)')
            else:
                axs[i][0].plot(tx_subset['time_s'], tx_subset['data_size_bytes'], color='blue')
                axs[i][0].set_title(f'TX Data to IMSI {imsi}')
                axs[i][0].set_ylabel('Data Size (bytes)')
            axs[i][0].set_xlabel('Time (s)')
            axs[i][0].grid(True)

            # RX plot
            if accumulate:
                axs[i][1].plot(rx_subset['time_s'], rx_subset['cum_data'], color='green')
                axs[i][1].set_title(f'Cumulative RX Data from IMSI {imsi}')
                axs[i][1].set_ylabel('Cumulative Data Size (bytes)')
            else:
                axs[i][1].plot(rx_subset['time_s'], rx_subset['data_size_bytes'], color='green')
                axs[i][1].set_title(f'RX Data from IMSI {imsi}')
                axs[i][1].set_ylabel('Data Size (bytes)')
            axs[i][1].set_xlabel('Time (s)')
            axs[i][1].grid(True)

    _min = xmin if xmin is not None else 0
    _max = xmax if xmax is not None else max(tx_data['time_s'].max(), rx_data['time_s'].max())
    plt.xlim(_min, _max)

    plt.tight_layout()
    if show:
        plt.show()
    else:
        fname = os.path.join(folder_name, "comm.png")
        if xmin is not None or xmax is not None:
            fname = fname.replace('.png', f'-{_min}-{_max}.png')
        plt.savefig(fname)
    plt.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Communication Events Log Processor")
    parser.add_argument('-s', "--search", type=str, default=None, help="Directory to search for the files")
    parser.add_argument('-d', "--dir", type=str, default=None, help="Name of the directory to process")
    parser.add_argument("--show", action="store_true", help="Show the plot (otherwise save as PNG)")

    parser.add_argument("--min", type=float, default=None, help="Minimum time in seconds to consider for plotting")
    parser.add_argument("--max", type=float, default=None, help="Maximum time in seconds to consider for plotting")

    parser.add_argument("--plot-both", action='store_false', help="Plot TX/RX separately (default is only RX)")

    args = parser.parse_args()

    if args.search is not None:
        comm_log_files = find_files(args.search, target_suffix="DataTrans.log")
        print("Found transmission log files:")
        for file in comm_log_files:
            print(file)

    elif args.dir is not None:
        plot_comm(args.dir, args.show, args.min, args.max, only_received=args.plot_both)

    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

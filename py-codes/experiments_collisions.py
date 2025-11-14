import os
import csv
import argparse
from tqdm import tqdm
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt

from rnti_imsi_map import map_rnti_imsi_from_log
from get_snr import get_experiment_snr
from get_snr import compute_weighted_average_snr


def compute_stats(counter):
    """
    Compute the minimum, maximum, and average value of a counter.

    Parameters
    ----------
    counter : dict
        A dictionary where the keys are the time stamps and the values are the counts.

    Returns
    -------
    min_val : float
        The minimum value in the counter.
    max_val : float
        The maximum value in the counter.
    avg_val : float
        The average value in the counter.
    """
    if not counter:
        return 0, 0, 0.0
    values = list(counter.values())
    return min(values), max(values), sum(values) / len(values)


def analyze_log_file(fname):
    """
    Analyze a log file and compute statistics on preamble collisions and receptions.

    Parameters
    ----------
    fname : str
        The path to the log file.

    Returns
    -------
    stats : dict
        A dictionary containing the following statistics:
        - 'total_collisions': The total number of preamble collisions.
        - 'total_received_preambles': The total number of preamble receptions.
        - 'min_collisions_per_sec': The minimum number of preamble collisions per second.
        - 'max_collisions_per_sec': The maximum number of preamble collisions per second.
        - 'avg_collisions_per_sec': The average number of preamble collisions per second.
        - 'min_received_preambles_per_sec': The minimum number of preamble receptions per second.
        - 'max_received_preambles_per_sec': The maximum number of preamble receptions per second.
        - 'avg_received_preambles_per_sec': The average number of preamble receptions per second.
    """
    collisions_per_sec = defaultdict(int)
    preambles_per_sec = defaultdict(int)
    total_collisions = 0
    total_preambles = 0

    with open(fname, 'r') as file:
        reader = csv.reader(file)
        for row in reader:
            if len(row) < 3:
                continue  # skip malformed lines, older logs may have 3 columns, newer ones 5
            event = row[1].strip()
            try:
                time_ms = int(row[2].strip())
            except ValueError:
                continue  # skip lines with invalid time
            time_sec = time_ms // 1000

            if event == 'PreambleCollision':
                collisions_per_sec[time_sec] += 1
                total_collisions += 1
            elif event == 'PreambleReceived':
                preambles_per_sec[time_sec] += 1
                total_preambles += 1

    min_coll, max_coll, avg_coll = compute_stats(collisions_per_sec)
    min_recv, max_recv, avg_recv = compute_stats(preambles_per_sec)

    return {
        'total_collisions': total_collisions,
        'total_received_preambles': total_preambles,
        'min_collisions_per_sec': min_coll,
        'max_collisions_per_sec': max_coll,
        'avg_collisions_per_sec': avg_coll,
        'min_received_preambles_per_sec': min_recv,
        'max_received_preambles_per_sec': max_recv,
        'avg_received_preambles_per_sec': avg_recv
    }


def read_data(path, verbose=False):
    collisions_dict = defaultdict(list)

    paths = [dirpath for dirpath, _, filenames in os.walk(path) if "MAC.log" in filenames]
    for dirpath in tqdm(paths):

        log_path = os.path.join(dirpath, "MAC.log")
        if verbose:
            print(f"Analyzing file: {log_path}")
        stats = analyze_log_file(log_path)

        map_fname = os.path.join(dirpath, "cell_connection.log")
        map_df = map_rnti_imsi_from_log(map_fname)
        num_unique_imsi = map_df['IMSI'].nunique()

        # find snr
        snr = get_experiment_snr(os.path.dirname(log_path), save=False)
        avg_snr = np.trunc(compute_weighted_average_snr(snr))

        stats['avg_snr'] = avg_snr

        collisions_dict[num_unique_imsi].append(stats)

    return collisions_dict


def plot_collisions_dict(collisions_dict, fname):
    # Aggregate data by avg_snr
    plot_data = defaultdict(lambda: defaultdict(list))

    for num_devices, entries in sorted(collisions_dict.items()):
        for entry in entries:
            snr = float(entry['avg_snr'])
            plot_data[snr]['num_devices'].append(int(num_devices))
            plot_data[snr]['total_collisions'].append(entry['total_collisions'])
            plot_data[snr]['total_received_preambles'].append(entry['total_received_preambles'])
            plot_data[snr]['avg_collisions_per_sec'].append(entry['avg_collisions_per_sec'])
            plot_data[snr]['avg_received_preambles_per_sec'].append(entry['avg_received_preambles_per_sec'])

    # Sort all metric lists for each snr based on num_devices
    for snr in plot_data:
        # Zip all lists together
        combined = list(zip(
            plot_data[snr]['num_devices'],
            plot_data[snr]['total_collisions'],
            plot_data[snr]['total_received_preambles'],
            plot_data[snr]['avg_collisions_per_sec'],
            plot_data[snr]['avg_received_preambles_per_sec']
        ))

        # Sort by num_devices (first element of each tuple)
        combined.sort(key=lambda x: x[0])

        # Unzip back into sorted lists
        (
            plot_data[snr]['num_devices'],
            plot_data[snr]['total_collisions'],
            plot_data[snr]['total_received_preambles'],
            plot_data[snr]['avg_collisions_per_sec'],
            plot_data[snr]['avg_received_preambles_per_sec']
        ) = map(list, zip(*combined))

    # Create 2x2 grid of plots
    fig, axs = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Metrics vs Number of Devices by avg_snr')

    # Plot each metric
    for snr, metrics in plot_data.items():
        axs[0, 0].plot(metrics['num_devices'], metrics['total_collisions'], label=f'SNR={snr}')
        axs[0, 1].plot(metrics['num_devices'], metrics['total_received_preambles'], label=f'SNR={snr}')
        axs[1, 0].plot(metrics['num_devices'], metrics['avg_collisions_per_sec'], label=f'SNR={snr}')
        axs[1, 1].plot(metrics['num_devices'], metrics['avg_received_preambles_per_sec'], label=f'SNR={snr}')

    # Set titles and labels
    axs[0, 0].set_title('Total Collisions vs Num Devices')
    axs[0, 1].set_title('Total Received Preambles vs Num Devices')
    axs[1, 0].set_title('Avg Collisions/sec vs Num Devices')
    axs[1, 1].set_title('Avg Received Preambles/sec vs Num Devices')

    for ax in axs.flat:
        ax.set_xlabel('Num Devices')
        ax.set_ylabel('Value')
        ax.legend()
        ax.grid(True)

    plt.tight_layout()
    plt.savefig(fname)
    plt.close()


if __name__ == "__main__":
    import json

    parser = argparse.ArgumentParser(description="Compute collisions from experiments files.")
    parser.add_argument("-p", "--path", type=str, default="./logs", help="Root directory to search for log files")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    args = parser.parse_args()

    collisions_dict = read_data(args.path, args.verbose)
    print(collisions_dict)
    with open(os.path.join(args.path, "collisions_stats.json"), "w") as f:
        json.dump(collisions_dict, f, indent=4)

    # with open(os.path.join(args.path, "collisions_stats.json"), "r") as f:
    #     collisions_dict = json.load(f)

    plot_collisions_dict(collisions_dict, fname=os.path.join(args.path, "collisions_experiments.png"))

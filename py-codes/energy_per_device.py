"""
Compute energy per device from nbiot_energy.log files.

- Recursively finds all nbiot_energy.log files.
- Reads each file, calculates total energy and divides by number of unique IMSIs.
- Stores results in a dictionary:
  - Key: Number of unique IMSIs
  - Value: List of energy per device
"""
import os
import argparse
import numpy as np
import pandas as pd
from collections import defaultdict
import matplotlib.pyplot as plt

from get_snr import get_experiment_snr
from get_snr import compute_weighted_average_snr


# define the state_map if needed for plotting
state_map = {
    0: "IDLE",
    1: "RX",
    2: "TX",
    3: "SLEEP",
    # Add more mappings as needed
}

def compute_energy_per_device(log_file):
    df = pd.read_csv(log_file, header=None, names=["Time", "IMSI", "State", "Energy"])
    df['StateName'] = df['State'].map(state_map)
    df['Time'] = df['Time'] / 1000  # Convert ms to seconds

    unique_devices = df['IMSI'].nunique()
    total_energy = df['Energy'].sum()
    energy_per_device = float(total_energy) / unique_devices if unique_devices > 0 else 0.0

    # find snr
    snr = get_experiment_snr(os.path.dirname(log_file), save=False)
    avg_snr = compute_weighted_average_snr(snr)

    return unique_devices, energy_per_device, avg_snr


def read_data(path, verbose):
    energy_dict = defaultdict(list)

    for dirpath, _, filenames in os.walk(path):
        if "nbiot_energy.log" in filenames:
            log_path = os.path.join(dirpath, "nbiot_energy.log")
            try:
                num_devices, energy_per_device, avg_snr = compute_energy_per_device(log_path)
                energy_dict[num_devices].append({"avg_snr": avg_snr, "energy": energy_per_device})
                if verbose:
                    print(f"Processed {log_path}: {num_devices} devices, {energy_per_device:.4f} J/device Average SNR: {avg_snr:.2f} dB")
            except Exception as e:
                print(f"Error processing {log_path}: {e}")

    if verbose:
        print("\nEnergy per device summary:")
        for devices, values in sorted(energy_dict.items()):
            print(f"{devices} devices: {values}")

    return energy_dict


def plot_energy_dict(energy_dict, fname="energy_per_device.png"):
    # Organize data by avg_snr
    snr_data = {}
    for num_devices, entries in energy_dict.items():
        for entry in entries:
            snr = entry['avg_snr']
            energy = entry['energy']
            if snr not in snr_data:
                snr_data[snr] = {'x': [], 'y': []}
            snr_data[snr]['x'].append(num_devices)
            snr_data[snr]['y'].append(energy)

    # Plotting
    plt.figure(figsize=(8, 6))
    for snr, data in snr_data.items():
        plt.plot(data['x'], data['y'], marker='o', label=f'avg_snr = {snr}')

    plt.xlabel('Number of Devices')
    plt.ylabel('Energy per Device')
    plt.title('Energy per Device vs Number of Devices by avg_snr')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(fname)
    plt.close()


def main():
    parser = argparse.ArgumentParser(description="Compute energy per device from nbiot_energy.log files.")
    parser.add_argument("-p", "--path", type=str, default="./logs", help="Root directory to search for log files")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    args = parser.parse_args()

    energy_dict = read_data(args.path, args.verbose)
    plot_energy_dict(energy_dict, fname=os.path.join(args.path, "energy_per_device.png"))


if __name__ == "__main__":
    main()

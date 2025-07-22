import argparse
import os
import re
import pandas as pd
import matplotlib.pyplot as plt


def find_energy_files(directory: str, target_suffix = "Energy.log") -> list:

    files_found = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(target_suffix):
                rel_path = os.path.relpath(os.path.join(root, file))
                files_found.append(rel_path)
    return files_found


def process_energy_file(file_path: str) -> pd.DataFrame:
    pattern = re.compile(
        r"(?P<time>\d+\.\d+)s\s+\[/NodeList/(?P<node>\d+)/ApplicationList/\d+/State\]\s+State changed from (?P<old>\d+) to (?P<new>\d+)"
    )

    data = []

    # Read and parse the file
    with open(file_path, "r") as f:
        for line in f:
            fields = line.strip().split(',')
            time_ns = float(fields[0].replace('+', '').replace('ns', ''))
            time_s = time_ns / 1e9  # Convert ns to seconds
            imsi = int(fields[1])
            ce_level = int(fields[2])
            energy = float(fields[3])
            fraction = float(fields[4])
            data.append([time_s, imsi, ce_level, energy, fraction])

    # Create a DataFrame
    df = pd.DataFrame(data, columns=['time_s', 'imsi', 'ce_level', 'energy', 'fraction'])

    return df


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument("--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument("--fname", type=str, default=None, help="Name of the file to process")
    parser.add_argument("--plot-fname", type=str, default="energy.png", help="Output plot file name")
    parser.add_argument("--show", action="store_true", help="Show the plot")
    args = parser.parse_args()

    if args.dir is not None:
        state_change_files = find_energy_files(args.dir)
        print("Found state change files:")
        for file in state_change_files:
            print(file)

    elif args.fname is not None:
        df = process_energy_file(args.fname)

        # Plot energy vs. time for each device
        plt.figure(figsize=(10, 6))
        for imsi_id, group in df.groupby('imsi'):
            plt.plot(group['time_s'], group['energy'], label=f'Device {imsi_id}')

        plt.xlabel("Time (seconds)")
        plt.ylabel("Energy Remaining")
        plt.title("Energy vs Time per Device (IMSI)")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()

        if args.show:
            plt.show()
        else:
            plt.savefig(args.plot_fname)


    else:
        print("Please provide either a directory or a file name to process.")
        parser.print_help()
        exit(1)

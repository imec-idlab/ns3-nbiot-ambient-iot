import os
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import re

# Define regex pattern to extract numbers
pattern = r"GenericCapacitor:Remaining energy = ([\d\.]+) with V: ([\d\.]+) at ([\d\.]+)"


def get_opts():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--filename", default="scenario.log", help="the file to parse")
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    args = get_opts()
    if not os.path.isfile(args.filename):
        print(f"File {args.filename} does not exist")
        exit(1)

    # Initialize an empty list to store extracted data
    data = []

    # Read the log file
    with open(args.filename, "r") as file:
        for line in file:
            match = re.search(pattern, line)
            if match:
                energy, voltage, time = map(float, match.groups())
                data.append([time, energy, voltage])

    # Create a pandas DataFrame
    df = pd.DataFrame(data, columns=["Time", "Energy", "Voltage"])

    # Sort the DataFrame by time
    df = df.sort_values("Time")

    # Plot the graph
    plt.figure(figsize=(10, 5))
    ax1 = plt.gca()
    ax1.plot(df["Time"], df["Energy"], label="Energy", color="blue")
    ax1.set_ylabel("Energy (J)")

    ax2 = plt.twinx()
    ax2.plot(df["Time"], df["Voltage"], label="Voltage", color="red")
    ax2.set_ylabel("Voltage (V)")

    plt.xlabel("Time")

    plt.title("Evolution over Time")
    plt.legend()
    plt.grid(True)
    plt.show()
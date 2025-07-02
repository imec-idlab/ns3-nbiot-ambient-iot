import argparse
import re
import pandas as pd
import matplotlib.pyplot as plt


def get_opts():
    parser = argparse.ArgumentParser(description="Process log file.")
    parser.add_argument('-f', '--filename', type=str, default='nb-scenario2.log', help='Log file to process')
    parser.add_argument('--save', action='store_true', help='save plots')
    parser.add_argument('--show-markers', action='store_true', help='show markers on the plot')
    return parser.parse_args()


if __name__ == '__main__':
    args = get_opts()


    # Read the log file
    with open(args.filename, "r") as file:
        lines = file.readlines()

    # Data storage
    data = {}

    # Regex patterns
    capacitor_pattern = re.compile(r"GenericCapacitor.*Remaining energy: ([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?) J, V: ([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?) V, at (\d+\.?\d*) s")
    energy_source_pattern = re.compile(r"EnergySource.*Total harvested power = ([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?), Total harvester current = ([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?) Total current consumed = (\d+) at (\d+\.?\d*) s")

    # Extract information
    for line in lines:
        capacitor_match = capacitor_pattern.search(line)
        energy_source_match = energy_source_pattern.search(line)

        if capacitor_match:
            energy, voltage, time = map(float, capacitor_match.groups())
            data[time] = data.get(time, {})
            data[time].update({"Energy (J)": energy, "Voltage (V)": voltage})

        if energy_source_match:
            harvested_power, harvester_current, consumed_current, time = map(float, energy_source_match.groups())
            data[time] = data.get(time, {})
            data[time].update({
                "Harvested Power (W)": harvested_power,
                "Harvester Current (A)": harvester_current,
                "Consumed Current (A)": consumed_current
            })

    # Convert to DataFrame and sort
    df = pd.DataFrame.from_dict(data, orient="index").sort_index()
    df.index.name = "Time (s)"

    print("Data collected from log file:", args.filename)
    print(df)

    # Plot graphs
    fig, axes = plt.subplots(3, 2, figsize=(12, 8))
    variables = ["Energy (J)", "Voltage (V)", "Harvested Power (W)", "Harvester Current (A)", "Consumed Current (A)"]

    marker = "o" if args.show_markers else None
    for i, var in enumerate(variables):
        ax = axes[i // 2, i % 2]
        ax.plot(df.index, df[var], marker=marker, linestyle="-")
        ax.set_title(var)
        ax.set_xlabel("Time (s)")
        ax.set_ylabel(var)
        ax.grid(True)

    fig.delaxes(axes[2, 1])

    plt.tight_layout()
    if args.save:
        plt.savefig("process_log.png")
    else:
        plt.show()

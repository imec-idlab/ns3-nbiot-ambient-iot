import argparse
import re
import matplotlib.pyplot as plt
from collections import defaultdict


def get_opts():
    parser = argparse.ArgumentParser(description="Parse energy log and visualize state and energy changes.")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--plot", dest="plot", action="store_true", help="Enable plotting (default)")
    group.add_argument("--no-plot", dest="plot", action="store_false", help="Disable plotting")
    parser.set_defaults(plot=True)

    parser.add_argument("--log-fname", type=str, default="scenario3.log", help="Path to the log file")

    args = parser.parse_args()
    return args


def compute_deltas(device_id: str, state_times: list, state_values: list, verbose=True):
    """
    Compute delta0 and delta1 estimates based on state transitions.
    :param state_times: List of times when states change.
    :param state_values: List of state values (0 or 1) corresponding to the times.
    :return: None
    """

    SCALE_TO_INT = 100  # Scale factor to convert seconds to integer for state representation
    complete_length = state_times[-1] * SCALE_TO_INT + 1
    complete_state = []

    for i in range(len(state_times)):
        start_time = state_times[i] * SCALE_TO_INT
        end_time = state_times[i + 1] * SCALE_TO_INT if i + 1 < len(state_times) else complete_length
        complete_state.extend([state_values[i]] * int(end_time - start_time))

    states_before = complete_state[:-1]
    states_after = complete_state[1:]

    transitions = len(state_times)

    active_total = sum(complete_state)
    inactive_total = len(complete_state) - active_total

    active_to_inactive = sum(1 for before, after in zip(states_before, states_after) if before == 1 and after == 0)
    inactive_to_active = sum(1 for before, after in zip(states_before, states_after) if before == 0 and after == 1)

    delta0_est = inactive_to_active / inactive_total if inactive_total > 0 else 0
    delta1_est = active_to_inactive / active_total if active_total > 0 else 0

    if verbose:
        print("Device ID:", device_id)
        print("delta0:", delta0_est)
        print("delta1:", delta1_est)
        print()

    return delta0_est, delta1_est

if __name__ == "__main__":
    args = get_opts()

    # File path or raw log string
    with open(args.log_fname, "r") as file:
        # Read the log data from a file
        log_data = file.read()

    # Regular expressions to extract required info
    state_change_pattern = re.compile(r"EnergyMarkov\((\d+)\): State changed from (\d) to (\d) at ([\d.]+) s")
    energy_decrease_pattern = re.compile(r"EnergyMarkov\((\d+)\): Decrease remaining energy(?: by)? ([\d.eE+-]+) at ([\d.]+) s")

    # Containers
    state_times = defaultdict(list)
    state_values = defaultdict(list)
    energy_times = defaultdict(list)
    energy_decreases = defaultdict(list)

    # State tracking
    current_state = 0

    for line in log_data.splitlines():
        state_match = state_change_pattern.search(line)
        energy_match = energy_decrease_pattern.search(line)

        if state_match:
            device_id, from_state, to_state, time = state_match.groups()
            current_state = int(to_state)
            state_times[device_id].append(float(time))
            state_values[device_id].append(current_state)

        if energy_match:
            device_id, energy, time = energy_match.groups()
            energy_times[device_id].append(float(time))
            energy_decreases[device_id].append(float(energy))


    print("Found device IDs:", list(state_times.keys()))
    # Calculate cumulative energy

    if args.plot:
        num_devices = len(state_times)
        fig, axes = plt.subplots(num_devices, 1, figsize=(10, 4 * num_devices), sharex=True)

        if num_devices == 1:
            axes = [axes]  # Ensure axes is iterable

    for idx, device_id in enumerate(state_times.keys()):
        delta0, delta1 = compute_deltas(device_id, state_times[device_id], state_values[device_id])

        if args.plot:
            cumulative_energy = []
            total_energy = 0
            for energy in energy_decreases[device_id]:
                total_energy += energy
                cumulative_energy.append(total_energy)

            ax1 = axes[idx]

            # Plot cumulative energy
            ax1.plot(energy_times[device_id], cumulative_energy, 'b-', label='Cumulative Energy')
            ax1.set_xlabel('Time (s)')
            ax1.set_ylabel('Cumulative Requested Energy', color='b')
            ax1.tick_params(axis='y', labelcolor='b')

            # Plot state on secondary axis
            ax2 = ax1.twinx()
            ax2.step(state_times[device_id], state_values[device_id], 'r-', where='post', label='State', alpha=0.5)
            ax2.set_ylabel('State', color='r')
            ax2.tick_params(axis='y', labelcolor='r')

            ax2.set_yticks([0, 1])
            ax2.set_yticklabels(['inactive', 'active'])

            plt.gca().text(0.05, 0.95,
                        f"$\delta_0$: {delta0:.2f}, $\delta_1$: {delta1:.2f}",
                        fontsize=12, verticalalignment='top')

            plt.title('State and Cumulative Energy Over Time - Device {}'.format(device_id))
            plt.tight_layout()
            plt.grid(True)

    if args.plot:
        plt.show()

    print("Done processing log file.")

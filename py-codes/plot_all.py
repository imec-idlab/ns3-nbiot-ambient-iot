import os
import sys
import argparse
from check_energy import find_files
from check_energy import plot_energy
from nbiot_energy import plot_energy_usage, plot_energy_usage_consolidated
from check_spectral_downlink import plot_downlink_usage, plot_downlink_usage_imsi
from check_spectral_uplink import plot_uplink_usage, plot_uplink_usage_imsi
from collisions import plot_collisions
from mac_events import plot_mac_events, plot_mac_events_imsi
from state_changes import plot_state_changes


if __name__ == "__main__":
    # Ensure the directory of this script is in the system path
    current_dir = os.path.dirname(os.path.abspath(__file__))
    if current_dir not in sys.path:
        sys.path.append(current_dir)

    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument('-d', "--dir", type=str, default=None, required=True, help="Directory to search for the files")
    parser.add_argument('-s', "--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    if not os.path.exists(args.dir) or not os.path.isdir(args.dir):
        print(f"Directory '{args.dir}' does not exist.")
        exit(1)

    print("Processing energy files in directory:", args.dir)
    energy_files = find_files(args.dir, target_suffix="Energy.log")
    for filename in energy_files:
        print("- Processing file:", filename)
        plot_energy(filename, show=args.show)

    print("Processing NB-IoT energy files in directory:", args.dir)
    energy_files = find_files(args.dir, target_suffix="nbiot_energy.log")
    for filename in energy_files:
        print("- Processing file:", filename)
        plot_energy_usage(filename, show=args.show, use_cumulative=True)
        plot_energy_usage(filename, show=args.show, use_cumulative=False)
        plot_energy_usage_consolidated(filename, show=args.show, use_cumulative=True)
        plot_energy_usage_consolidated(filename, show=args.show, use_cumulative=False)

    # print("Processing spectral downlink files in directory:", args.dir)
    # spectral_downlink_files = find_files(args.dir, target_suffix="Spectral_Downlink.log")
    # for filename in spectral_downlink_files:
    #     print("- Processing file:", filename)
    #     plot_downlink_usage(filename, show=args.show)
    #     plot_downlink_usage_imsi(filename)

    # print("Processing spectral uplink files in directory:", args.dir)
    # spectral_uplink_files = find_files(args.dir, target_suffix="Spectral_Uplink.log")
    # for filename in spectral_uplink_files:
    #     print("- Processing file:", filename)
    #     plot_uplink_usage(filename, show=args.show)
    #     plot_uplink_usage_imsi(filename)

    # print("Processing collision in files in directory:", args.dir)
    # collision_files = find_files(args.dir, target_suffix="_MAC.log")
    # for filename in collision_files:
    #     print("- Processing file:", filename)
    #     plot_collisions(filename, show=args.show)

    # print("Processing state changes in directory:", args.dir)
    # state_change_files = find_files(args.dir, target_suffix="state-changes.log")
    # for filename in state_change_files:
    #     print("- Processing file:", filename)
    #     plot_state_changes(filename, show=args.show)

    # print("Processing UE mac events in directory:", args.dir)
    # collision_files = find_files(args.dir, target_suffix="ueMAC.log")
    # for filename in collision_files:
    #     print("- Processing file:", filename)
    #     plot_mac_events(filename, show=args.show)

    # print("Processing eNB mac events in directory:", args.dir)
    # collision_files = find_files(args.dir, target_suffix="_MAC.log")
    # for filename in collision_files:
    #     print("- Processing file:", filename)
    #     plot_mac_events(filename, show=args.show)
    #     plot_mac_events_imsi(filename, show=args.show)

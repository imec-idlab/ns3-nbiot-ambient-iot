import argparse
from check_energy import find_files
from check_energy import plot_energy
from check_spectral_downlink import plot_downlink_usage
from check_spectral_uplink import plot_uplink_usage
from collisions import plot_collisions
from mac_events import plot_mac_events
from state_changes import plot_state_changes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Energy Changes")
    parser.add_argument('-d', "--dir", type=str, default=None, help="Directory to search for the files")
    parser.add_argument('-s', "--show", action="store_true", help="Show the plot (otherwise save as PNG)")
    args = parser.parse_args()

    print("Processing energy files in directory:", args.dir)
    energy_files = find_files(args.dir, target_suffix="Energy.log")
    for filename in energy_files:
        plot_energy(filename, show=args.show)

    print("Processing spectral downlink files in directory:", args.dir)
    spectral_downlink_files = find_files(args.dir, target_suffix="Spectral_Downlink.log")
    for filename in spectral_downlink_files:
        plot_downlink_usage(filename, show=args.show)

    print("Processing spectral uplink files in directory:", args.dir)
    spectral_uplink_files = find_files(args.dir, target_suffix="Spectral_Uplink.log")
    for filename in spectral_uplink_files:
        plot_uplink_usage(filename, show=args.show)

    print("Processing collision in files in directory:", args.dir)
    collision_files = find_files(args.dir, target_suffix="_MAC.log")
    for filename in collision_files:
        plot_collisions(filename, show=args.show)

    print("Processing eNB mac events in directory:", args.dir)
    collision_files = find_files(args.dir, target_suffix="_MAC.log")
    for filename in collision_files:
        plot_mac_events(filename, show=args.show)

    print("Processing UE mac events in directory:", args.dir)
    collision_files = find_files(args.dir, target_suffix="ueMAC.log")
    for filename in collision_files:
        plot_mac_events(filename, show=args.show)

    print("Processing state changes in directory:", args.dir)
    state_change_files = find_files(args.dir, target_suffix="state-changes.log")
    for filename in state_change_files:
        plot_state_changes(filename, show=args.show)

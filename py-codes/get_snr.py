import os
import pickle
from collections import defaultdict
import pandas as pd

from parse_ue_measurements import parse_measurements
from read_noisefigure_log import parse_noise_figure, NoisePower
from rnti_imsi_map import map_rnti_imsi_from_log


def compute_snr_per_entry(data, noisefigure: dict, df_mapping: pd.DataFrame):
    """
    Compute the SNR (in dB) for each entry in the given data.

    This function takes in the parsed UE measurements data, the noise figure configuration, and a mapping of RNTI to IMSI.
    It returns a list of dictionaries, each containing the time, RNTI, IMSI, RSRP, noise power, and SNR for each entry.

    :param data: The parsed UE measurements data
    :param noisefigure: The noise figure configuration
    :param df_mapping: A mapping of RNTI to IMSI
    :return: A list of dictionaries, each containing the time, RNTI, IMSI, RSRP, noise power, and SNR for each entry
    """
    snr_results = []

    # Use eNB's DL bandwidth for noise calculation
    bandwidth_mhz = noisefigure["eNB"].get("Bandwidth DL", 1.0)
    bandwidth_hz = bandwidth_mhz * 1e6

    for entry in data:
        rnti = entry["RNTI"]
        rsrp_dbm = entry["RSRP"]
        time = entry["Time"]

        # Find IMSI from RNTI
        try:
            imsi = int(df_mapping[df_mapping["RNTI"] == rnti]["IMSI"].values[0])
        except IndexError:
            # RNTI not found in mapping because it's not from a UE
            continue

        # Notice that IMSI starts in 1, and noisefigure["UE"] starts in 0
        ue_params = noisefigure["UE"][imsi - 1]
        np = NoisePower(noise_figure_db=ue_params["NoiseFigure"], bandwidth_hz=bandwidth_hz)
        noise_dbm = np.get_noise_power_dbm()

        snr_db = rsrp_dbm - noise_dbm
        snr_results.append({
            "Time": time,
            "RNTI": rnti,
            "IMSI": imsi,
            "RSRP_dBm": rsrp_dbm,
            "Noise_dBm": noise_dbm,
            "SNR_dB": snr_db
        })

    return snr_results



def experiment_snr(path_log, verbose=False, save=True):
    uelogfile = os.path.join(path_log, "ReportUeMeasurements.log")
    noise_figure_file = os.path.join(path_log, "NoiseFigure.log")
    connection_log_file = os.path.join(path_log, "cell_connection.log")

    data = parse_measurements(uelogfile)
    noisefigure = parse_noise_figure(noise_figure_file)
    df_mapping = map_rnti_imsi_from_log(connection_log_file)  # pandas dataframe
    # print(df_mapping)

    snr_entries = compute_snr_per_entry(data, noisefigure, df_mapping)

    # # print all entries
    # for e in snr_entries:
    #     print(f"Time {e['Time']:.1f}s | IMSI {e['IMSI']} | RSRP: {e['RSRP_dBm']:.2f} dBm | Noise: {e['Noise_dBm']:.2f} dBm | SNR: {e['SNR_dB']:.2f} dB")

    # Group entries by IMSI to find constant SNR intervals
    grouped = defaultdict(list)
    for entry in snr_entries:
        grouped[entry["IMSI"]].append(entry)

    # Find constant SNR intervals
    intervals = dict()
    for imsi, records in grouped.items():
        if imsi not in intervals:
            intervals[imsi] = []
        # print(f"IMSI {imsi}:")
        start_time = records[0]["Time"]
        current_snr = records[0]["SNR_dB"]

        for i in range(1, len(records)):
            if records[i]["SNR_dB"] != current_snr:
                end_time = records[i - 1]["Time"]
                print(f"  SNR {current_snr} dB from {start_time}s to {end_time}s")
                intervals[imsi].append((start_time, end_time, current_snr))
                # update the pointers
                start_time = records[i]["Time"]
                current_snr = records[i]["SNR_dB"]

        # Print the last interval
        end_time = records[-1]["Time"]
        intervals[imsi].append((start_time, end_time, current_snr))
        # print(f"  SNR {current_snr} dB from {start_time}s to {end_time}s")

    if verbose:
        print(intervals)

    if save:
        with open(os.path.join(path_log, "snr_intervals.pickle"), "wb") as f:
            pickle.dump(intervals, f, protocol=pickle.DEFAULT_PROTOCOL)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Parse connection log file (map RNTI to IMSI).")
    parser.add_argument('-p', '--path-log', type=str, help="Path to the log files")
    args = parser.parse_args()

    experiment_snr(args.path_log, save=True)

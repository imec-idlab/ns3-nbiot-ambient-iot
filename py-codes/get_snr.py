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
        imsi = df_mapping[df_mapping["RNTI"] == rnti]["IMSI"].values[0]

        ue_params = noisefigure["UE"][imsi]
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



if __name__ == "__main__":
    import os
    import argparse
    parser = argparse.ArgumentParser(description="Parse connection log file (map RNTI to IMSI).")
    parser.add_argument('-p', '--path-log', type=str, help="Path to the log files")
    args = parser.parse_args()

    uelogfile = os.path.join(args.path_log, "ReportUeMeasurements.log")
    noise_figure_file = os.path.join(args.path_log, "NoiseFigure.log")
    connection_log_file = os.path.join(args.path_log, "cell_connection.log")

    data = parse_measurements(uelogfile)
    import pdb; pdb.set_trace()
    noisefigure = parse_noise_figure(noise_figure_file)
    # df_mapping = map_rnti_imsi_from_log(connection_log_file)  # pandas dataframe
    df_mapping = pd.DataFrame([{"IMSI": 1, "RNTI": 0, "CellId": 0}])

    snr_entries = compute_snr_per_entry(data, noisefigure, df_mapping)

    for e in snr_entries:
        print(f"Time {e['Time']:.1f}s | IMSI {e['IMSI']} | RSRP: {e['RSRP_dBm']:.2f} dBm | Noise: {e['Noise_dBm']:.2f} dBm | SNR: {e['SNR_dB']:.2f} dB")

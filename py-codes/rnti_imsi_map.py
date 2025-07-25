import argparse
import re
import pandas as pd


# Load and parse the log file
def map_rnti_imsi_from_log(filepath):
    """
    Parse a connection log file and return a pandas DataFrame containing the mapping from RNTI to IMSI.

    Parameters
    ----------
    filepath : str
        Path to the connection log file.

    Returns
    -------
    pandas.DataFrame
        A DataFrame with columns 'IMSI', 'CellId', and 'RNTI', containing the mapping from RNTI to IMSI.
    """
    data = []
    with open(filepath, "r") as file:
        for line in file:
            match = re.search(r'IMSI: (\d+) CellId: (\d+) RNTI: (\d+)', line)
            if match:
                imsi = int(match.group(1))
                cellid = int(match.group(2))
                rnti = int(match.group(3))
                data.append({'IMSI': imsi, 'CellId': cellid, 'RNTI': rnti})
    return pd.DataFrame(data)


# Usage example
# python rnti_imsi_map.py -f "../logs/markov/u3_t60000000000_c0_e0/25_07_2025_11_35_44/w0_s1_cell_connection.log"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Parse connection log file (map RNTI to IMSI).")
    parser.add_argument('-f', '--logfile', type=str, help="Path to the connection log file")
    args = parser.parse_args()

    df = map_rnti_imsi_from_log(args.logfile)
    print(df)

import re


def parse_measurements(file_path):
    """
    Parse a ReportUeMeasurements log file and extract UE measurements.

    The log file is expected to contain lines in the following format:

    ReportUeMeasurements RNTI: <RNTI> CellId: <CellId> RSRP: <RSRP> RSRQ: <RSRQ> ComponentCarrierID: <ComponentCarrierID> Time: <Time>

    The function returns a list of dictionaries containing the parsed UE measurements.

    :param file_path: The path to the ReportUeMeasurements log file to parse.
    :return: A list of dictionaries containing the parsed UE measurements.
    """
    measurements = []

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            match = re.match(
                r'^ReportUeMeasurements RNTI: (\d+) CellId: (\d+) RSRP: ([\d\.\-]+) RSRQ: ([\d\.\-nan]+) ComponentCarrierID: ([\d\-]+) Time: (\d+)',
                line
            )
            if match:
                rnti, cell_id, rsrp, rsrq, ccid, time_ms = match.groups()

                entry = {
                    "RNTI": int(rnti),
                    "CellId": int(cell_id),
                    "RSRP": float(rsrp),
                    "RSRQ": None if rsrq == "-nan" else float(rsrq),
                    "ComponentCarrierID": None if ccid == "-1" else int(ccid),
                    "Time": int(time_ms) / 1000.0  # convert ms to seconds
                }
                measurements.append(entry)

    return measurements


if __name__ == "__main__":
    import argparse
    # Example usage
    parser = argparse.ArgumentParser(description="Parse ReportUeMeasurements log file")
    parser.add_argument('-f', '--logfile', type=str, required=True, help="Path to the ReportUeMeasurements log file")
    args = parser.parse_args()

    # Example usage
    data = parse_measurements(args.logfile)
    for entry in data:
        print(entry)

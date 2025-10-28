import re
import math


class NoisePower:
    def __init__(self, noise_figure_db, bandwidth_hz=1e6):
        """
        Initialize a NoisePower object with the given noise figure (dB) and optional bandwidth (Hz).

        :param noise_figure_db: The noise figure in dB configured as a command line parameter in ns3
        :param bandwidth_hz: The bandwidth in Hz (default is 1 MHz)
        """
        self.noise_figure_db = noise_figure_db
        self.bandwidth_hz = bandwidth_hz  # Default to 1 MHz if not overridden

    def get_noise_power_w(self, bandwidth_hz=None):
        """
        Compute the noise power (W) given the noise figure (dB) and an optional bandwidth (Hz).

        If the bandwidth is not provided, the default bandwidth specified when the object was created is used.

        The noise power is computed as k * T * B * F, where k is the Boltzmann constant, T is the reference temperature,
        B is the bandwidth, and F is the noise figure in linear units.

        :param bandwidth_hz: Optional bandwidth in Hz
        :return: Noise power in W
        """
        B = bandwidth_hz if bandwidth_hz and bandwidth_hz > 0.0 else self.bandwidth_hz
        k = 1.38064852e-23  # Boltzmann constant [J/K]
        T = 290.0           # Reference temperature [K]
        F = 10 ** (self.noise_figure_db / 10.0)
        return k * T * B * F

    def get_noise_power_dbm(self, bandwidth_hz=None):
        """
        Compute the noise power (dBm) given the noise figure (dB) and an optional bandwidth (Hz).

        If the bandwidth is not provided, the default bandwidth specified when the object was created is used.

        The noise power is computed as 10 * log10(noise power in W) + 30, where the noise power in W is computed as k * T * B * F,
        where k is the Boltzmann constant, T is the reference temperature, B is the bandwidth, and F is the noise figure in linear units.

        :param bandwidth_hz: Optional bandwidth in Hz
        :return: Noise power in dBm
        """
        noise_w = self.get_noise_power_w(bandwidth_hz)
        return 10.0 * math.log10(noise_w) + 30.0  # Convert W → dBm


def parse_noise_figure(file_path):
    """
    Parse a Noisefigure log file and extract UE and eNB radio configuration.

    The log file is expected to contain lines in the following format:

    UE <IMSI> NoiseFigure: <Noise Figure> TxPower: <Tx Power>
    eNB NoiseFigure: <Noise Figure> TxPower: <Tx Power>
    eNB Bandwidth DL: <Bandwidth DL> UL: <Bandwidth UL>

    The function returns a dictionary with the following structure:

    {
        "UE": {
            <IMSI>: {
                "NoiseFigure": <float>,
                "TxPower": <float>
            }
        },
        "eNB": {
            "NoiseFigure": <float>,
            "TxPower": <float>,
            "Bandwidth UL": <float>,
            "Bandwidth DL": <float>
        }
    }

    :param file_path: The path to the Noisefigure log file to parse.
    :return: A dictionary containing the parsed UE and eNB radio configuration.
    """
    result = {"UE": {}, "eNB": {}}

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()

            # Match UE lines
            ue_match = re.match(r'^UE (\d+) NoiseFigure: (\d+) TxPower: (\d+)', line)
            if ue_match:
                imsi, nf, tx = ue_match.groups()
                result["UE"][int(imsi)] = {
                    "NoiseFigure": float(nf),
                    "TxPower": float(tx)
                }
                continue

            # Match eNB NoiseFigure and TxPower
            enb_match = re.match(r'^eNB NoiseFigure: (\d+) TxPower: (\d+)', line)
            if enb_match:
                nf, tx = enb_match.groups()
                result["eNB"]["NoiseFigure"] = float(nf)
                result["eNB"]["TxPower"] = float(tx)
                continue

            # Match eNB Bandwidth
            bw_match = re.match(r'^eNB Bandwidth DL: (\d+) UL: (\d+)', line)
            if bw_match:
                dl, ul = bw_match.groups()
                result["eNB"]["Bandwidth UL"] = float(ul)
                result["eNB"]["Bandwidth DL"] = float(dl)

    return result


if __name__ == "__main__":
    import argparse
    # Example usage
    parser = argparse.ArgumentParser(description="Parse NoiseFigure log file")
    parser.add_argument('-f', '--logfile', type=str, required=True, help="Path to the NoiseFigure log file")
    args = parser.parse_args()

    # Parse the Noisefigure log file
    config = parse_noise_figure(args.logfile)
    print(config)
    # {'UE': {0: {'NoiseFigure': 7.0, 'TxPower': 10.0}, 1: {'NoiseFigure': 7.0, 'TxPower': 10.0}, 2: {'NoiseFigure': 7.0, 'TxPower': 10.0}, 3: {'NoiseFigure': 7.0, 'TxPower': 10.0}}, 'eNB': {'NoiseFigure': 10.0, 'TxPower': 30.0, 'Bandwidth UL': 25.0, 'Bandwidth DL': 1.0}}

    # Set bandwidth (Hz) for all UEs
    bandwidth_hz = config["eNB"]["Bandwidth DL"]  # 1 MHz

    print(f"UE Noise Power (dBm) @ {bandwidth_hz} MHz:")
    for imsi, ue_params in config["UE"].items():
        np = NoisePower(noise_figure_db=ue_params["NoiseFigure"], bandwidth_hz=bandwidth_hz)
        noise_dbm = np.get_noise_power_dbm()
        print(f"- UE {imsi}: {noise_dbm:.2f} dBm")

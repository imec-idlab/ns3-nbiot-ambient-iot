import argparse
import os
import sys
import re
from collections import defaultdict

"""
Relationship Between BER and PER
The following equation demonstrates the mathematical relationship between BER and PER,
assuming an ideal communication system over a binary symmetric channel with uncorrelated noise:

per = 1 - (1 - ber)^(n)

n is the number of bits in a packet.
"""



def parse_log(log_fname):
    # Regular expressions for transmit and receive lines
    tx_pattern = re.compile(r"LteUeMac::DoTransmitPdu RNTI: (\d+), Id: (\d+), Size: (\d+) bytes, \d+ bytes")
    rx_pattern = re.compile(r"LteEnbMac::DoReceivePhyPdu RNTI: (\d+), Id: (\d+), Size: (\d+) bytes, \d+ bytes")

    tx_dict = {}
    rx_dict = {}

    with open(log_fname, 'r') as f:
        for line in f:
            tx_match = tx_pattern.search(line)
            rx_match = rx_pattern.search(line)

            if tx_match:
                rnti, pid, tx_bytes = map(int, tx_match.groups())
                tx_dict[(rnti, pid)] = tx_bytes

            elif rx_match:
                rnti, pid, rx_bytes = map(int, rx_match.groups())
                rx_dict[(rnti, pid)] = rx_bytes

    # Combine transmit and receive entries
    all_keys = set(tx_dict.keys()) | set(rx_dict.keys())
    paired_list = []

    for key in sorted(all_keys):
        tx_bytes = tx_dict.get(key)
        rx_bytes = rx_dict.get(key)
        mismatch = (tx_bytes != rx_bytes) if (tx_bytes is not None and rx_bytes is not None) else True

        entry = {
            'rnti': key[0],
            'id': key[1],
            'transmit_bytes': tx_bytes,
            'receive_bytes': rx_bytes,
            'mismatch': mismatch
        }
        paired_list.append(entry)

    return paired_list

if __name__ == "__main__":
    # Example usage
    # python check_tx_rx_pdu.py pdu.txt

    parser = argparse.ArgumentParser(description="Process NB-LENA PDU log file")
    parser.add_argument("log_fname", nargs="?", default="pdu.txt", help="Path to the log file (e.g., pdu.txt)")
    args = parser.parse_args()

    if not os.path.isfile(args.log_fname):
        print(f"Error: File '{args.log_fname}' does not exist.")
        sys.exit(1)

    # Proceed with your log parsing
    print(f"Processing file: {args.log_fname}")
    results = parse_log(args.log_fname)

    number_pkts = len(results)
    number_pkts_loss = sum(1 for entry in results if entry['mismatch'])
    per = number_pkts_loss / number_pkts if number_pkts > 0 else 0
    print(f"Total packets: {number_pkts}, Lost packets: {number_pkts_loss}, PER: {per:.2%}")

    average_packet_size_in_bytes = sum(entry['receive_bytes'] for entry in results if entry['receive_bytes'] is not None) / number_pkts if number_pkts > 0 else 0

    avg_bits = 8 * average_packet_size_in_bytes
    ber = 1 - (1 - per) ** (1 / avg_bits)
    print(f"Estimated BER: {ber:.2%}")

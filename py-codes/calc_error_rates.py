
def read_log(filename):
    # assumes the files contains lines with the following format:
    # <timestamp> <size>
    with open(filename) as f:
        return [int(line.strip().split()[1]) for line in f.readlines()]

def compute_ber(tx_sizes, rx_sizes):
    # Bit Error Rate (BER) = (Total Bits - Received Bits) / Total Bits
    # Notice: sizes are in bytes, thus, BER will be greater than zero only if some packet's bytes are lost
    total_bits = sum(tx_sizes) * 8  # Convert bytes to bits
    received_bits = sum(rx_sizes) * 8
    bit_errors = total_bits - received_bits
    ber = bit_errors / total_bits if total_bits > 0 else 0
    return ber

def compute_per(tx_sizes, rx_sizes):
    # Packet Error Rate (PER) = (Total Packets - Received Packets) / Total Packets
    total_packets = len(tx_sizes)
    received_packets = len(rx_sizes)
    packet_errors = total_packets - received_packets
    per = packet_errors / total_packets if total_packets > 0 else 0
    return per

tx_sizes = read_log("tx_log.txt")
rx_sizes = read_log("rx_log.txt")

print("BER:", compute_ber(tx_sizes, rx_sizes))
print("PER:", compute_per(tx_sizes, rx_sizes))
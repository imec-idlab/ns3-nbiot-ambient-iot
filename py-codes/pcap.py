import os
import glob
import datetime
from tqdm import tqdm
import pyshark

import pandas as pd


"""
requirements:

```bash
pip install pyshark
sudo apt install tshark
```

"""

if __name__ == '__main__':
    # Load the .pcap file
    pcap_filenames = glob.glob(os.path.join("logs/markov/u4_t1800000000000_c0_e0/28_07_2025_16_11_25", "*.pcap"))

    all_data = []
    for pcap_filename in pcap_filenames:
        cap = pyshark.FileCapture(pcap_filename)

        for packet in tqdm(cap, desc=os.path.basename(pcap_filename)):
            data = {
                'time': (packet.sniff_time - datetime.datetime(1970, 1, 1, 1, 0, 0, 0)).total_seconds(),
                'source': None,
                'destination': None,
                'protocol': packet.highest_layer,
                'length': packet.length
            }
            if hasattr(packet, 'ip'):
                data['source'] = getattr(packet.ip, 'src', None)
                data['destination'] = getattr(packet.ip, 'dst', None)

            try:
                imsi = packet.s1ap.imsi  # For S1AP
            except AttributeError:
                try:
                    imsi = packet.gtp.imsi  # For GTP
                except AttributeError:
                    try:
                        imsi = packet.gtpv2.e212_imsi
                    except AttributeError:
                        imsi = None  # IMSI not found in this packet
            data['imsi'] = imsi
            print(f"IMSI: {imsi}")

            # print(data)
            all_data.append(data)

        cap.close()

    print(len(all_data))

    # Save the data to a CSV file
    df = pd.DataFrame(all_data)
    df.to_csv('data.csv', index=False)

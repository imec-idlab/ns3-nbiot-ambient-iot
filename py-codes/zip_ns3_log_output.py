import argparse
import os
import zipfile

def compress_ns3_logs(root_path, file_name="ns3_log_output.log"):
    # Walk through all subdirectories
    for dirpath, _, filenames in os.walk(root_path):
        if file_name in filenames:
            log_path = os.path.join(dirpath, file_name)
            zip_path = log_path + ".zip"

            # Create zip file and add the log file
            with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                zipf.write(log_path, arcname=file_name)

            # Remove the original log file
            os.remove(log_path)
            print(f"Compressed and removed: {log_path}")


# Example usage
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compress NS3 log files")
    # path to the directory you want to search the log files to compress
    parser.add_argument("-d", "--directory", type=str, default="./logs", help="Path to the directory you want to search the log files to compress")
    args = parser.parse_args()
    compress_ns3_logs(args.directory)

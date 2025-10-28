import re


def parse_simulation_config(file_path):
    """
    Parse a simulation configuration file and return a dictionary of parameters.

    The function looks for "key = value" lines and for a line starting with "Raw Command-Line Arguments:",
    followed by any number of lines containing "--key=value" arguments. The function returns a dictionary
    containing all the parsed parameters.

    :param file_path: The path to the simulation configuration file.
    :return: A dictionary of parameters.
    """
    params = {}

    with open(file_path, 'r') as f:
        lines = f.readlines()

    # Parse "key = value" lines
    for line in lines:
        match = re.match(r'^(\w+)\s*=\s*(.+)$', line.strip())
        if match:
            key, value = match.groups()
            params[key] = value

    # Find the line with raw command-line arguments
    for i, line in enumerate(lines):
        if line.strip().startswith("Raw Command-Line Arguments:"):
            raw_args = ' '.join(lines[i+1:]).strip()
            break
    else:
        raw_args = ""

    # Parse "--key=value" arguments
    for match in re.finditer(r'--([\w:]+)=([^\s]+)', raw_args):
        key, value = match.groups()
        params[key] = value

    return params


if __name__ == "__main__":
    # Example usage
    config_dict = parse_simulation_config("/home/h3dema/ns3-nbiot/logs/markov/u4_t10000000000_c0_e0/28_10_2025_14_18_36/simulation_config.log")
    for k, v in config_dict.items():
        print(f"{k}: {v}")

import os


def find_files(directory: str, target_suffix: str) -> list:

    """
    Walks through a directory and finds all files with the specified suffix.

    Args:
        directory (str): The directory to start the search from.
        target_suffix (str): The suffix of the files to look for.

    Returns:
        list: A list of paths to the files found.
    """
    files_found = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(target_suffix):
                rel_path = os.path.relpath(os.path.join(root, file))
                files_found.append(rel_path)
    return files_found

import argparse
import subprocess
import sys
from typing import Tuple

def _parse_args() -> Tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(description="Run compile.py with additional arguments")
    return parser.parse_known_args()

def main():
    args, unknown_args = _parse_args()

    # Construct the command to call compile.py with unknown arguments
    command = [sys.executable, 'compile.py'] + unknown_args

    # Call compile.py with the unknown arguments
    result = subprocess.run(command, capture_output=True, text=True)

    # Print the output from compile.py
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)

if __name__ == "__main__":
    main()

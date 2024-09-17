#!/usr/bin/env python3

import os
import subprocess
import sys


def main():
    # Change to the directory of the script
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    # Change to the 'native' directory and run 'pio run'
    os.chdir("native")
    result = subprocess.run(["pio", "run"], check=True)

    # Exit with the same status as the pio run command
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()

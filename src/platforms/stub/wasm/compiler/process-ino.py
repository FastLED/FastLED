#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Process INO file and dump AST.")
    parser.add_argument("input_file", help="Input source file (.cpp)")
    return parser.parse_args()

def run_command(command):
    result = subprocess.run(command, capture_output=True, text=True)
    print(result.stdout)
    if result.returncode != 0:
        print(f"Error: {result.stderr}", file=sys.stderr)
        sys.exit(result.returncode)

def main() -> None:
    # Parse command-line arguments
    args = parse_arguments()

    # Ensure the file exists
    if not os.path.isfile(args.input_file):
        print(f"Error: File '{args.input_file}' not found!")
        sys.exit(1)

    # Add Emscripten's Clang to PATH
    os.environ['PATH'] = "/emsdk_portable/upstream/bin:" + os.environ['PATH']

    # Print Clang version
    run_command(["clang", "--version"])

    # Dump the AST
    run_command(["clang", "-Xclang", "-ast-dump", "-fsyntax-only", "-nostdinc", args.input_file])

if __name__ == "__main__":
    main()

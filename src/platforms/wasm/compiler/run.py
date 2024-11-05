import argparse
import subprocess
import sys
from typing import Tuple
from pathlib import Path

_PORT = 80

_CHOICES = [
    "compile",
    "server"
]

HERE = Path(__file__).parent

def _parse_args() -> Tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(description="Run compile.py with additional arguments")
    parser.add_argument("mode", help="Which mode does this script run in", choices=_CHOICES)
    return parser.parse_known_args()

def _run_server(unknown_args: list[str]) -> None:
    cmd_list = [
        "uvicorn",
        "server:app",
        "--host",
        "0.0.0.0",
        "--port",
        f"{_PORT}"
    ] + unknown_args
    subprocess.run(cmd_list, cwd=str(HERE))

def _run_compile(unknown_args: list[str]) -> None:

    # Construct the command to call compile.py with unknown arguments
    command = [sys.executable, 'compile.py'] + unknown_args

    # Call compile.py with the unknown arguments
    result = subprocess.run(command, capture_output=True, text=True)

    # Print the output from compile.py
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)

def main():
    args, unknown_args = _parse_args()

    if args.mode == "compile":
        _run_compile(unknown_args)
    elif args.mode == "server":
        _run_server(unknown_args)

if __name__ == "__main__":
    main()

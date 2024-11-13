import argparse
import subprocess
import os
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

def _run_server(unknown_args: list[str]) -> int:
    env = os.environ.copy()
    if "--disable-auto-clean" in unknown_args:
        env["DISABLE_AUTO_CLEAN"] = "1"
        unknown_args.remove("--disable-auto-clean")
    cmd_list = [
        "uvicorn",
        "server:app",
        "--host",
        "0.0.0.0",
        "--port",
        f"{_PORT}"
    ] + unknown_args
    cp: subprocess.CompletedProcess = subprocess.run(cmd_list, cwd=str(HERE), env=env)
    return cp.returncode

def _run_compile(unknown_args: list[str]) -> int:

    # Construct the command to call compile.py with unknown arguments
    command = [sys.executable, 'compile.py'] + unknown_args

    # Call compile.py with the unknown arguments
    result = subprocess.run(command, text=True)

    # Print the output from compile.py
    #print(result.stdout)
    #if result.stderr:
    #    print(result.stderr, file=sys.stderr)
    return result.returncode

def main() -> int:
    args, unknown_args = _parse_args()

    try:
        if args.mode == "compile":
            rtn = _run_compile(unknown_args)
            return rtn
        elif args.mode == "server":
            rtn = _run_server(unknown_args)
            return rtn
        raise ValueError(f"Unknown mode: {args.mode}")
    except KeyboardInterrupt:
        print("Exiting...")
        return 1


if __name__ == "__main__":
    sys.exit(main())

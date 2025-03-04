import argparse
import subprocess
import sys


def parse_args() -> tuple[argparse.Namespace, list]:
    parser = argparse.ArgumentParser(description="Compile wasm")
    parser.add_argument(
        "sketch_dir",
        nargs="?",
        default="examples/wasm",
        help="The directory of the sketch to compile",
    )
    # return parser.parse_args()
    known_args, unknown_args = parser.parse_known_args()
    return known_args, unknown_args


def main() -> int:
    args, unknown_args = parse_args()
    cmd_list = ["fastled", args.sketch_dir, "--build"]
    cmd_list.extend(unknown_args)
    cmd_str = subprocess.list2cmdline(cmd_list)
    print(f"Running command: {cmd_str}")
    rtn = subprocess.call(cmd_list)
    if rtn != 0:
        print(f"ERROR: Command {cmd_str} failed with return code {rtn}")
    cmd_list = ["fastled", args.sketch_dir, "--just-compile"]
    cmd_list.extend(unknown_args)
    cmd_str = subprocess.list2cmdline(cmd_list)
    print(f"Running command: {cmd_str}")
    rtn = subprocess.call(cmd_list)
    if rtn != 0:
        print(f"ERROR: Command {cmd_str} failed with return code {rtn}")
    return rtn


if __name__ == "__main__":
    sys.exit(main())

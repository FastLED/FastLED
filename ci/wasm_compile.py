import argparse
import subprocess
import sys
from typing import List, Tuple


def parse_args() -> Tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(description="Compile wasm")
    parser.add_argument(
        "sketch_dir",
        nargs="?",
        default="examples/wasm",
        help="The directory of the sketch to compile",
    )
    # return parser.parse_args()
    known_args, unknown_args = parser.parse_known_args()
    # if -b or --build is in the unknown_args, remove it
    if "--build" in unknown_args:
        print("WARNING: --build is no longer supported. It will be ignored.")
        unknown_args.remove("--build")
    if "-b" in unknown_args:
        print("WARNING: -b is no longer supported. It will be ignored.")
        unknown_args.remove("-b")
    return known_args, unknown_args


def run_command(cmd_list: List[str]) -> int:
    """Run a command and return its exit code."""
    cmd_str = subprocess.list2cmdline(cmd_list)
    print(f"Running command: {cmd_str}")
    rtn = subprocess.call(cmd_list)
    if rtn != 0:
        print(f"ERROR: Command {cmd_str} failed with return code {rtn}")
    return rtn


def main() -> int:
    args, unknown_args = parse_args()

    # First run the build command
    build_cmd = ["fastled", args.sketch_dir] + unknown_args
    build_result = run_command(build_cmd)

    # Then run the compile command
    compile_cmd = ["fastled", args.sketch_dir, "--just-compile"] + unknown_args
    compile_result = run_command(compile_cmd)

    # Return non-zero if either command failed
    return build_result if build_result != 0 else compile_result


if __name__ == "__main__":
    sys.exit(main())

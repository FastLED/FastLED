import argparse
import subprocess


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


def main() -> None:
    args, unknown_args = parse_args()
    cmd_list = ["fastled", args.sketch_dir, "--build", "--just-compile"]
    cmd_list.extend(unknown_args)
    subprocess.check_call(cmd_list)


if __name__ == "__main__":
    main()

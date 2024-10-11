import argparse
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
WASM_DIR = HERE / "wasm"
DOCKER_FILE = WASM_DIR / "Dockerfile"


class WASMCompileError(Exception):
    """Custom exception for WASM compilation errors."""

    pass


def check_prerequisites():
    if not WASM_DIR.exists():
        raise WASMCompileError(f"ERROR: WASM directory not found at {WASM_DIR}")
    if not DOCKER_FILE.exists():
        raise WASMCompileError(f"ERROR: Dockerfile not found at {DOCKER_FILE}")


def filter_containers():
    return subprocess.run(
        ["docker", "ps", "-a", "-q", "--filter", "ancestor=fastled-wasm-compiler"],
        capture_output=True,
        text=True,
    ).stdout.strip()


def container_exists():
    return bool(filter_containers())


def image_exists():
    return (
        subprocess.run(
            ["docker", "image", "inspect", "fastled-wasm-compiler"],
            capture_output=True,
        ).returncode
        == 0
    )


def clean():
    if container_exists():
        print("Stopping and removing containers...")
        container_ids = filter_containers()
        if container_ids:
            subprocess.run(["docker", "stop"] + container_ids.split(), check=True)
            subprocess.run(["docker", "rm"] + container_ids.split(), check=True)
        else:
            print("No containers found for fastled-wasm-compiler.")
    else:
        print("No containers found for fastled-wasm-compiler.")

    if image_exists():
        print("Removing fastled-wasm-compiler image...")
        subprocess.run(["docker", "rmi", "fastled-wasm-compiler"], check=True)
    else:
        print("No image found for fastled-wasm-compiler.")


def build_image():
    print("Building Docker image...")
    try:
        subprocess.run(
            [
                "docker",
                "build",
                "-t",
                "fastled-wasm-compiler",
                "-f",
                str(DOCKER_FILE),
                str(WASM_DIR),
            ],
            check=True,
            text=True,
        )
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to build Docker image.\nError: {e}")


def run_container(directory):
    absolute_directory = os.path.abspath(directory)
    if not os.path.isdir(absolute_directory):
        raise WASMCompileError(
            f"ERROR: Directory '{absolute_directory}' does not exist."
        )

    try:
        subprocess.run(
            [
                "docker",
                "run",
                "--rm",
                "-it",
                "-v",
                f"{absolute_directory}:/workspace",
                "fastled-wasm-compiler",
            ],
            check=True,
        )
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to run Docker container.\n{e}")


def main():
    parser = argparse.ArgumentParser(description="WASM Compiler for FastLED")
    parser.add_argument(
        "directory", nargs="?", help="The directory to mount as a volume"
    )
    parser.add_argument(
        "-b",
        "--build",
        action="store_true",
        help="Build the Docker image before running",
    )
    parser.add_argument(
        "-c",
        "--clean",
        action="store_true",
        help="Clean up Docker containers and images",
    )
    args = parser.parse_args()

    try:
        check_prerequisites()

        if args.clean:
            clean()
            return

        if args.directory is None:
            parser.error("ERROR: directory is required unless --clean is specified")

        if args.build or not image_exists():
            build_image()

        run_container(args.directory)

    except WASMCompileError as e:
        print(f"\033[91m{str(e)}\033[0m")  # Print error in red
        sys.exit(1)
    except Exception as e:
        print(f"\033[91mUnexpected error: {str(e)}\033[0m")  # Print error in red
        sys.exit(1)


if __name__ == "__main__":
    main()

import os
import sys
import subprocess
import argparse
from pathlib import Path

def build_image():
    dockerfile_path = Path("examples/wasm/compiler/Dockerfile")
    if not dockerfile_path.is_file():
        print("Error: Dockerfile not found in examples/wasm/compiler/")
        print("Please make sure you are running this script from the root of the FastLED project.")
        sys.exit(1)
    
    print("Building Docker image...")
    subprocess.run(["docker", "build", "-t", "fastled-wasm-compiler", "-f", str(dockerfile_path), "examples/wasm/compiler"], check=True)

def run_container(directory):
    absolute_directory = os.path.abspath(directory)
    if not os.path.isdir(absolute_directory):
        print(f"Error: Directory '{absolute_directory}' does not exist.")
        sys.exit(1)
    
    subprocess.run(["docker", "run", "--rm", "-it", "-v", f"{absolute_directory}:/workspace", "fastled-wasm-compiler"], check=True)

def main():
    parser = argparse.ArgumentParser(description="WASM Compiler for FastLED")
    parser.add_argument("directory", help="The directory to mount as a volume")
    parser.add_argument("-b", "--build", action="store_true", help="Build the Docker image before running")
    args = parser.parse_args()

    if args.build or not subprocess.run(["docker", "image", "inspect", "fastled-wasm-compiler"], capture_output=True).returncode == 0:
        build_image()

    run_container(args.directory)

if __name__ == "__main__":
    main()

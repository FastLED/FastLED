import os
import subprocess
import argparse
import csv
import dateutil.parser
import shutil
from pathlib import Path

def run_command(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    output, error = process.communicate()
    return output.decode('utf-8'), error.decode('utf-8')

def step_back_commits(steps):
    step_back_command = f"git reset --hard HEAD~{steps}"
    output, error = run_command(step_back_command)
    if error:
        print(f"Error stepping back {steps} commit(s): {error}")
        return False
    return True

def check_firmware_size(board: str) -> int:
    base_path = Path(".build") / board / ".pio" / "build" / board
    firmware = base_path / "firmware.bin"
    if not firmware.exists():
        firmware = base_path / "firmware.hex"
    if not firmware.exists():
        raise FileNotFoundError(f"firmware.bin or firmware.hex not found in {base_path}")
    size_command = f"du -b {firmware}"

    output, error = run_command(size_command)
    if error:
        print(f"Error checking firmware size: {error}")
        return None
    size_in_bytes = output.strip().split()[0]
    return int(size_in_bytes)

def get_commit_hash():
    hash_command = "git rev-parse HEAD"
    output, error = run_command(hash_command)
    if error:
        print(f"Error getting commit hash: {error}")
        return None
    return output.strip()

def get_commit_date(commit_hash):
    date_command = f"git show -s --format=%ci {commit_hash}"
    output, error = run_command(date_command)
    if error:
        print(f"Error getting commit date: {error}")
        return None
    return dateutil.parser.parse(output.strip()).isoformat()

def main(board: str, num_commits: int, skip_step: int, start_commit: str | None = None, end_commit: str | None = None):
    # Create tmp directory if it doesn't exist
    if not os.path.exists("tmp"):
        os.makedirs("tmp")

    # Change to the tmp directory
    os.chdir("tmp")

    # 1. Git clone FastLED repository
    print("Cloning FastLED repository...")
    clone_command = "git clone https://github.com/FastLED/FastLED.git"
    output, error = run_command(clone_command)
    #if error:
    #    print(f"Error cloning repository: {error}")
    #    os.chdir("..")
    #    return

    # Change to the FastLED directory
    os.chdir("FastLED")

    # Checkout the latest commit
    run_command("git checkout master")

    # If end_commit is specified, checkout that commit
    # if end_commit:
    #     print(f"Checking out end commit: {end_commit}")
    #     checkout_command = f"git checkout {end_commit}"
    #     output, error = run_command(checkout_command)
    #     #if error:
    #     #    print(f"Error checking out end commit: {error}")
    #     #    return

    # Prepare CSV file
    csv_filename = "../../firmware_sizes.csv"
    with open(csv_filename, 'w', newline='') as csvfile:
        csvwriter = csv.writer(csvfile)
        csvwriter.writerow(['datetime', 'commit_hash', 'binary_size'])

    commits_checked = 0
    first_iteration = True
    while True:
        current_commit = get_commit_hash()

        if first_iteration and start_commit:
            first_iteration = False
            while True:
                if current_commit == start_commit:
                    break
                if not step_back_commits(1):
                    break
                current_commit = get_commit_hash()
        
        if num_commits and commits_checked >= num_commits:
            print(f"Checked {num_commits} commits")
            break

        if end_commit and current_commit == end_commit:
            print(f"Checked until end commit: {end_commit}")
            break

        # 2. Run ci-compile.py for current commit
        print(f"\nChecking commit {commits_checked + 1}")

        # remove .build/esp32dev/pio/build/esp32dev/ directory
        board_files = Path('.build') / board / ".pio" / "build" / board
        if board_files.exists():
            shutil.rmtree(str(board_files), ignore_errors=True)
        compile_command = f"python3 ci/ci-compile.py --boards {board} --examples Blink"
        output, error = run_command(compile_command)
        if error:
            print(f"Error running ci-compile.py: {error}")
            if not step_back_commits(skip_step):
                break
            continue

        # 3. Check firmware size and get commit hash
        print("Checking firmware size...")
        size = check_firmware_size(board)
        commit_hash = get_commit_hash()
        if size and commit_hash:
            commit_date = get_commit_date(commit_hash)
            print(f"Firmware size: {size} bytes")
            
            # Write to CSV incrementally
            with open(csv_filename, 'a', newline='') as csvfile:
                csvwriter = csv.writer(csvfile)
                csvwriter.writerow([commit_date, commit_hash, size])
            
            print(f"Result appended to {csv_filename}")

        commits_checked += 1

        # 4. Step back one commit
        print("Stepping back 1 commit...")
        if not step_back_commits(1):
            break

    # Don't remove the tmp directory
    print("\nTemporary directory 'tmp' has been left intact for inspection.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check FastLED firmware size for multiple commits.")
    parser.add_argument("--num-commits", type=int, default=1, help="Number of commits to check")
    parser.add_argument("--skip-step", type=int, default=1, help="Number of commits to skip between checks")
    parser.add_argument("--start-commit", type=str, help="Starting commit hash")
    parser.add_argument("--end-commit", type=str, help="Ending commit hash")
    parser.add_argument("--board", type=str, default="esp32dev", help="Board to check firmware size for")
    args = parser.parse_args()

    if args.start_commit or args.end_commit:
        if not (args.start_commit and not args.end_commit):
            print("Both start commit and end commit must be specified.")
            exit(1)
        # if start_commit is specified, end_commit must be specified

    num_commits = args.num_commits
    if args.start_commit and args.end_commit:
        if args.start_commit == args.end_commit:
            print("Start commit and end commit are the same.")
            exit(1)
        num_commits = 999999
    
        
    main(args.board, num_commits, args.skip_step, args.start_commit, args.end_commit)

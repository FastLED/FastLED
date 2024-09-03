import os
import subprocess
import argparse
import csv
from datetime import datetime
import dateutil.parser

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

def check_firmware_size():
    size_command = "du -b .build/esp32dev/.pio/build/esp32dev/firmware.bin"
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

def main(num_commits, skip_step):
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

    # Prepare CSV file
    csv_filename = "../../firmware_sizes.csv"
    with open(csv_filename, 'w', newline='') as csvfile:
        csvwriter = csv.writer(csvfile)
        csvwriter.writerow(['datetime', 'commit_hash', 'binary_size'])

    commits_checked = 0
    while commits_checked < num_commits:
        # 2. Run ci-compile.py for current commit
        print(f"\nChecking commit {commits_checked + 1} of {num_commits}")
        compile_command = "python3 ci/ci-compile.py --boards esp32dev --examples Blink"
        output, error = run_command(compile_command)
        if error:
            print(f"Error running ci-compile.py: {error}")
            if not step_back_commits(skip_step):
                break
            continue

        # 3. Check firmware size and get commit hash
        print("Checking firmware size...")
        size = check_firmware_size()
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

        # 4. Step back skip_step commits
        if commits_checked < num_commits:
            print(f"Stepping back {skip_step} commit(s)...")
            if not step_back_commits(skip_step):
                break

    # Don't remove the tmp directory
    print("\nTemporary directory 'tmp' has been left intact for inspection.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check FastLED firmware size for multiple commits.")
    parser.add_argument("num_commits", type=int, help="Number of commits to check")
    parser.add_argument("--skip-step", type=int, default=1, help="Number of commits to skip between checks")
    args = parser.parse_args()
    main(args.num_commits, args.skip_step)

#!/usr/bin/env python3
import json
import os
import sys
from pathlib import Path

from ci.ci.test_args import parse_args
from ci.ci.test_commands import run_command
from ci.ci.test_env import setup_environment, setup_force_exit, setup_watchdog
from ci.ci.test_runner import runner as test_runner
from ci.ci.test_types import (
    FingerprintResult,
    calculate_fingerprint,
    process_test_flags,
)


def main() -> None:
    try:
        # Change to script directory first
        os.chdir(Path(__file__).parent)

        # Set up watchdog timer
        watchdog = setup_watchdog()

        # Parse and process arguments
        args = parse_args()
        args = process_test_flags(args)

        # Handle --no-interactive flag
        if args.no_interactive:
            os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
            os.environ["GITHUB_ACTIONS"] = (
                "true"  # This ensures all subprocess also run in non-interactive mode
            )

        # Handle --interactive flag
        if args.interactive:
            os.environ.pop("FASTLED_CI_NO_INTERACTIVE", None)
            os.environ.pop("GITHUB_ACTIONS", None)

        # Set up remaining environment based on arguments
        setup_environment(args)

        # Handle stack trace control
        enable_stack_trace = not args.no_stack_trace
        if enable_stack_trace:
            print("Stack trace dumping enabled for test timeouts")
        else:
            print("Stack trace dumping disabled for test timeouts")

        # Validate conflicting arguments
        if args.no_interactive and args.interactive:
            print(
                "Error: --interactive and --no-interactive cannot be used together",
                file=sys.stderr,
            )
            sys.exit(1)

        # Set up fingerprint caching
        cache_dir = Path(".cache")
        cache_dir.mkdir(exist_ok=True)
        fingerprint_file = cache_dir / "fingerprint.json"

        def write_fingerprint(fingerprint: FingerprintResult) -> None:
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_fingerprint() -> FingerprintResult | None:
            if fingerprint_file.exists():
                with open(fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data.get("hash", ""),
                            elapsed_seconds=data.get("elapsed_seconds"),
                            status=data.get("status"),
                        )
                    except json.JSONDecodeError:
                        print("Invalid fingerprint file. Recalculating...")
            return None

        # Calculate and save fingerprint
        prev_fingerprint = read_fingerprint()
        fingerprint_data = calculate_fingerprint()
        src_code_change = (
            True
            if prev_fingerprint is None
            else fingerprint_data.hash != prev_fingerprint.hash
        )
        write_fingerprint(fingerprint_data)

        # Run tests using the test runner
        test_runner(args, src_code_change)

        # Set up force exit daemon and exit
        daemon_thread = setup_force_exit()
        sys.exit(0)

    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)


if __name__ == "__main__":
    main()

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
GitHub Actions Debug Tool

Efficiently analyzes GitHub Actions failures by streaming logs and filtering for errors.
Avoids downloading massive log files by filtering as it streams.

Usage:
    python gh_debug.py <run-id-or-url>
    python gh_debug.py 18397776636
    python gh_debug.py https://github.com/FastLED/FastLED/actions/runs/18397776636

Features:
    - Streams logs instead of downloading everything
    - Filters for errors in real-time
    - Stops after finding sufficient errors
    - Shows error context (lines before/after)
    - Identifies job and step that failed
"""

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


class GitHubDebugger:
    """Debug GitHub Actions failures efficiently."""

    # Critical error patterns - actual compilation failures
    CRITICAL_PATTERNS = [
        r"\.(cpp|h|c|ino):\d+:\d+: error:",  # Compiler errors with file:line:col
        r"static assertion failed",
        r"compilation terminated",
        r"undefined reference",
        r"fatal error:",
    ]

    # General error patterns
    ERROR_PATTERNS = [
        r"error:",
        r"Error compiling",
        r"FAILED",
        r"Assertion.*failed",
        r"undefined reference",
        r"fatal error:",
        r"compilation terminated",
        r"Test.*failed",
        r"Error \d+",
        r"^\s*\^\s*$",  # Compiler error markers (^)
    ]

    # Patterns to exclude from critical error count (tool warnings, not build failures)
    EXCLUDE_PATTERNS = [
        r"esp_idf_size: error: unrecognized arguments",
        r"Warning: esp-idf-size exited with code",
    ]

    def __init__(self, run_id: str, max_errors: int = 10, context_lines: int = 5):
        """Initialize debugger.

        Args:
            run_id: GitHub run ID or URL
            max_errors: Maximum number of errors to collect before stopping
            context_lines: Lines of context before/after errors
        """
        self.run_id = self._extract_run_id(run_id)
        self.max_errors = max_errors
        self.context_lines = context_lines
        self.repo = self._get_repo()
        self.critical_errors: list[dict[str, str]] = []
        self.warnings: list[dict[str, str]] = []
        self.log_file: Optional[Path] = None

    def _extract_run_id(self, run_input: str) -> str:
        """Extract run ID from URL or return as-is if already an ID."""
        # Check if it's a URL
        match = re.search(r"/actions/runs/(\d+)", run_input)
        if match:
            return match.group(1)
        # Assume it's already an ID
        return run_input

    def _get_repo(self) -> str:
        """Get repository in owner/repo format."""
        try:
            result = subprocess.run(
                ["gh", "repo", "view", "--json", "nameWithOwner"],
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            data = json.loads(result.stdout)
            return data["nameWithOwner"]
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting repo info: {e}", file=sys.stderr)
            return "FastLED/FastLED"  # Default fallback

    def get_run_info(self) -> dict[str, str]:
        """Get basic run information."""
        try:
            result = subprocess.run(
                [
                    "gh",
                    "run",
                    "view",
                    self.run_id,
                    "--json",
                    "displayTitle,status,conclusion,createdAt",
                ],
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            return json.loads(result.stdout)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting run info: {e}", file=sys.stderr)
            return {}

    def get_failed_jobs(self) -> list[dict[str, str]]:
        """Get list of failed jobs."""
        try:
            result = subprocess.run(
                ["gh", "run", "view", self.run_id, "--json", "jobs"],
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            data = json.loads(result.stdout)
            jobs = data.get("jobs", [])

            # Return only failed jobs
            failed_jobs = [
                {
                    "name": job.get("name", "Unknown"),
                    "id": str(job.get("databaseId", "")),
                    "conclusion": job.get("conclusion", ""),
                }
                for job in jobs
                if job.get("conclusion") in ["failure", "cancelled"]
            ]
            return failed_jobs
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting failed jobs: {e}", file=sys.stderr)
            return []

    def download_logs(self) -> Optional[Path]:
        """Download logs to .logs/gh/ directory for analysis.

        Returns:
            Path to downloaded log file, or None if download failed
        """
        # Create .logs/gh/ directory
        log_dir = Path(".logs/gh")
        log_dir.mkdir(parents=True, exist_ok=True)

        log_file = log_dir / f"run_{self.run_id}.txt"

        # Check if log already exists and is recent (< 1 hour old)
        if log_file.exists():
            age_seconds: float = log_file.stat().st_ctime
            if time.time() - age_seconds < 3600:
                print(f"Using cached log file: {log_file}")
                return log_file

        print(f"Downloading logs to: {log_file}")

        try:
            # Download logs using gh run view
            result = subprocess.run(
                ["gh", "run", "view", self.run_id, "--log"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=True,
                timeout=300,  # 5 minute timeout
            )

            # Save to file
            with open(log_file, "w", encoding="utf-8") as f:
                f.write(result.stdout)

            print(f"Log downloaded successfully ({len(result.stdout)} bytes)\n")
            return log_file

        except subprocess.TimeoutExpired:
            print("Error: Log download timed out", file=sys.stderr)
            return None
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error downloading logs: {e}", file=sys.stderr)
            return None

    def analyze_logs(self, log_file: Path) -> None:
        """Analyze downloaded logs for errors.

        Args:
            log_file: Path to downloaded log file
        """
        print(f"Analyzing logs from: {log_file}\n")

        try:
            with open(log_file, "r", encoding="utf-8") as f:
                lines = f.readlines()

            # Process logs line by line
            error_buffer: list[str] = []
            context_buffer: list[str] = []
            in_error_context = False
            lines_after_error = 0
            current_job = "Unknown"

            for line in lines:
                line = line.rstrip()

                # Extract job name from log line
                job_match = re.match(r"^([\w\s/]+)\t", line)
                if job_match:
                    current_job = job_match.group(1).strip()

                # Keep context buffer
                context_buffer.append(line)
                if len(context_buffer) > self.context_lines:
                    context_buffer.pop(0)

                # Check if line should be excluded (tool warnings)
                is_excluded = any(
                    re.search(pattern, line, re.IGNORECASE)
                    for pattern in self.EXCLUDE_PATTERNS
                )

                # Check if line matches error pattern
                is_error = any(
                    re.search(pattern, line, re.IGNORECASE)
                    for pattern in self.ERROR_PATTERNS
                )

                # Check if line matches critical pattern
                is_critical = any(
                    re.search(pattern, line, re.IGNORECASE)
                    for pattern in self.CRITICAL_PATTERNS
                )

                if is_error and not in_error_context:
                    # Start new error context
                    in_error_context = True
                    lines_after_error = 0

                    # Add context before error
                    error_buffer.extend(context_buffer[:-1])  # All but current line
                    error_buffer.append(line)
                elif in_error_context:
                    error_buffer.append(line)
                    lines_after_error += 1

                    # Check if we've collected enough context
                    if lines_after_error >= self.context_lines:
                        # Save this error block
                        if is_excluded or (
                            not is_critical
                            and any(
                                re.search(p, "\n".join(error_buffer), re.IGNORECASE)
                                for p in self.EXCLUDE_PATTERNS
                            )
                        ):
                            # This is a tool warning, not a critical error
                            self._save_error_block(
                                current_job, error_buffer, is_critical=False
                            )
                        else:
                            # This is a critical error
                            self._save_error_block(
                                current_job, error_buffer, is_critical=True
                            )

                        error_buffer = []
                        in_error_context = False

                        # Check if we've collected enough CRITICAL errors
                        if len(self.critical_errors) >= self.max_errors:
                            print(
                                f"\n[Stopping after {self.max_errors} critical errors]"
                            )
                            break

            # Save any remaining error in buffer
            if error_buffer:
                self._save_error_block(current_job, error_buffer, is_critical=False)

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"\nError analyzing logs: {e}", file=sys.stderr)

    def _save_error_block(
        self, job_name: str, lines: list[str], is_critical: bool = True
    ) -> None:
        """Save an error block to the collection.

        Args:
            job_name: Name of the job
            lines: Error context lines
            is_critical: Whether this is a critical error or just a warning
        """
        error_block = {
            "job": job_name,
            "content": "\n".join(lines),
        }

        if is_critical:
            self.critical_errors.append(error_block)
        else:
            self.warnings.append(error_block)

    def print_summary(self) -> None:
        """Print summary of errors found."""
        if not self.critical_errors and not self.warnings:
            print("\n" + "=" * 80)
            print("No errors found in logs!")
            print("=" * 80)
            return

        print("\n" + "=" * 80)
        if self.critical_errors:
            print(f"CRITICAL ERRORS: Found {len(self.critical_errors)} error(s)")
        if self.warnings:
            print(f"WARNINGS: Found {len(self.warnings)} warning(s)")
        print("=" * 80 + "\n")

        # Show critical errors first
        if self.critical_errors:
            print("=" * 80)
            print("CRITICAL ERRORS (Build Failures)")
            print("=" * 80 + "\n")
            for i, error in enumerate(self.critical_errors, 1):
                print(
                    f"--- Critical Error {i}/{len(self.critical_errors)} in {error['job']} ---"
                )
                print(error["content"])
                print()

        # Show warnings if no critical errors, or if there are few warnings
        if self.warnings and (not self.critical_errors or len(self.warnings) <= 5):
            print("=" * 80)
            print("WARNINGS (Non-Critical)")
            print("=" * 80 + "\n")
            for i, warning in enumerate(self.warnings[:5], 1):
                print(
                    f"--- Warning {i}/{min(len(self.warnings), 5)} in {warning['job']} ---"
                )
                print(warning["content"])
                print()
            if len(self.warnings) > 5:
                print(
                    f"... and {len(self.warnings) - 5} more warnings (check log file for details)"
                )
                print()

    def debug(self) -> None:
        """Run the debugging process."""
        # Get run info
        run_info = self.get_run_info()
        if run_info:
            print(f"\nWorkflow: {run_info.get('displayTitle', 'Unknown')}")
            print(f"Status: {run_info.get('status', 'Unknown')}")
            print(f"Conclusion: {run_info.get('conclusion', 'Unknown')}")
            print(f"Created: {run_info.get('createdAt', 'Unknown')}")

        # Get failed jobs
        failed_jobs = self.get_failed_jobs()
        if not failed_jobs:
            print("\nNo failed jobs found!")
            return

        print(f"\nFound {len(failed_jobs)} failed job(s):")
        for job in failed_jobs:
            print(f"  - {job['name']} ({job['conclusion']})")

        # Download logs
        print()
        log_file = self.download_logs()
        if not log_file:
            print("Failed to download logs. Cannot continue analysis.", file=sys.stderr)
            return

        self.log_file = log_file

        # Analyze logs
        self.analyze_logs(log_file)

        # Print summary
        self.print_summary()

        # Show log file location at the end
        if self.log_file:
            print("=" * 80)
            print(f"Full logs saved to: {self.log_file}")
            print("=" * 80)


def main():
    parser = argparse.ArgumentParser(
        description="Debug GitHub Actions failures efficiently",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s 18397776636
    %(prog)s https://github.com/FastLED/FastLED/actions/runs/18397776636
    %(prog)s 18397776636 --max-errors 20 --context 10
        """,
    )
    parser.add_argument("run_id", help="GitHub Actions run ID or URL")
    parser.add_argument(
        "--max-errors",
        type=int,
        default=10,
        help="Maximum number of errors to collect (default: 10)",
    )
    parser.add_argument(
        "--context",
        type=int,
        default=5,
        help="Lines of context before/after errors (default: 5)",
    )

    args = parser.parse_args()

    debugger = GitHubDebugger(
        run_id=args.run_id,
        max_errors=args.max_errors,
        context_lines=args.context,
    )

    try:
        debugger.debug()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

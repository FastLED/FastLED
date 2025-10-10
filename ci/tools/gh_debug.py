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
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


class GitHubDebugger:
    """Debug GitHub Actions failures efficiently."""

    # Error patterns to detect
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
        self.errors_found: List[Dict[str, str]] = []

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
        except Exception as e:
            print(f"Error getting repo info: {e}", file=sys.stderr)
            return "FastLED/FastLED"  # Default fallback

    def get_run_info(self) -> Dict[str, str]:
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
        except Exception as e:
            print(f"Error getting run info: {e}", file=sys.stderr)
            return {}

    def get_failed_jobs(self) -> List[Dict[str, str]]:
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
        except Exception as e:
            print(f"Error getting failed jobs: {e}", file=sys.stderr)
            return []

    def stream_job_logs(self, job_id: str, job_name: str) -> None:
        """Stream and filter job logs for errors.

        Args:
            job_id: GitHub job ID
            job_name: Human-readable job name
        """
        print(f"\n{'=' * 80}")
        print(f"Analyzing job: {job_name} (ID: {job_id})")
        print(f"{'=' * 80}\n")

        try:
            # Use gh api to stream logs
            # Pipe through grep to filter first, then process
            api_path = f"/repos/{self.repo}/actions/jobs/{job_id}/logs"

            # Build grep pattern for all error patterns
            grep_pattern = "|".join(f"({p})" for p in self.ERROR_PATTERNS)

            # Stream logs, grep for errors with context
            cmd = [
                "gh",
                "api",
                api_path,
                "--paginate",
            ]

            print(f"Fetching logs (this may take a moment)...\n")

            # Run command and capture output
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )

            if process.stdout is None:
                print("Error: Could not open subprocess stdout", file=sys.stderr)
                return

            # Process logs line by line
            error_buffer: List[str] = []
            context_buffer: List[str] = []
            in_error_context = False
            lines_after_error = 0

            for line in process.stdout:
                line = line.rstrip()

                # Keep context buffer
                context_buffer.append(line)
                if len(context_buffer) > self.context_lines:
                    context_buffer.pop(0)

                # Check if line matches error pattern
                is_error = any(
                    re.search(pattern, line, re.IGNORECASE)
                    for pattern in self.ERROR_PATTERNS
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
                        self._save_error_block(job_name, error_buffer)
                        error_buffer = []
                        in_error_context = False

                        # Check if we've collected enough errors
                        if len(self.errors_found) >= self.max_errors:
                            print(
                                f"\n[Stopping after {self.max_errors} errors to avoid processing entire log]"
                            )
                            process.terminate()
                            break

            # Save any remaining error in buffer
            if error_buffer:
                self._save_error_block(job_name, error_buffer)

            # Wait for process to complete
            process.wait(timeout=5)

        except subprocess.TimeoutExpired:
            print("\nLog fetching timed out", file=sys.stderr)
        except Exception as e:
            print(f"\nError streaming logs: {e}", file=sys.stderr)

    def _save_error_block(self, job_name: str, lines: List[str]) -> None:
        """Save an error block to the collection."""
        self.errors_found.append(
            {
                "job": job_name,
                "content": "\n".join(lines),
            }
        )

    def print_summary(self) -> None:
        """Print summary of errors found."""
        if not self.errors_found:
            print("\n" + "=" * 80)
            print("No errors found in logs!")
            print("=" * 80)
            return

        print("\n" + "=" * 80)
        print(f"SUMMARY: Found {len(self.errors_found)} error(s)")
        print("=" * 80 + "\n")

        for i, error in enumerate(self.errors_found, 1):
            print(f"--- Error {i}/{len(self.errors_found)} in {error['job']} ---")
            print(error["content"])
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

        # Process each failed job
        for job in failed_jobs:
            if not job["id"]:
                print(f"\nSkipping {job['name']} - no job ID", file=sys.stderr)
                continue

            self.stream_job_logs(job["id"], job["name"])

            # Stop if we have enough errors
            if len(self.errors_found) >= self.max_errors:
                print(f"\nCollected {self.max_errors} errors. Stopping analysis.")
                break

        # Print summary
        self.print_summary()


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
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

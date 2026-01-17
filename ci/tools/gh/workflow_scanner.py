from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
GitHub Actions Workflow Scanner

Scans a GitHub Actions workflow file and analyzes all failing jobs.
Fetches logs concurrently but streams output serially to prevent buffer overflow.

Usage:
    uv run ci/tools/gh/workflow_scanner.py
    uv run ci/tools/gh/workflow_scanner.py --workflow build.yml
    uv run ci/tools/gh/workflow_scanner.py --run-id 12345678
    uv run ci/tools/gh/workflow_scanner.py --workflow build.yml --max-runs 5

Features:
    - Scans workflow runs for failures
    - Fetches logs concurrently for speed
    - Streams output serially to prevent stdout overflow
    - Filters for "error" and "fatal" with ±20 lines context
    - Shows progress for each job
"""

import argparse
import json
import queue
import subprocess
import sys
import threading
from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class JobInfo:
    """Information about a workflow job."""

    job_id: str
    job_name: str
    run_id: str
    conclusion: str
    status: str


@dataclass
class ErrorBlock:
    """A block of log lines containing an error."""

    job_name: str
    job_id: str
    lines: list[str]
    error_line_index: int  # Index of the line that triggered the error


class WorkflowScanner:
    """Scans GitHub Actions workflows and analyzes failures."""

    # Case-insensitive error patterns
    ERROR_KEYWORDS = ["error", "fatal"]

    def __init__(
        self,
        workflow_file: str = "build.yml",
        run_id: Optional[str] = None,
        context_lines: int = 20,
        max_concurrent_jobs: int = 5,
    ):
        """Initialize the workflow scanner.

        Args:
            workflow_file: Workflow filename (e.g., 'build.yml')
            run_id: Specific run ID to analyze (if None, uses latest)
            context_lines: Lines of context before/after errors
            max_concurrent_jobs: Maximum jobs to process concurrently
        """
        self.workflow_file = workflow_file
        self.run_id = run_id
        self.context_lines = context_lines
        self.max_concurrent_jobs = max_concurrent_jobs
        self.repo = self._get_repo()

        # Queue for streaming output
        self.output_queue: queue.Queue[Optional[ErrorBlock]] = queue.Queue()
        self.jobs_completed = 0
        self.jobs_lock = threading.Lock()

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

    def get_workflow_runs(self, max_runs: int = 1) -> list[dict[str, Any]]:
        """Get recent workflow runs.

        Args:
            max_runs: Maximum number of runs to retrieve

        Returns:
            List of workflow run dictionaries
        """
        try:
            cmd = [
                "gh",
                "run",
                "list",
                "--workflow",
                self.workflow_file,
                "--json",
                "databaseId,displayTitle,status,conclusion,createdAt,headBranch",
                "--limit",
                str(max_runs),
            ]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True,
                timeout=30,
            )
            runs = json.loads(result.stdout)
            return runs
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting workflow runs: {e}", file=sys.stderr)
            return []

    def get_run_jobs(self, run_id: str) -> list[JobInfo]:
        """Get all jobs for a specific run.

        Args:
            run_id: The workflow run ID

        Returns:
            List of JobInfo objects
        """
        try:
            result = subprocess.run(
                ["gh", "run", "view", run_id, "--json", "jobs"],
                capture_output=True,
                text=True,
                check=True,
                timeout=30,
            )
            data = json.loads(result.stdout)
            jobs = data.get("jobs", [])

            job_infos: list[JobInfo] = []
            for job in jobs:
                job_info = JobInfo(
                    job_id=str(job.get("databaseId", "")),
                    job_name=job.get("name", "Unknown"),
                    run_id=run_id,
                    conclusion=job.get("conclusion", ""),
                    status=job.get("status", ""),
                )
                job_infos.append(job_info)

            return job_infos
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting jobs for run {run_id}: {e}", file=sys.stderr)
            return []

    def get_failed_jobs(self, run_id: str) -> list[JobInfo]:
        """Get only failed jobs for a run.

        Args:
            run_id: The workflow run ID

        Returns:
            List of JobInfo objects for failed jobs
        """
        all_jobs = self.get_run_jobs(run_id)
        failed_jobs = [
            job
            for job in all_jobs
            if job.conclusion in ["failure", "cancelled", "timed_out"]
        ]
        return failed_jobs

    def fetch_and_filter_job_logs(self, job: JobInfo) -> list[ErrorBlock]:
        """Fetch logs for a job and filter for errors.

        This runs in a worker thread. Results are put in the output queue.

        Args:
            job: JobInfo object for the job to process

        Returns:
            List of ErrorBlock objects containing filtered errors
        """
        error_blocks: list[ErrorBlock] = []

        try:
            # Use gh api to get logs
            api_path = f"/repos/{self.repo}/actions/jobs/{job.job_id}/logs"

            cmd = ["gh", "api", api_path, "--paginate"]

            # Run command and capture output
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )

            if process.stdout is None:
                return error_blocks

            # Read all lines into memory (we need to do context lookback)
            all_lines = [line.rstrip() for line in process.stdout]
            process.wait(timeout=60)

            # Find all lines matching error keywords
            error_indices: set[int] = set()
            for i, line in enumerate(all_lines):
                line_lower = line.lower()
                if any(keyword in line_lower for keyword in self.ERROR_KEYWORDS):
                    error_indices.add(i)

            # For each error, extract context
            processed_indices: set[int] = set()

            for error_idx in sorted(error_indices):
                # Skip if we've already processed this as part of another error's context
                if error_idx in processed_indices:
                    continue

                # Calculate context range
                start_idx = max(0, error_idx - self.context_lines)
                end_idx = min(len(all_lines), error_idx + self.context_lines + 1)

                # Extract context lines
                context_lines = all_lines[start_idx:end_idx]

                # Mark these indices as processed
                for idx in range(start_idx, end_idx):
                    processed_indices.add(idx)

                # Create error block
                error_block = ErrorBlock(
                    job_name=job.job_name,
                    job_id=job.job_id,
                    lines=context_lines,
                    error_line_index=error_idx - start_idx,
                )
                error_blocks.append(error_block)

        except subprocess.TimeoutExpired:
            print(f"Timeout fetching logs for {job.job_name}", file=sys.stderr)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(
                f"Error fetching logs for {job.job_name}: {e}",
                file=sys.stderr,
            )

        return error_blocks

    def process_job_worker(self, job: JobInfo) -> None:
        """Worker thread function to process a single job.

        Args:
            job: JobInfo object to process
        """
        # Fetch and filter logs
        error_blocks = self.fetch_and_filter_job_logs(job)

        # Put results in queue (even if empty, to track completion)
        for block in error_blocks:
            self.output_queue.put(block)

        # Signal completion
        with self.jobs_lock:
            self.jobs_completed += 1

    def stream_output(self, total_jobs: int) -> None:
        """Stream output from the queue to stdout.

        This runs in the main thread and outputs results serially.

        Args:
            total_jobs: Total number of jobs being processed
        """
        error_count = 0

        while True:
            # Check if we're done
            with self.jobs_lock:
                if self.jobs_completed >= total_jobs:
                    # Drain any remaining items in queue
                    while not self.output_queue.empty():
                        try:
                            block = self.output_queue.get_nowait()
                            if block:
                                error_count += 1
                                self._print_error_block(block, error_count)
                        except queue.Empty:
                            break
                    break

            # Get next item from queue (with timeout to check completion)
            try:
                block = self.output_queue.get(timeout=0.5)
                if block:
                    error_count += 1
                    self._print_error_block(block, error_count)
            except queue.Empty:
                continue

        # Print summary
        print("\n" + "=" * 80)
        if error_count == 0:
            print("No errors found!")
        else:
            print(f"Total errors found: {error_count}")
        print("=" * 80)

    def _print_error_block(self, block: ErrorBlock, error_num: int) -> None:
        """Print a single error block.

        Args:
            block: ErrorBlock to print
            error_num: Sequential error number
        """
        print(f"\n{'=' * 80}")
        print(f"Error #{error_num} in job: {block.job_name} (ID: {block.job_id})")
        print(f"{'=' * 80}")

        # Print lines with the error line highlighted
        for i, line in enumerate(block.lines):
            if i == block.error_line_index:
                print(f">>> {line}")
            else:
                print(f"    {line}")

        print()  # Empty line after each block

    def scan(self) -> None:
        """Main scanning logic."""
        # Get run to analyze
        if self.run_id:
            run_id = self.run_id
            print(f"Analyzing specific run: {run_id}")
        else:
            # Get latest run
            runs = self.get_workflow_runs(max_runs=1)
            if not runs:
                print("No workflow runs found!", file=sys.stderr)
                return

            run = runs[0]
            run_id = str(run["databaseId"])
            print(f"Analyzing latest run: {run_id}")
            print(f"  Title: {run.get('displayTitle', 'Unknown')}")
            print(f"  Branch: {run.get('headBranch', 'Unknown')}")
            print(f"  Status: {run.get('status', 'Unknown')}")
            print(f"  Conclusion: {run.get('conclusion', 'Unknown')}")

        # Get failed jobs
        print(f"\nFetching jobs for run {run_id}...")
        failed_jobs = self.get_failed_jobs(run_id)

        if not failed_jobs:
            print("\nNo failed jobs found!")
            return

        print(f"\nFound {len(failed_jobs)} failed job(s):")
        for job in failed_jobs:
            print(f"  - {job.job_name} ({job.conclusion})")

        print(f"\nProcessing logs (fetching {len(failed_jobs)} jobs concurrently)...")
        print("=" * 80)

        # Start worker threads to process jobs
        threads: list[threading.Thread] = []
        for job in failed_jobs:
            thread = threading.Thread(target=self.process_job_worker, args=(job,))
            thread.start()
            threads.append(thread)

            # Limit concurrent threads
            while len([t for t in threads if t.is_alive()]) >= self.max_concurrent_jobs:
                threads[0].join(timeout=0.1)
                threads = [t for t in threads if t.is_alive()]

        # Start streaming output in main thread
        self.stream_output(total_jobs=len(failed_jobs))

        # Wait for all threads to complete
        for thread in threads:
            thread.join()

        print("\nDone!")


def main():
    parser = argparse.ArgumentParser(
        description="Scan GitHub Actions workflow for failures",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Scan latest build.yml run
    %(prog)s

    # Scan specific workflow
    %(prog)s --workflow build.yml

    # Scan specific run
    %(prog)s --run-id 12345678

    # Scan with more context
    %(prog)s --context 30

    # Look at last 5 runs
    %(prog)s --max-runs 5
        """,
    )

    parser.add_argument(
        "--workflow",
        default="build.yml",
        help="Workflow filename (default: build.yml)",
    )
    parser.add_argument(
        "--run-id",
        help="Specific run ID to analyze (default: latest run)",
    )
    parser.add_argument(
        "--context",
        type=int,
        default=20,
        help="Lines of context before/after errors (default: 20)",
    )
    parser.add_argument(
        "--max-concurrent",
        type=int,
        default=5,
        help="Maximum concurrent job processing (default: 5)",
    )
    parser.add_argument(
        "--max-runs",
        type=int,
        default=1,
        help="If no run-id specified, scan this many recent runs (default: 1)",
    )

    args = parser.parse_args()

    # If max_runs > 1 and no specific run_id, process multiple runs
    if args.max_runs > 1 and not args.run_id:
        scanner_temp = WorkflowScanner(workflow_file=args.workflow)
        runs = scanner_temp.get_workflow_runs(max_runs=args.max_runs)

        if not runs:
            print("No runs found!", file=sys.stderr)
            sys.exit(1)

        print(f"Found {len(runs)} recent run(s)\n")

        for i, run in enumerate(runs, 1):
            print(f"\n{'#' * 80}")
            print(f"# Run {i}/{len(runs)}: {run['displayTitle']}")
            print(f"# Run ID: {run['databaseId']}")
            print(f"{'#' * 80}\n")

            scanner = WorkflowScanner(
                workflow_file=args.workflow,
                run_id=str(run["databaseId"]),
                context_lines=args.context,
                max_concurrent_jobs=args.max_concurrent,
            )

            try:
                scanner.scan()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                print("\n\nInterrupted by user")
                sys.exit(1)

    else:
        # Single run mode
        scanner = WorkflowScanner(
            workflow_file=args.workflow,
            run_id=args.run_id,
            context_lines=args.context,
            max_concurrent_jobs=args.max_concurrent,
        )

        try:
            scanner.scan()
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

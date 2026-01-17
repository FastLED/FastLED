from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
GitHub Actions Health Check

Analyzes the latest workflow run and provides a comprehensive health report
with error analysis, root cause identification, and recommendations.

Usage:
    uv run ci/tools/gh/gh_healthcheck.py
    uv run ci/tools/gh/gh_healthcheck.py --workflow build.yml
    uv run ci/tools/gh/gh_healthcheck.py --run-id 12345678
    uv run ci/tools/gh/gh_healthcheck.py --detail high

Features:
    - Scans latest workflow run for failures
    - Groups errors by root cause
    - Provides structured analysis and recommendations
    - Multiple detail levels (low, medium, high)
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class ErrorPattern:
    """Pattern for matching and categorizing errors."""

    pattern: str
    category: str
    description: str
    suggestion: str


@dataclass
class JobFailure:
    """Information about a failed job."""

    job_id: str
    job_name: str
    conclusion: str
    errors: list[str]


class HealthChecker:
    """Analyzes GitHub Actions workflow health."""

    # Error patterns with categorization
    ERROR_PATTERNS = [
        ErrorPattern(
            pattern=r"fatal error: ([^:]+): No such file or directory",
            category="Missing Header",
            description="Missing include file",
            suggestion="Check if header exists for this platform/version",
        ),
        ErrorPattern(
            pattern=r"undefined reference to [`']([^']+)'",
            category="Linker Error",
            description="Undefined symbol",
            suggestion="Missing implementation or library dependency",
        ),
        ErrorPattern(
            pattern=r"error: '([^']+)' was not declared",
            category="Undeclared Identifier",
            description="Symbol not found in scope",
            suggestion="Check includes and namespace usage",
        ),
        ErrorPattern(
            pattern=r"PlatformIO command failed.*platform.*show.*returned non-zero",
            category="Platform Resolution",
            description="PlatformIO cannot resolve platform",
            suggestion="Platform may be deprecated or unavailable",
        ),
        ErrorPattern(
            pattern=r"compilation terminated",
            category="Compilation Fatal",
            description="Compilation stopped due to errors",
            suggestion="Fix preceding errors",
        ),
    ]

    def __init__(
        self,
        workflow_file: str = "build.yml",
        run_id: Optional[str] = None,
        detail_level: str = "medium",
    ):
        """Initialize health checker.

        Args:
            workflow_file: Workflow filename
            run_id: Specific run ID (if None, uses latest)
            detail_level: Amount of detail (low, medium, high)
        """
        self.workflow_file = workflow_file
        self.run_id = run_id
        self.detail_level = detail_level
        self.repo = self._get_repo()

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
            return "FastLED/FastLED"

    def get_latest_run(self) -> Optional[dict[str, Any]]:
        """Get the latest workflow run."""
        try:
            cmd = [
                "gh",
                "run",
                "list",
                "--workflow",
                self.workflow_file,
                "--json",
                "databaseId,displayTitle,status,conclusion,createdAt,headBranch,event",
                "--limit",
                "1",
            ]

            result = subprocess.run(
                cmd, capture_output=True, text=True, check=True, timeout=30
            )
            runs = json.loads(result.stdout)
            return runs[0] if runs else None
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting workflow runs: {e}", file=sys.stderr)
            return None

    def get_run_info(self, run_id: str) -> Optional[dict[str, Any]]:
        """Get detailed run information."""
        try:
            result = subprocess.run(
                [
                    "gh",
                    "run",
                    "view",
                    run_id,
                    "--json",
                    "displayTitle,status,conclusion,createdAt,headBranch,event,jobs",
                ],
                capture_output=True,
                text=True,
                check=True,
                timeout=30,
            )
            return json.loads(result.stdout)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error getting run info: {e}", file=sys.stderr)
            return None

    def get_job_logs(self, job_id: str, max_lines: int = 1000) -> list[str]:
        """Fetch logs for a specific job (only last N lines for efficiency).

        Args:
            job_id: GitHub job ID
            max_lines: Maximum lines to fetch (default 1000)

        Returns:
            List of log lines
        """
        try:
            api_path = f"/repos/{self.repo}/actions/jobs/{job_id}/logs"
            cmd = ["gh", "api", api_path]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False,  # Don't raise on error
                timeout=30,  # Shorter timeout
            )

            if result.returncode != 0:
                return []

            # Only keep last N lines (where errors usually are)
            lines = result.stdout.splitlines()
            return lines[-max_lines:] if len(lines) > max_lines else lines
        except subprocess.TimeoutExpired:
            print(f"⚠️  Timeout fetching logs for job {job_id}", file=sys.stderr)
            return []
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"⚠️  Error fetching job logs: {e}", file=sys.stderr)
            return []

    def analyze_logs(self, logs: list[str]) -> list[tuple[str, str, str]]:
        """Analyze logs and extract categorized errors.

        Args:
            logs: List of log lines

        Returns:
            List of (category, error_line, context) tuples
        """
        errors: list[tuple[str, str, str]] = []

        # Search for errors with context
        for i, line in enumerate(logs):
            line_lower = line.lower()

            # Check for error keywords
            if "error" in line_lower or "fatal" in line_lower:
                # Categorize the error
                category = "Generic Error"
                for pattern_obj in self.ERROR_PATTERNS:
                    if re.search(pattern_obj.pattern, line, re.IGNORECASE):
                        category = pattern_obj.category
                        break

                # Extract context (±3 lines)
                start = max(0, i - 3)
                end = min(len(logs), i + 4)
                context = "\n".join(logs[start:end])

                errors.append((category, line, context))

        return errors

    def group_errors_by_category(
        self, errors: list[tuple[str, str, str]]
    ) -> dict[str, list[tuple[str, str]]]:
        """Group errors by category.

        Args:
            errors: List of (category, error_line, context) tuples

        Returns:
            Dictionary mapping category to list of (error_line, context) tuples
        """
        grouped: dict[str, list[tuple[str, str]]] = defaultdict(list)

        for category, error_line, context in errors:
            grouped[category].append((error_line, context))

        return dict(grouped)

    def extract_root_causes(
        self, grouped_errors: dict[str, list[tuple[str, str]]]
    ) -> list[dict[str, Any]]:
        """Extract root causes from grouped errors.

        Args:
            grouped_errors: Errors grouped by category

        Returns:
            List of root cause dictionaries
        """
        root_causes: list[dict[str, Any]] = []

        for category, errors in grouped_errors.items():
            # Find the pattern info
            pattern_info = next(
                (p for p in self.ERROR_PATTERNS if p.category == category), None
            )

            # Extract specific details (e.g., missing file names)
            details: set[str] = set()
            for error_line, _ in errors:
                # Extract missing file for header errors
                if category == "Missing Header":
                    match = re.search(r"fatal error: ([^:]+): No such file", error_line)
                    if match:
                        details.add(match.group(1))
                # Extract undefined symbols
                elif category == "Linker Error":
                    match = re.search(
                        r"undefined reference to [`']([^']+)'", error_line
                    )
                    if match:
                        details.add(match.group(1))

            root_cause = {
                "category": category,
                "count": len(errors),
                "description": pattern_info.description if pattern_info else category,
                "suggestion": (
                    pattern_info.suggestion if pattern_info else "Review error details"
                ),
                "details": list(details),
            }
            root_causes.append(root_cause)

        return root_causes

    def print_health_report(
        self, run_info: dict[str, Any], job_failures: list[JobFailure]
    ) -> None:
        """Print comprehensive health report.

        Args:
            run_info: Run information dictionary
            job_failures: List of failed jobs with their errors
        """
        print("\n" + "=" * 80)
        print("GitHub Actions Health Check Report")
        print("=" * 80)

        # Run summary
        print("\n📋 Run Information:")
        print(f"  Run ID: {self.run_id}")
        print(f"  Title: {run_info.get('displayTitle', 'Unknown')}")
        print(f"  Branch: {run_info.get('headBranch', 'Unknown')}")
        print(f"  Event: {run_info.get('event', 'Unknown')}")
        print(f"  Status: {run_info.get('status', 'Unknown')}")
        print(f"  Conclusion: {run_info.get('conclusion', 'Unknown')}")

        # Job summary
        jobs = run_info.get("jobs", [])
        total_jobs = len(jobs)
        failed_jobs = len(job_failures)
        passed_jobs = total_jobs - failed_jobs

        print("\n📊 Job Summary:")
        print(f"  Total Jobs: {total_jobs}")
        print(f"  ✅ Passed: {passed_jobs}")
        print(f"  ❌ Failed: {failed_jobs}")

        if not job_failures:
            print("\n✨ All jobs passed! Workflow is healthy.")
            return

        # Failed jobs detail
        print(f"\n❌ Failed Jobs ({failed_jobs}):")
        for job in job_failures:
            print(f"  - {job.job_name} ({job.conclusion})")

        # Root cause analysis
        print("\n🔍 Root Cause Analysis:")

        # Aggregate all errors
        all_errors: list[tuple[str, str, str]] = []
        for job in job_failures:
            # Parse job errors (stored as strings)
            for error_str in job.errors:
                # Simple categorization
                category = "Generic Error"
                for pattern_obj in self.ERROR_PATTERNS:
                    if re.search(pattern_obj.pattern, error_str, re.IGNORECASE):
                        category = pattern_obj.category
                        break
                all_errors.append((category, error_str, error_str))

        grouped = self.group_errors_by_category(all_errors)
        root_causes = self.extract_root_causes(grouped)

        # Sort by count (most common first)
        root_causes.sort(key=lambda x: x["count"], reverse=True)

        for i, cause in enumerate(root_causes, 1):
            print(f"\n  {i}. {cause['category']} ({cause['count']} occurrences)")
            print(f"     Description: {cause['description']}")
            print(f"     💡 Suggestion: {cause['suggestion']}")

            if cause["details"] and self.detail_level in ["medium", "high"]:
                print("     Affected:")
                for detail in list(cause["details"])[:5]:  # Show max 5
                    print(f"       - {detail}")

        # Detailed errors (only in high detail mode)
        if self.detail_level == "high":
            print("\n📝 Detailed Error Log:")
            for job in job_failures:
                if job.errors:
                    print(f"\n  Job: {job.job_name}")
                    for i, error in enumerate(job.errors[:3], 1):  # Show first 3
                        print(f"    Error {i}:")
                        for line in error.split("\n")[:5]:  # Show first 5 lines
                            print(f"      {line}")

        # Recommendations
        print("\n💡 Recommendations:")

        # Check for common issues
        has_missing_headers = any(
            c["category"] == "Missing Header" for c in root_causes
        )
        has_platform_errors = any(
            c["category"] == "Platform Resolution" for c in root_causes
        )

        if has_missing_headers:
            print(
                "  1. Missing header files detected - likely platform/version compatibility issue"
            )
            print("     Consider using conditional includes: #if __has_include(...)")

        if has_platform_errors:
            print("  2. PlatformIO platform resolution failures detected")
            print("     Platform may be deprecated - consider upgrading or removing")

        if failed_jobs == 1:
            print("  3. Only one job failed - issue may be platform-specific")
        elif failed_jobs == total_jobs:
            print("  4. All jobs failed - likely a fundamental build system issue")

        print("\n" + "=" * 80)

    def run_healthcheck(self) -> int:
        """Run the health check.

        Returns:
            Exit code (0 = healthy, 1 = failures detected)
        """
        # Get run to analyze
        if self.run_id:
            run_info = self.get_run_info(self.run_id)
            if not run_info:
                print(f"❌ Could not fetch run {self.run_id}", file=sys.stderr)
                return 1
        else:
            # Get latest run
            latest = self.get_latest_run()
            if not latest:
                print("❌ No workflow runs found", file=sys.stderr)
                return 1

            self.run_id = str(latest["databaseId"])
            run_info = self.get_run_info(self.run_id)
            if not run_info:
                print(f"❌ Could not fetch run {self.run_id}", file=sys.stderr)
                return 1

        # Analyze jobs
        jobs = run_info.get("jobs", [])
        job_failures: list[JobFailure] = []

        for i, job in enumerate(jobs):
            if job.get("conclusion") in ["failure", "cancelled", "timed_out"]:
                job_id = str(job.get("databaseId", ""))
                job_name = job.get("name", "Unknown")
                conclusion = job.get("conclusion", "")

                # Fetch and analyze logs
                if self.detail_level in ["medium", "high"]:
                    print(
                        f"  Analyzing job {i + 1}/{len(jobs)}: {job_name}...",
                        file=sys.stderr,
                    )
                    logs = self.get_job_logs(job_id)
                    errors_tuples = self.analyze_logs(logs)
                    # Store just error lines for now
                    errors = [error_line for _, error_line, _ in errors_tuples[:10]]
                else:
                    errors = []

                job_failures.append(
                    JobFailure(
                        job_id=job_id,
                        job_name=job_name,
                        conclusion=conclusion,
                        errors=errors,
                    )
                )

        # Print report
        self.print_health_report(run_info, job_failures)

        # Return exit code
        return 1 if job_failures else 0


def main():
    parser = argparse.ArgumentParser(
        description="Health check for GitHub Actions workflows",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Check latest build.yml run
    %(prog)s

    # Check specific workflow
    %(prog)s --workflow build.yml

    # Check specific run
    %(prog)s --run-id 12345678

    # High detail mode
    %(prog)s --detail high
        """,
    )

    parser.add_argument(
        "--workflow",
        default="build.yml",
        help="Workflow filename (default: build.yml)",
    )
    parser.add_argument(
        "--run-id",
        help="Specific run ID to analyze (default: latest)",
    )
    parser.add_argument(
        "--detail",
        choices=["low", "medium", "high"],
        default="medium",
        help="Detail level (default: medium)",
    )

    args = parser.parse_args()

    checker = HealthChecker(
        workflow_file=args.workflow,
        run_id=args.run_id,
        detail_level=args.detail,
    )

    try:
        exit_code = checker.run_healthcheck()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

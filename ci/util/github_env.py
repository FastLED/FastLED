#!/usr/bin/env python3
"""GitHub Actions environment detection utilities."""

import os


def is_github_actions() -> bool:
    """Check if currently running on GitHub Actions runner.

    On GitHub Actions runners, we should use --local to avoid pulling
    Docker images, since:
    1. Images may already be built locally in the runner
    2. Pulling wastes time and bandwidth
    3. Native compilation is often acceptable in CI

    Returns:
        True if running on GitHub Actions, False otherwise
    """
    return os.getenv("GITHUB_ACTIONS") == "true"

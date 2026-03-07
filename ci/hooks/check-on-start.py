#!/usr/bin/env python3
"""Claude Code SessionStart hook that captures the repo state fingerprint."""

import hashlib
import json
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
SESSION_FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "session_fingerprint.json"


def run_cmd(cmd):
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )


def get_current_fingerprint():
    result = run_cmd(["git", "status", "--porcelain"])
    if result.returncode != 0 or not result.stdout.strip():
        return None
    return hashlib.md5(result.stdout.encode()).hexdigest()


def save_session_fingerprint(fingerprint):
    SESSION_FINGERPRINT_FILE.parent.mkdir(parents=True, exist_ok=True)
    SESSION_FINGERPRINT_FILE.write_text(
        json.dumps(
            {
                "fingerprint": fingerprint,
                "description": "Captured at session start",
            }
        )
    )


def main():
    fingerprint = get_current_fingerprint()
    if fingerprint is None:
        if SESSION_FINGERPRINT_FILE.exists():
            SESSION_FINGERPRINT_FILE.unlink()
        return 0
    save_session_fingerprint(fingerprint)
    return 0


if __name__ == "__main__":
    sys.exit(main())

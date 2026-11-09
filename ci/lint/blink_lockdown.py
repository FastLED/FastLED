#!/usr/bin/env python3
"""Freeze `examples/Blink/Blink.ino` against casual edits.

Blink is the canonical first sketch, the size-check baseline and the default
compile target, so its exact structure matters. Agents have repeatedly
rewritten it as a throwaway probe for an unrelated bug. Probes belong in
`examples/AutoResearch/AutoResearch.ino` or in a scratch export of the repo,
never in Blink.

The check compares the sketch's SHA-256 (CRLF normalised to LF) with
``FROZEN_BLINK_SHA256``. To change Blink on purpose, edit the sketch and
update the hash below in the same PR, so the change is visible in review.
"""

from __future__ import annotations

import hashlib
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
BLINK_PATH = PROJECT_ROOT / "examples" / "Blink" / "Blink.ino"

# Update only together with a deliberate, reviewed edit to Blink.ino.
FROZEN_BLINK_SHA256 = "61a676f3ec0c00d0dc754a6c2317cb64f2a2b7ac15c4d85021c8dd35ab85b661"


def blink_sha256(path: Path = BLINK_PATH) -> str:
    """Return the LF-normalised SHA-256 of the sketch."""
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def run() -> bool:
    """Return True when Blink.ino matches the frozen hash."""
    if not BLINK_PATH.exists():
        print(f"[blink-lockdown] ERROR: {BLINK_PATH} is missing.")
        return False
    found = blink_sha256()
    if found == FROZEN_BLINK_SHA256:
        print("✅ Blink lockdown: examples/Blink/Blink.ino matches its frozen hash.")
        return True
    print(
        "[blink-lockdown] ERROR: examples/Blink/Blink.ino changed.\n"
        "Blink is frozen: do not use it as a probe sketch. Revert it with\n"
        "  git checkout -- examples/Blink/Blink.ino\n"
        "and test in examples/AutoResearch/AutoResearch.ino or a scratch export.\n"
        "For a deliberate, reviewed change, set FROZEN_BLINK_SHA256 in\n"
        f"ci/lint/blink_lockdown.py to {found} in the same PR."
    )
    return False


def main() -> int:
    return 0 if run() else 1


if __name__ == "__main__":
    sys.exit(main())

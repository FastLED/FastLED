---
description: Run test suite
argument-hint: [test-name] [--cpp] [--no-fingerprint] [--qemu platform]
---

Run tests using `uv run test.py` with optional arguments.

Arguments: ${1:-} ${2:-} ${3:-} ${4:-}

Use the 'test-agent' sub-agent to execute tests and report results in a clear, actionable format.

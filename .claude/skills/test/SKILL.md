---
name: test
description: Run test suite and report results. Use when running unit tests, integration tests, or verifying code changes pass all tests.
argument-hint: [test-name] [--cpp] [--clean] [--run platform]
context: fork
agent: test-agent
---

Run tests using `uv run test.py` with optional arguments.

Arguments: $ARGUMENTS

Use `bash test` wrapper (NEVER bare `python`). Common invocations:
- `bash test` — run all tests
- `bash test --cpp` — run only C++ tests
- `bash test TestName` — run a specific test
- `bash test --clean` — clean build then test

Report results in a clear, actionable format showing pass/fail counts and any error details.

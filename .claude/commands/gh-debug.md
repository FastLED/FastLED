---
description: Debug GitHub Actions build failures
argument-hint: [run-id or workflow-url]
---

Pull GitHub Actions logs for a workflow run, parse them to identify errors, and provide a clear diagnostic report.

Arguments: ${1:-}

**Primary Method (Recommended)**: Use the Python script for efficient log analysis:
```bash
uv run ci/tools/gh_debug.py ${1:-}
```

This script:
- Streams logs instead of downloading full files (avoids 55MB+ downloads)
- Filters for errors in real-time
- Stops after finding 10 errors (configurable with --max-errors)
- Shows context around each error (5 lines before/after, configurable with --context)
- Handles timeouts gracefully

**Fallback Method**: If the Python script fails, use the 'gh-debug-agent' sub-agent with:

## Smart Log Fetching Strategy (AVOID downloading full 55MB+ logs)

1. **First, identify failed jobs/steps** using `gh run view <run-id> --log-failed`
   - This shows only logs from failed steps (much smaller)
   - Parse output to identify which jobs and steps failed

2. **For each failed step, use targeted log extraction**:
   - **IMPORTANT**: Filter BEFORE limiting to avoid processing huge logs
   - Use `gh api /repos/FastLED/FastLED/actions/jobs/{job_id}/logs | grep -E "(error:|FAILED|Assertion|undefined reference|Error compiling|Test.*failed)" -A 10 -B 5 | tail -n 500`
   - This filters first (fast), then limits output (avoids timeout)
   - Prioritize steps named "Build summary and failure logs" as they contain consolidated error info
   - Use timeout of 3 minutes for log fetching commands

3. **Parse logs to identify**:
   - Compilation errors (look for "error:", "undefined reference", "no such file")
   - Test failures (look for "FAILED", "Assertion failed", exit codes)
   - Runtime issues (segfaults, exceptions, timeouts)

4. **Extract error context**:
   - Get ~10 lines before and after each error
   - Capture file paths, line numbers, and error messages
   - Identify patterns (multiple similar errors = systematic issue)

## Input Handling

The agent should handle:
- Run IDs (e.g., "18391541037")
- Workflow URLs (e.g., "https://github.com/FastLED/FastLED/actions/runs/18391541037")
- Most recent failed run if no argument provided (use `gh run list --status failure --limit 1`)

## Output Format

Provide:
- Workflow name and run number
- Job(s) that failed with step names
- Specific error messages with surrounding context (max ~50 lines per error)
- File paths and line numbers where applicable
- Suggested next steps or potential fixes

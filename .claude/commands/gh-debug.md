---
description: Debug GitHub Actions build failures
argument-hint: [run-id or workflow-url]
---

Pull GitHub Actions logs for a workflow run, parse them to identify errors, and provide a clear diagnostic report.

Arguments: ${1:-}

Use the 'gh-debug-agent' sub-agent to:

## Smart Log Fetching Strategy (AVOID downloading full 55MB+ logs)

1. **First, identify failed jobs/steps** using `gh run view <run-id> --log-failed`
   - This shows only logs from failed steps (much smaller)
   - Parse output to identify which jobs and steps failed

2. **For each failed step, use targeted log extraction**:
   - Use `gh run view <run-id> --log --job <job-id>` and pipe through `tail -n 1000` to get last 1000 lines
   - Or use `gh api` with `/repos/{owner}/{repo}/actions/jobs/{job_id}/logs` and extract tail
   - Prioritize steps named "Build summary and failure logs" as they contain consolidated error info

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

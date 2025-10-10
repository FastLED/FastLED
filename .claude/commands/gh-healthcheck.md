---
description: Run health check on GitHub Actions workflow
argument-hint: [--workflow build.yml] [--run-id <id>] [--detail low|medium|high]
---

Analyze GitHub Actions workflow health and provide a comprehensive diagnostic report with root cause analysis and recommendations.

Arguments: ${1:-}

Run the GitHub Actions health check tool:

```bash
uv run ci/tools/gh/gh_healthcheck.py ${1:-}
```

This health check tool:
- Analyzes the latest workflow run (or specific run-id if provided)
- Identifies all failed jobs
- Groups errors by root cause category
- Provides structured analysis and recommendations
- Supports multiple detail levels (low, medium, high)

## Features

**Root Cause Analysis**:
- Groups errors by category (Missing Headers, Linker Errors, Platform Issues, etc.)
- Identifies patterns across multiple failed jobs
- Extracts specific details (missing files, undefined symbols)

**Structured Reporting**:
- Run summary with job statistics
- Failed job listing
- Categorized error analysis with occurrence counts
- Actionable recommendations based on error patterns

**Detail Levels**:
- `low`: Summary only, no log fetching (fast)
- `medium`: Fetches logs and categorizes errors (default)
- `high`: Full error details with context

## Usage Examples

**Check latest build.yml run**:
```bash
uv run ci/tools/gh/gh_healthcheck.py
```

**Check specific run**:
```bash
uv run ci/tools/gh/gh_healthcheck.py --run-id 18399875461
```

**High detail analysis**:
```bash
uv run ci/tools/gh/gh_healthcheck.py --detail high
```

**Check different workflow**:
```bash
uv run ci/tools/gh/gh_healthcheck.py --workflow test.yml
```

## Output Format

The tool provides:
1. **Run Information**: Title, branch, event, status, conclusion
2. **Job Summary**: Total, passed, failed job counts
3. **Root Cause Analysis**: Categorized errors with counts and suggestions
4. **Recommendations**: Actionable steps based on detected patterns

## Exit Codes

- `0`: Workflow healthy, all jobs passed
- `1`: Failures detected or errors occurred

## When to Use

- **Daily health checks**: Monitor workflow status
- **After commits**: Verify builds are passing
- **Debugging failures**: Quick overview of what's broken
- **Pattern analysis**: Identify systematic issues across jobs

## Comparison with /gh-debug

- **gh-healthcheck**: Overview and analysis of entire workflow
- **gh-debug**: Deep dive into specific run with full error context

Use gh-healthcheck for quick status and root cause identification.
Use gh-debug when you need detailed error logs and context.

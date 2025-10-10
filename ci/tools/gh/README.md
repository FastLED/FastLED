# GitHub Actions Debugging Tools

Tools for analyzing and debugging GitHub Actions workflow failures.

## Tools

### 1. `workflow_scanner.py` - Workflow Failure Scanner

Scans GitHub Actions workflow runs and analyzes all failing jobs. Designed to prevent stdout buffer overflow by streaming output serially while fetching logs concurrently.

#### Features

- **Automatic workflow scanning**: Analyzes latest or specific workflow runs
- **Concurrent log fetching**: Fetches multiple job logs in parallel for speed
- **Serial streaming output**: Outputs results one job at a time to prevent buffer overflow
- **Error filtering**: Filters logs for "error" and "fatal" keywords (case insensitive)
- **Context extraction**: Grabs ±20 lines (configurable) around each error
- **Progress tracking**: Shows which jobs are being processed

#### Usage

```bash
# Scan latest build.yml run (default)
uv run ci/tools/gh/workflow_scanner.py

# Scan specific workflow
uv run ci/tools/gh/workflow_scanner.py --workflow build.yml

# Scan specific run by ID
uv run ci/tools/gh/workflow_scanner.py --run-id 12345678

# Scan with more context lines
uv run ci/tools/gh/workflow_scanner.py --context 30

# Scan last 5 runs
uv run ci/tools/gh/workflow_scanner.py --max-runs 5

# Limit concurrent job processing
uv run ci/tools/gh/workflow_scanner.py --max-concurrent 3
```

#### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--workflow` | `build.yml` | Workflow filename to scan |
| `--run-id` | Latest | Specific run ID to analyze |
| `--context` | `20` | Lines of context before/after errors |
| `--max-concurrent` | `5` | Maximum concurrent job processing |
| `--max-runs` | `1` | Number of recent runs to scan (if no run-id) |

#### Output Format

The tool outputs error blocks in the following format:

```
================================================================================
Error #1 in job: build (ubuntu-latest, gcc) (ID: 12345678)
================================================================================
    [20 lines of context before]
>>> line containing "error" or "fatal" keyword
    [20 lines of context after]

================================================================================
Error #2 in job: test (windows-latest) (ID: 12345679)
================================================================================
    [context lines...]
>>> error line
    [context lines...]
```

Lines containing the error keyword are prefixed with `>>>` for easy identification.

#### Architecture

```
WorkflowScanner
├── Workflow Discovery
│   ├── get_workflow_runs()    - Get runs for specified workflow
│   └── get_failed_jobs()       - Filter for failed/cancelled jobs
│
├── Log Processing (Worker Threads)
│   ├── fetch_and_filter_job_logs()  - Download logs via gh API
│   ├── Filter for error/fatal keywords
│   └── Extract ±N lines of context
│
└── Output Streaming (Main Thread)
    ├── output_queue              - Thread-safe queue
    └── stream_output()           - Serial stdout writing
```

#### How It Prevents Buffer Overflow

1. **Concurrent Fetching**: Worker threads fetch job logs in parallel (default: 5 concurrent)
2. **Queue-based Communication**: Workers put filtered results into a thread-safe queue
3. **Serial Output**: Main thread reads from queue and outputs one error block at a time
4. **Controlled Flow**: Output keeps pace with processing, preventing memory buildup

#### Error Detection

The tool searches for these keywords (case insensitive):
- `error`
- `fatal`

When found, it extracts the surrounding context (default ±20 lines) and outputs the block immediately.

#### Requirements

- `gh` CLI tool installed and authenticated
- Python 3.8+
- Repository must be a git repository

#### Examples

**Example 1: Scan latest build.yml run**
```bash
uv run ci/tools/gh/workflow_scanner.py
```

**Example 2: Deep dive into a specific failing run**
```bash
uv run ci/tools/gh/workflow_scanner.py --run-id 18397776636 --context 30
```

**Example 3: Scan last 3 runs to find pattern of failures**
```bash
uv run ci/tools/gh/workflow_scanner.py --max-runs 3
```

### 2. `gh_healthcheck.py` - Workflow Health Check

Analyzes GitHub Actions workflow health and provides a comprehensive diagnostic report with root cause analysis and recommendations.

#### Features

- **Quick health assessment**: Fast overview of workflow status
- **Root cause analysis**: Groups errors by category (Missing Headers, Linker Errors, Platform Issues)
- **Pattern detection**: Identifies systematic issues across multiple jobs
- **Actionable recommendations**: Suggests fixes based on detected patterns
- **Multiple detail levels**: Choose speed vs. depth of analysis

#### Usage

```bash
# Check latest build.yml run
uv run ci/tools/gh/gh_healthcheck.py

# Check specific run
uv run ci/tools/gh/gh_healthcheck.py --run-id 18399875461

# High detail analysis (slower, more info)
uv run ci/tools/gh/gh_healthcheck.py --detail high

# Quick check (fast, summary only)
uv run ci/tools/gh/gh_healthcheck.py --detail low
```

#### Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--workflow` | filename | `build.yml` | Workflow to analyze |
| `--run-id` | run ID | latest | Specific run to analyze |
| `--detail` | low/medium/high | `medium` | Analysis depth |

#### Detail Levels

- **low**: Summary only, no log fetching (⚡ fastest)
- **medium**: Fetches logs and categorizes errors (⚖️ balanced)
- **high**: Full error details with context (🐌 slowest)

#### Output Example

```
================================================================================
GitHub Actions Health Check Report
================================================================================

📋 Run Information:
  Run ID: 18399875461
  Title: feat(compile_perf): add support for tracking header include tree
  Branch: master
  Status: completed
  Conclusion: failure

📊 Job Summary:
  Total Jobs: 17
  ✅ Passed: 13
  ❌ Failed: 4

❌ Failed Jobs (4):
  - build_esp32dev_idf33 / build (failure)
  - build_esp32_c3_devkitm_1 / build (failure)
  ...

🔍 Root Cause Analysis:

  1. Missing Header (15 occurrences)
     Description: Missing include file
     💡 Suggestion: Check if header exists for this platform/version
     Affected:
       - soc/soc_caps.h
       - esp_system.h

💡 Recommendations:
  1. Missing header files detected - likely platform/version compatibility issue
     Consider using conditional includes: #if __has_include(...)
```

#### Exit Codes

- `0`: Workflow healthy (all jobs passed)
- `1`: Failures detected

#### Slash Command

Use the `/gh-healthcheck` command for quick access:
```
/gh-healthcheck
/gh-healthcheck --run-id 18399875461
/gh-healthcheck --detail high
```

---

### 3. `gh_debug.py` - Single Run Debugger

Located at `ci/tools/gh_debug.py`, this tool provides deep analysis of a specific workflow run with full error context.

See the tool's help for usage:
```bash
uv run ci/tools/gh_debug.py --help
```

#### When to Use Which Tool

| Tool | Use Case | Speed | Detail |
|------|----------|-------|--------|
| `gh_healthcheck.py` | Quick status check, pattern analysis | ⚡ Fast | Overview + recommendations |
| `workflow_scanner.py` | Stream all errors from workflow | ⚖️ Medium | All errors with context |
| `gh_debug.py` | Deep dive into specific failure | 🐌 Slow | Full logs with context |

## Development

### Adding New Tools

1. Create new `.py` file in `ci/tools/gh/`
2. Add class/functions following existing patterns
3. Update `__init__.py` to export public API
4. Document in this README

### Testing

```bash
# Test with a known failing run
uv run ci/tools/gh/workflow_scanner.py --run-id <failing-run-id>

# Test with latest run
uv run ci/tools/gh/workflow_scanner.py
```

## Troubleshooting

**"No workflow runs found"**
- Verify the workflow filename is correct (default: `build.yml`)
- Check that the workflow has run at least once
- Ensure `gh` CLI is authenticated: `gh auth status`

**"Error getting repo info"**
- Make sure you're in a git repository
- Check that `gh` CLI is installed: `gh --version`
- Authenticate: `gh auth login`

**Timeout errors**
- Increase timeout in the code if dealing with very large log files
- Reduce `--max-concurrent` to process fewer jobs at once

**No errors found but jobs failed**
- Try increasing `--context` to capture more lines
- The job may have failed for reasons other than error/fatal keywords
- Use the existing `gh_debug.py` tool for more detailed analysis

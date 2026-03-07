# Smart on_stop Hook Setup

Smart fingerprinting has been integrated into your existing on_stop hook at `~/.claude/hooks/check-on-stop.py`. No setup required!

## Problem

The `on_stop` hook runs every time an agent session ends, even if:
- Agent made no code changes
- Agent ran but made the same changes as a previous run (redundant execution)

This is wasteful for expensive operations like:
- Code formatting/linting
- Test runs
- Artifact generation
- Cleanup commands

## Solution: Git-Based Fingerprinting

Uses `git status --porcelain` to fingerprint the current repo state. Only executes hook if:
1. **Repo is dirty** (has actual changes)
2. **Fingerprint differs** from last run (new changes detected)

**Fingerprint storage:** `.cache/last_agent_changes_fingerprint`

## Implementation Location

**`~/.claude/hooks/check-on-stop.py`** - Your existing on_stop hook now includes:
- `get_current_fingerprint()` - Get MD5 of `git status --porcelain`
- `should_skip_hook()` - Determine if lint+tests should run
- Smart decision before expensive operations

**No configuration changes needed** - Claude Code already uses this hook!

## How It Works

### First Run
```
Agent session ends
→ Check git status --porcelain (has changes)
→ Save fingerprint: MD5("M  src/file.cpp\nA  tests/new.cpp")
→ Run hook (first_run)
```

### Second Run (Same Changes)
```
Agent session ends
→ Check git status --porcelain (has changes)
→ Compute fingerprint: MD5("M  src/file.cpp\nA  tests/new.cpp")
→ Compare with stored: MATCH ✓
→ Skip hook (same_changes) ⏭️
```

### Third Run (New Changes)
```
Agent session ends
→ Check git status --porcelain (different changes)
→ Compute fingerprint: MD5("M  src/file2.cpp\nD  tests/old.cpp")
→ Compare with stored: DIFFERENT ✗
→ Save new fingerprint
→ Run hook (new_changes) 🔧
```

### Clean Repo
```
Agent session ends
→ Check git status --porcelain (CLEAN)
→ Skip hook (no_changes) ⏭️
```

## Reasons

The hook prints one of:
- `no_changes` - Repository is clean (no modifications)
- `first_run` - First time seeing this fingerprint
- `same_changes` - Fingerprint unchanged since last hook
- `new_changes` - Fingerprint differs from last hook
- `error` - Git error (runs hook to be safe)

## Hook Behavior

The hook runs **lint and C++ tests in parallel** when:
- Repository has actual changes
- Changes differ from the last hook run

If lint fails, tests are cancelled and only lint errors are shown. Otherwise, both results are reported.

**Output messages:**
- `⏭️  Skipping lint+tests (no new changes)` - Fingerprint unchanged, hook skipped
- `🔧 Running lint and tests (new changes detected)` - Fingerprint differs, hook running

## Benefits

| Scenario | Before | After |
|----------|--------|-------|
| Agent makes no changes | Hook runs (wasted) | Hook skipped ⏭️ |
| Agent makes same changes twice | Hook runs twice (wasteful) | Hook runs once ⏭️ |
| Agent makes new changes | Hook runs ✓ | Hook runs ✓ |
| Repo is committed (clean) | Hook runs (wasted) | Hook skipped ⏭️ |

## Implementation Notes

- **Git dependency**: Requires `git` command available in PATH
- **Repo root detection**: Uses `Path(__file__).parent.parent.parent` to locate repo
- **Fingerprint format**: MD5 hash of `git status --porcelain` output (includes untracked files)
- **Cache location**: `.cache/` directory (created automatically, not committed)
- **Error handling**: If git fails, hook runs (safer than skipping)

## Debugging

Check what fingerprint was recorded:

```bash
cat .cache/last_agent_changes_fingerprint
# Output: {"fingerprint": "a1b2c3d4e5f6..."}
```

See current status:

```bash
git status --porcelain
```

## Troubleshooting

### Hook never runs (always skipped)
- Check `.cache/last_agent_changes_fingerprint` exists
- Verify `git status --porcelain` shows changes
- If fingerprint is the same, that's expected (same_changes - working correctly!)

### Hook always runs
- Check if you're making actual changes each time
- Verify git status actually differs between runs
- Clear cached fingerprint: `rm .cache/last_agent_changes_fingerprint`

### Permission errors with hook
- Ensure `~/.claude/hooks/check-on-stop.py` is executable
- Ensure `.cache/` directory is writable by Claude Code

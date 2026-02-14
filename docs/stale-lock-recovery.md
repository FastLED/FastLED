# Stale Lock Detection and Recovery

## Problem

When build processes are killed (Ctrl+C, task manager, etc.), lock files can be left behind without PID metadata. This happens when a process:

1. Acquires the lock file (creates `.lock`)
2. Gets killed BEFORE writing PID metadata (`.lock.pid`)
3. Leaves an orphaned lock file that blocks future builds

**Previous behavior:** Age-based fallback only triggered after 30 minutes, causing long build delays.

## Solution

### Changes Made

#### 1. Reduced Age Threshold (file_lock_rw.py)
- **Before:** 30 minutes for locks without metadata
- **After:** 2 minutes for locks without metadata
- **Rationale:** Faster recovery from killed processes, more practical threshold

```python
# Old threshold
if age_seconds > 1800:  # 30 minutes

# New threshold
if age_seconds > 120:  # 2 minutes
```

#### 2. Immediate Stale Lock Check (build_lock.py)
- Added stale lock check at START of lock acquisition (before wait loop)
- Prevents immediate blocking on stale locks from previous runs
- Removes stale locks proactively rather than waiting for timeout

```python
# Check for stale lock BEFORE attempting acquisition
if self.lock_file.exists():
    if self._check_stale_lock():
        print(f"Removed stale lock before acquisition: {self.lock_file}")
```

#### 3. Improved Logging
- Better diagnostic messages for stale lock detection
- Clearer indication when locks are from killed processes
- Debug-level logging for recent locks (reduces noise)

## How It Works

### Lock Metadata Files

Each lock has two files:
- **`.lock`** - Actual lock file (managed by fasteners library)
- **`.lock.pid`** - PID metadata (JSON with PID, timestamp, operation, hostname)

### Stale Detection Logic

1. **With PID metadata:** Check if process with that PID is still alive
   - Uses `os.kill(pid, 0)` on Unix
   - Uses `OpenProcess()` on Windows
   - If process is dead → lock is stale

2. **Without PID metadata:** Use age-based heuristic
   - Lock older than 2 minutes → stale (likely from killed process)
   - Lock younger than 2 minutes → assume active (may be in progress)

### Recovery Process

1. **At acquisition start:** Check if lock exists and is stale
2. **During acquisition wait:** Check periodically (every 1 second)
3. **If stale detected:** Remove `.lock` and `.lock.pid` files
4. **Retry acquisition:** Attempt to acquire immediately after removal

## Testing

Test script demonstrates two scenarios:

```bash
uv run python test_stale_lock.py
```

**Test 1:** Stale lock (3 minutes old, no metadata)
- ✅ Detected as stale
- ✅ Removed automatically
- ✅ New lock acquired in <0.1s

**Test 2:** Recent lock (30 seconds old, no metadata)
- ✅ Acquirable if no process holds it (OS-level lock released)
- ✅ Correct behavior - fasteners handles OS locks properly

## Real-World Scenario

**Before fix:**
```bash
bash test
# Blocks for up to 30 minutes on stale lock
# User has to manually kill processes or delete lock files
```

**After fix:**
```bash
bash test
# Detects stale lock immediately
# Removes it automatically
# Build proceeds in <0.1s

# Output:
# Detected stale lock at .build/locks/libfastled_build.lock (process dead)
# Removed stale lock file: .build/locks/libfastled_build.lock
```

## Edge Cases Handled

1. **No metadata, recent lock:** Assume active (2-minute grace period)
2. **No metadata, old lock:** Treat as stale (likely orphaned)
3. **Has metadata, dead PID:** Stale (process crashed)
4. **Has metadata, alive PID:** Active (legitimate lock)
5. **Can't check PID:** Assume active (fail safe)
6. **OS-level lock released:** Acquirable immediately (fasteners handles this)

## Performance Impact

- **Stale lock with metadata:** ~0.01s to check PID + remove
- **Stale lock without metadata:** ~0.01s to check age + remove
- **Active lock (no contention):** No overhead (immediate acquisition)
- **Active lock (contention):** +1s per stale check iteration

## Future Improvements

Possible enhancements (not currently implemented):
- Reduce 2-minute threshold further (30 seconds?) for faster recovery
- Add lock file corruption detection and recovery
- Track lock acquisition duration for performance monitoring
- Implement lock analytics (how often stale locks occur)

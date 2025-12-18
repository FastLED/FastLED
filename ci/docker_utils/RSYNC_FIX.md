# Docker Rsync Timestamp Fix

## Problem

Docker compilations (`bash compile --docker esp32s3`) were always rebuilding files, even when nothing changed. This caused:
- Slow build times (full rebuilds every time)
- No benefit from PlatformIO's incremental compilation
- Wasted time and resources

## Root Cause

The issue was in the rsync commands that sync files from host → Docker container (`ci/compiler/docker_manager.py:489-538`).

### Original rsync flags:
```bash
rsync -a --checksum --delete /host/src/ /fastled/src/
```

**The problem:**
1. `-a` (archive mode) includes `-t` flag → **preserves timestamps from source**
2. `--checksum` detects content changes correctly ✓
3. **BUT**: When rsync runs, it compares checksums and if different, copies the file
4. On Windows/Docker mounts, **timestamps can be unstable** between runs
5. Even if content is unchanged, rsync would **update destination timestamps** to match source
6. PlatformIO sees changed timestamps → **full rebuild triggered**

### File Sync Flow:
```
Host (Windows)
    ↓ rsync with -a flag
/fastled/ in container (timestamps updated from host)
    ↓ dirsync (via source_manager.py)
PlatformIO build dirs (.pio/build/)
    ↓ PlatformIO checks timestamps
REBUILD TRIGGERED (unnecessary!)
```

## Solution

Added `--no-t` flag to all rsync commands:

```bash
rsync -a --no-t --checksum --delete /host/src/ /fastled/src/
```

**How --no-t fixes it:**
- Files are **only copied when content actually changes** (via `--checksum`)
- When content is unchanged, **destination timestamps remain stable**
- PlatformIO sees stable timestamps → **proper incremental builds** ✓

### Updated rsync flags explained:
- `--checksum`: Compare MD5 checksums instead of timestamps (detects real changes)
- `--no-t`: **Do NOT update destination timestamps when content is unchanged**
- `--delete`: Remove destination files that don't exist in source
- `-a` (minus `-t`): Still preserves permissions, ownership, etc.

## Line Ending Protection

An additional concern was line ending differences (CRLF vs LF) causing spurious syncs:

### Multi-layer protection:
1. **`.gitattributes`**: Enforces `eol=lf` for all text files
2. **`.editorconfig`**: Provides editor-level LF enforcement (newly added)
3. **`core.autocrlf=input`**: Git keeps LF in working directory
4. **Verification**: All source files confirmed to have LF endings

### Why line endings matter:
- Host (Windows) could theoretically have CRLF if git is misconfigured
- Docker container expects LF (Linux standard)
- rsync `--checksum` compares **byte-for-byte**, so CRLF ≠ LF
- If line endings differ → checksums differ → file copied → timestamp updated → rebuild

**Current status:** ✓ All files have LF, no CRLF detected

## Testing the Fix

### 1. Test incremental builds work:
```bash
# First build (will compile everything)
bash compile --docker esp32s3 --examples Blink

# Second build (should skip unchanged files)
bash compile --docker esp32s3 --examples Blink
```

**Expected:** Second build should be much faster, showing "up to date" messages

### 2. Verify sync behavior inside container:
```bash
# Start a container manually
docker run -it --rm \
  -v "$(pwd)://host:ro" \
  niteris/fastled-compiler-esp-32s3:latest \
  bash

# Inside container, run diagnostic
bash /host/ci/docker_utils/diagnose_sync.sh
```

**Expected:** Should show "No changes detected" for all directories

### 3. Check for line ending issues:
```bash
# On host (Windows)
python -c "
import pathlib
files = ['src/FastLED.h', 'examples/Blink/Blink.ino']
for f in files:
    content = pathlib.Path(f).read_bytes()
    has_crlf = b'\r\n' in content
    print(f'{f}: CRLF={has_crlf}')
"
```

**Expected:** All files should show `CRLF=False`

## Files Modified

1. **`ci/compiler/docker_manager.py`**:
   - Added `--no-t` to 5 rsync commands (lines 490, 504, 516, 527, 538)
   - Updated docstrings to explain rsync flags and line ending protection

2. **`.editorconfig`** (new file):
   - Enforces `end_of_line = lf` across all text files
   - Provides editor-level consistency

3. **`ci/docker_utils/diagnose_sync.sh`** (new file):
   - Diagnostic tool to check sync behavior inside containers
   - Detects line ending mismatches and content differences

4. **`ci/docker_utils/RSYNC_FIX.md`** (this file):
   - Documentation of the problem, solution, and testing

## Verification

Run this command to verify the fix is in place:
```bash
grep -n "rsync.*--no-t.*--checksum" ci/compiler/docker_manager.py
```

Should return 5 matches showing all rsync commands have `--no-t` flag.

## Additional Notes

- The `dirsync` calls in `source_manager.py` are **not affected** by this issue
  - They run INSIDE the container (not host→container)
  - They copy from /fastled → PlatformIO build dirs
  - If /fastled is stable (thanks to --no-t fix), these syncs are efficient

- Docker mounts on Windows can have timestamp quirks due to:
  - NTFS → ext4 filesystem translation
  - Docker Desktop's file sharing mechanism
  - Subsecond timestamp precision differences

## References

- rsync man page: https://linux.die.net/man/1/rsync
- Git attributes: https://git-scm.com/docs/gitattributes
- EditorConfig: https://editorconfig.org

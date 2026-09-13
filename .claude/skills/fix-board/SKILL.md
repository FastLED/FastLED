---
name: fix-board
description: Automatically diagnose and fix board upload/monitor issues. Runs three-phase device workflow (Compile, Upload, Monitor) and applies fixes.
disable-model-invocation: true
context: fork
agent: fix-board-agent
---

Automatically run the three-phase device workflow (Compile, Upload, Monitor), diagnose any failures, and apply fixes.

1. **Compile**: Run compilation using `uv run ci/debug_attached.py` (compile phase only)
2. **Upload**: Execute upload with automatic port conflict resolution
3. **Monitor**: Attach to serial monitor, capture output for 10 seconds, detect failure keywords
4. **Diagnose**: Identify upload failures, compilation errors, runtime crashes, ESP-IDF errors
5. **Fix**: Automatically fix code, configuration, or suggest manual hardware fixes
6. **Verify**: Re-test after applying fixes (up to 3 attempts)

## Using debug_attached.py

```bash
uv run ci/debug_attached.py esp32dev --timeout 10 --fail-on PANIC --fail-on "guru meditation"
```

## What Gets Fixed Automatically

- Upload failures: Port detection, permission issues, bootloader problems
- Compilation errors: Missing includes, type errors, dependency issues
- Configuration issues: Wrong board settings, incorrect platformio.ini parameters
- Runtime crashes: Watchdog resets, brownout detection, stack overflows

## What Requires Manual Intervention

- Hardware issues: Physical wiring problems, power supply issues
- Driver issues: Missing USB drivers, permission setup
- Severe code issues: Major architecture problems requiring human decisions

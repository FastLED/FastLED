---
description: Automatically diagnose and fix PlatformIO board upload/monitor issues
---

Automatically run the three-phase device workflow (Compile → Upload → Monitor), diagnose any failures, and apply fixes.

Use the 'fix-board-agent' sub-agent to:

1. **Phase 1 - Compile**: Run compilation using `uv run ci/debug_attached.py` (compile phase only)
2. **Phase 2 - Upload**: Execute upload with automatic port conflict resolution (kills lingering monitors)
3. **Phase 3 - Monitor**: Attach to serial monitor, capture output for 10 seconds (default), detect failure keywords
4. **Capture Logs**: Save all output to timestamped log file for analysis
5. **Diagnose Issues**: Identify upload failures, compilation errors, runtime crashes, ESP-IDF errors, or configuration problems
6. **Strategize Fixes**: Determine the root cause and plan corrective actions
7. **Apply Fixes**: Automatically fix code, configuration, or suggest manual hardware fixes
8. **Verify Results**: Re-test after applying fixes (up to 3 attempts)
9. **Report**: Provide detailed summary with status and any required manual steps

## Using debug_attached.py

The agent should use `uv run ci/debug_attached.py` which provides:
- Three-phase workflow: Compile → Upload (with self-healing) → Monitor
- Automatic port conflict resolution (kills lingering monitor processes)
- Keyword-based failure detection (`--fail-on PANIC`, `--fail-on ERROR`, etc.)
- Exit code 0 for success/normal timeout, exit code 1 for failures
- Real-time output display with summary (first/last 100 lines)

Example command:
```bash
uv run ci/debug_attached.py esp32dev --timeout 10 --fail-on PANIC --fail-on "guru meditation"
```

## What Gets Fixed Automatically

- **Upload failures**: Port detection, permission issues, bootloader problems
- **Compilation errors**: Missing includes, type errors, dependency issues
- **Configuration issues**: Wrong board settings, incorrect platformio.ini parameters
- **Runtime crashes**: Watchdog resets, brownout detection, stack overflows

## What Requires Manual Intervention

- **Hardware issues**: Physical wiring problems, power supply issues
- **Driver issues**: Missing USB drivers (Windows), permission setup (Linux)
- **Severe code issues**: Major architecture problems requiring human decisions

## Output

The agent will provide a structured report with:
- Status (✅ SUCCESS / ⚠️ PARTIAL / ❌ FAILED)
- Issues found and fixes applied
- Manual steps required (if any)
- Log file locations (preserved only if issues persist)

On complete success, all temporary logs are automatically cleaned up.

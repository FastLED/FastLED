---
description: Automatically diagnose and fix PlatformIO board upload/monitor issues
---

Automatically run PlatformIO upload and monitor commands, diagnose any failures, and apply fixes.

Use the 'fix-board-agent' sub-agent to:

1. **Execute Upload/Monitor**: Run `pio run -t upload -t monitor` for 30 seconds
2. **Capture Logs**: Save all output for analysis
3. **Diagnose Issues**: Identify upload failures, compilation errors, runtime crashes, or configuration problems
4. **Strategize Fixes**: Determine the root cause and plan corrective actions
5. **Apply Fixes**: Automatically fix code, configuration, or suggest manual hardware fixes
6. **Verify Results**: Re-test after applying fixes (up to 3 attempts)
7. **Report**: Provide detailed summary with status and any required manual steps

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

# ESP-IDF 5.5 Incompatibility with Git Bash/MSYS on Windows

## Problem

PlatformIO builds for ESP32-C6 (and other ESP32 variants using ESP-IDF 5.5) fail when executed from Git Bash (MSYS/MinGW) environment on Windows.

## Symptoms

```
idf_tools.py installation failed (rc=1). Tail:
ERROR: MSys/Mingw is not supported. Please follow the getting started guide of the documentation to set up a supported environment

error: C:\Users\niteris\.platformio\packages\tool-esptoolpy does not appear to be a Python project, as neither `pyproject.toml` nor `setup.py` are present in the directory

'riscv32-esp-elf-g++' is not recognized as an internal or external command,
operable program or batch file.
```

Build fails with exit code 1 during the preprocessing stage when converting `.ino` files.

## Root Cause

ESP-IDF 5.5 toolchain installation scripts (`idf_tools.py`) explicitly reject MSYS/MinGW environments. The toolchain requires:
- Windows CMD (Command Prompt)
- PowerShell
- WSL (Windows Subsystem for Linux)

Git Bash uses MSYS2 which mimics a Unix-like environment on Windows, but ESP-IDF's installation and build scripts detect this as an unsupported platform.

## Affected Platforms

- ESP32-C6 (RISC-V architecture) - uses `toolchain-riscv32-esp`
- ESP32-C3 (RISC-V architecture) - uses `toolchain-riscv32-esp`
- ESP32-C2 (RISC-V architecture) - uses `toolchain-riscv32-esp`
- ESP32-S3 (Xtensa architecture) - uses `toolchain-xtensa-esp32s3`
- ESP32 (Xtensa architecture) - uses `toolchain-xtensa-esp32`

**Note**: While Xtensa-based chips may have different error patterns, the underlying issue (MSYS rejection) affects all ESP-IDF 5.5 based builds.

## Workarounds

### Option 1: Use Windows CMD or PowerShell (Recommended for local dev)

From Windows Command Prompt or PowerShell:
```cmd
pio run
```

Or from Git Bash, invoke via CMD:
```bash
cmd //c "pio run"
```

**Issue**: Even invoking via `cmd //c` from Git Bash still triggers the MSYS detection error because environment variables and shell context are inherited. ESP-IDF's `idf_tools.py` detects MSYS through environment markers.

**CONFIRMED**: You MUST open a separate native Windows CMD or PowerShell window outside of Git Bash. Running `cmd //c` from within Git Bash does NOT work.

### Option 2: Use Docker (Recommended for CI/CD)

The project has Docker support for ESP32 builds:
```bash
uv run ci/ci-compile.py esp32c6 --examples <example_name> --docker
```

**Limitation**: The `dev/dev.ino` file is not in the `examples/` directory, so it can't be built via `ci-compile.py` without modifications.

### Option 3: Use WSL (Windows Subsystem for Linux)

Install WSL and run PlatformIO from the Linux environment:
```bash
wsl
cd /mnt/c/Users/niteris/dev/fastled5
pio run
```

### Option 4: Switch to ESP32-S3 for Development

If PARLIO driver testing isn't specifically required on ESP32-C6, temporarily switch the `dev` environment to `esp32s3`:

In `platformio.ini`:
```ini
[env:dev]
extends = env:esp32s3  # Changed from env:esp32c6
```

ESP32-S3 uses Xtensa toolchain which may have different (but still present) MSYS compatibility issues.

## Recommended Solution

For this project's development workflow:
1. **Local development on Windows**: Open a separate PowerShell/CMD terminal for PlatformIO builds
   ```cmd
   REM In a native Windows CMD or PowerShell window (NOT Git Bash):
   cd C:\Users\niteris\dev\fastled5
   pio run
   ```
   **Steps to test**:
   1. Press `Win+R`, type `cmd`, press Enter
   2. Navigate to project: `cd C:\Users\niteris\dev\fastled5`
   3. Run build: `pio run`

   If packages are corrupted from MSYS attempts, clean them first:
   ```cmd
   rmdir /s /q "%USERPROFILE%\.platformio\packages\toolchain-riscv32-esp"
   rmdir /s /q "%USERPROFILE%\.platformio\packages\tool-esptoolpy"
   rmdir /s /q "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32"
   rmdir /s /q "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32-libs"
   ```

2. **CI/CD**: Already uses Docker for ESP32 builds (no changes needed)
3. **Quick testing**: Use the host-based example compilation system:
   ```bash
   uv run test.py --examples <example_name>
   ```
   (Note: Only works for examples in `examples/` directory, not `dev/dev.ino`)

## Related Information

- ESP-IDF Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html
- PlatformIO ESP32 Platform: https://github.com/pioarduino/platform-espressif32
- PlatformIO Toolchain: `toolchain-riscv32-esp@14.2.0+20250730`
- Framework: `arduino-esp32@3.3.4` with `esp-idf@5.5.0`

## Status

**Unresolved** - This is a limitation of ESP-IDF 5.5 toolchain, not a FastLED bug.

Users on Windows must use CMD/PowerShell/WSL for ESP32 PlatformIO builds when working from Git Bash.

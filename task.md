# Windows QEMU Permissions Error Investigation Report

## Problem Summary
The QEMU test command `bash test --qemu esp32dev` fails on Windows due to permissions error requiring administrator privileges.

## Error Analysis

### Root Cause
The error occurs during the QEMU installation phase when attempting to run the QEMU installer executable. The specific error is:

```
[WinError 740] The requested operation requires elevation
```

### Error Location
The permissions error originates in the ESP32 QEMU installation process within `ci/install-qemu.py`. The installer attempts three different installation methods, all failing with the same elevation error:

1. **Installation attempt 1**: `qemu-installer.exe /VERYSILENT /SUPPRESSMSGBOXES /DIR=<target_dir>`
2. **Installation attempt 2**: `qemu-installer.exe /VERYSILENT /SUPPRESSMSGBOXES`
3. **Installation attempt 3**: `qemu-installer.exe /S`

All three attempts fail with `[WinError 740] The requested operation requires elevation`.

### Technical Details

**Failing Command Sequence:**
1. Command: `bash test --qemu esp32dev`
2. Calls QEMU installation process
3. Downloads QEMU installer: `qemu-w64-setup-20241220.exe`
4. Attempts silent installation without administrator privileges
5. **FAILS**: Windows requires elevation for installer execution

**File Locations:**
- Installer downloaded to: `.cache\qemu\qemu-installer.exe`
- Target installation directory: `C:\Users\niteris\dev\fastled\.cache\qemu`
- Source URL: `https://qemu.weilnetz.de/w64/2024/qemu-w64-setup-20241220.exe`

## Impact
- QEMU tests cannot run automatically on Windows
- ESP32 emulation testing is blocked
- CI/development workflow interrupted

## Recommended Solutions

### Option 1: Use Espressif Official Signed Binaries (RECOMMENDED)
**Modify the installation script to download and extract precompiled binaries directly from Espressif's official releases instead of using an installer:**

**Direct Download URLs for Windows x64:**
- **QEMU-Xtensa** (ESP32/ESP32-S2/ESP32-S3):
  - URL: `https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-xtensa-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz`
  - SHA256: `ef550b912726997f3c1ff4a4fb13c1569e2b692efdc5c9f9c3c926a8f7c540fa`
  - Size: 33.06 MB

- **QEMU-RISCV32** (ESP32-C3/ESP32-H2/ESP32-C2):
  - URL: `https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-riscv32-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz`
  - SHA256: `9474015f24d27acb7516955ec932e5307226bd9d6652cdc870793ed36010ab73`
  - Size: 35.47 MB

**Implementation Approach:**
1. Download the `.tar.xz` archives directly from GitHub releases
2. Extract using Python's `tarfile` module (no admin privileges required)
3. Copy binaries to `.cache/qemu` directory
4. No installer execution needed - completely portable

**Benefits:**
- No administrator privileges required
- Official Espressif signed binaries
- Same binaries used by ESP-IDF
- Automatic SHA256 verification
- Portable installation

### Option 2: Manual Administrator Installation
Run the QEMU installer manually with administrator privileges:
1. Navigate to `.cache\qemu\qemu-installer.exe`
2. Right-click → "Run as administrator"
3. Complete installation manually

### Option 3: Pre-install QEMU System-wide
Install QEMU globally with administrator privileges and ensure `qemu-system-xtensa.exe` is in system PATH.

### Option 4: Skip QEMU Installation
Use environment variable `FASTLED_QEMU_SKIP_INSTALL=true` to skip automatic installation if QEMU is already installed system-wide.

## Status
**Issue Confirmed**: Windows permissions error prevents automatic QEMU installation during test execution.
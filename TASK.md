# Task: Enable Docker-Based QEMU Testing for ESP32-C3 with BlinkParallel.ino

## Problem Statement
Docker-based QEMU for local builds currently does not work. However, we have improved the situation with the tobozo2 merged-bin GitHub Action that now works successfully.

**Goal**: Get Docker-based QEMU builds working locally so that we can run esp32c3 against the BlinkParallel.ino example.

## Background

### What's Currently Working
1. **GitHub Actions QEMU Testing** (`.github/workflows/qemu_tobozo2.yml`)
   - Uses `tobozo/esp32-qemu-sim@v2.1.0` GitHub Action
   - Successfully tests ESP32, ESP32-S3, and ESP32-C3
   - Uses merged binary approach: `--merged-bin -o qemu-build/merged.bin`
   - Passes single `flash-image: qemu-build/merged.bin` to QEMU
   - Works for Blink and BlinkParallel examples

2. **Merged Binary Generation** (`ci/ci-compile.py`)
   - `--merged-bin` flag creates single flash image
   - Combines bootloader, partitions, and firmware
   - Supported on ESP32 platforms only
   - Example command:
     ```bash
     uv run ci/ci-compile.py esp32c3 --examples BlinkParallel --merged-bin -o qemu-build/merged.bin
     ```

### What's Not Working
1. **Docker-based QEMU** (`ci/dockerfiles/qemu_esp32_docker.py`)
   - Docker isolation issues on Windows/Git Bash
   - Path conversion problems
   - Container startup/cleanup issues

## Action Plan

### Phase 1: Analyze Current Implementation
**Objective**: Understand why GitHub Actions work but local builds don't

- [ ] **Step 1.1**: Test current merged-bin generation locally
  ```bash
  uv run ci/ci-compile.py esp32c3 --examples BlinkParallel --merged-bin -o qemu-build/merged.bin --defines FASTLED_ESP32_IS_QEMU --verbose
  ```
  - Verify merged.bin is created
  - Check file size and magic bytes (should be 0xE9)
  - Compare with GitHub Actions output

- [ ] **Step 1.2**: Identify Docker QEMU issues
  - Test if Docker is available: `docker --version`
  - Try running Docker QEMU manually:
    ```bash
    uv run ci/dockerfiles/qemu_esp32_docker.py qemu-build/merged.bin --timeout 60 --machine esp32c3
    ```
  - Document specific error messages
  - Check if issue is:
    - Docker availability/permissions
    - Path conversion (Windows/Git Bash)
    - QEMU binary location in container
    - Volume mounting
    - ESP32-C3 specific machine type


### Phase 2: Fix Docker QEMU Runner
**Objective**: Get Docker-based QEMU working for ESP32-C3

- [ ] **Step 2.1**: Update `DockerQEMURunner` class in `ci/dockerfiles/qemu_esp32_docker.py`
  - Verify ESP32-C3 machine type support (line 264-267)
  - Test QEMU binary path in container: `/opt/esp/tools/qemu-xtensa/esp_develop_9.2.2_20250817/qemu/bin/qemu-system-riscv32`
  - Fix any path conversion issues for Windows/Git Bash

- [ ] **Step 2.2**: Test with merged binary
  ```bash
  # First generate merged binary
  uv run ci/ci-compile.py esp32c3 --examples BlinkParallel --merged-bin -o qemu-build/merged.bin --defines FASTLED_ESP32_IS_QEMU

  # Then run in Docker QEMU
  uv run ci/dockerfiles/qemu_esp32_docker.py qemu-build/merged.bin --machine esp32c3 --timeout 120 --interrupt-regex "Test finished.*completed.*iterations|Starting loop iteration 2"
  ```

- [ ] **Step 2.3**: Verify output matches GitHub Actions
  - Check for "BlinkParallel setup starting" message
  - Verify loop iterations
  - Ensure "Test finished" message appears
  - No crash patterns ("guru meditation", "abort()")

### Phase 3: Integration with test.py
**Objective**: Make QEMU testing seamless via `test.py`

- [ ] **Step 3.1**: Verify current `test.py` QEMU integration
  - Check if `--qemu esp32c3` flag exists
  - Test current behavior:
    ```bash
    uv run test.py --qemu esp32c3
    ```

- [ ] **Step 3.2**: Add BlinkParallel example test
  - Create test configuration for BlinkParallel on ESP32-C3
  - Add to test suite with QEMU validation
  - Ensure it uses merged-bin approach

- [ ] **Step 3.3**: Document usage in CLAUDE.md
  - Add ESP32-C3 QEMU testing examples
  - Document merged-bin requirement
  - Add troubleshooting tips

### Phase 4: Verification & Documentation
**Objective**: Ensure reliable, repeatable local QEMU testing

- [ ] **Step 4.1**: Test full workflow
  ```bash
  # One-command test from scratch
  uv run test.py --qemu esp32c3 BlinkParallel
  ```

- [ ] **Step 4.2**: Test on different platforms
  - Windows (Git Bash)
  - Windows (PowerShell)
  - Linux (if available)
  - WSL2 (if available)

- [ ] **Step 4.3**: Create troubleshooting guide
  - Document common errors
  - Add Docker installation/permission fixes
  - Include example outputs (success and failure)

- [ ] **Step 4.4**: Update documentation
  - Update `ci/AGENTS.md` with QEMU testing instructions
  - Update `CLAUDE.md` Quick Reference with QEMU commands
  - Add README in `ci/dockerfiles/` if needed

## Success Criteria

✅ **Primary Goal**: Run BlinkParallel.ino on ESP32-C3 in Docker-based QEMU locally
  - Command: `uv run test.py --qemu esp32c3 BlinkParallel` (or equivalent)
  - Expected output:
    - "BlinkParallel setup starting"
    - "Starting loop iteration 1"
    - "Starting loop iteration 2"
    - "Test finished - completed 2 iterations"
  - No crashes or errors
  - Completes in < 2 minutes

✅ **Secondary Goals**:
  - Works on Windows Git Bash (primary development environment)
  - Uses merged-bin approach (consistent with GitHub Actions)
  - Uses Docker-based QEMU (consistent with GitHub Actions)
  - Properly documented for other developers
  - Can test other examples (Blink, custom sketches)

## Key Files to Modify

1. **`ci/dockerfiles/qemu_esp32_docker.py`** - Docker QEMU runner
   - Lines 264-321: ESP32-C3 machine type configuration
   - Lines 331-453: Container execution logic

2. **`test.py`** - Test framework integration
   - QEMU test invocation logic
   - Example compilation with merged-bin

3. **Documentation**:
   - `CLAUDE.md` - User-facing quick reference
   - `ci/AGENTS.md` - Build system instructions
   - `ci/dockerfiles/README.md` - Docker QEMU documentation

## Notes

- ESP32-C3 uses **RISC-V** architecture (`qemu-system-riscv32`)
- ESP32/ESP32-S3 use **Xtensa** architecture (`qemu-system-xtensa`)
- Merged binary is **required** for proper QEMU boot (includes bootloader + partitions)
- BlinkParallel uses parallel SPI output - good test for ESP32-C3 parallel features
- GitHub Actions workflow is the "ground truth" - local should match its behavior
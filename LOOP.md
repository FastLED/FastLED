# ESP32 Platform Refactoring: Agent Implementation Loop

## Overview

This document defines the **agent loop** for refactoring the ESP32 platform directory structure from its current flat organization to a more hierarchical, maintainable structure.

**Goal**: Organize 78 ESP32 platform files into logical subdirectories while maintaining backward compatibility and zero functionality regression.

**Approved Changes** (from STRUCTURE_VALIDATION.md):
1. ✅ Create `core/` directory for system files
2. ✅ Create `drivers/` hierarchy with subdirectories
3. ✅ Rename `lcd/` → `lcd_cam/`
4. ✅ Consolidate SPI drivers under `drivers/spi/`
5. ✅ Move RMT under `drivers/rmt/` (keeping rmt_4/rmt_5 split)

**Estimated Effort**: ~13 hours of agent work + ~4 hours validation

---

## Prerequisites

### Before Starting

1. **Verify clean git state**:
   ```bash
   git status  # Should show no uncommitted changes
   ```


2. **Document current state**:
   ```bash
   find src/platforms/esp/32 -type f -name "*.h" -o -name "*.hpp" -o -name "*.cpp" | wc -l
   # Should show ~78 files
   ```

3. **Run baseline tests**:
   ```bash
   uv run test.py --cpp  # All tests must pass
   bash compile --docker esp32s3 examples/Blink  # Verify compilation works
   ```

4. **Create backup tag**:
   ```bash
   git tag backup/before-esp32-refactor
   ```

---

## Phase 1: Create Directory Structure (30 minutes)

### Step 1.1: Create New Directories

**Agent Task**: Create empty directory structure

```bash
mkdir -p src/platforms/esp/32/core
mkdir -p src/platforms/esp/32/drivers/rmt
mkdir -p src/platforms/esp/32/drivers/i2s
mkdir -p src/platforms/esp/32/drivers/lcd_cam
mkdir -p src/platforms/esp/32/drivers/parlio
mkdir -p src/platforms/esp/32/drivers/spi
mkdir -p src/platforms/esp/32/drivers/spi_ws2812
```

**Validation**:
```bash
tree -L 3 src/platforms/esp/32/drivers
# Should show new directory structure
```

---

## Phase 2: Move Core System Files (2 hours)

### Step 2.1: Identify Core Files

**Agent Task**: Read STRUCTURE_VALIDATION.md and identify files to move to `core/`

**Target Files** (10 files):
- `fastled_esp32.h` - Master aggregator
- `led_sysdefs_esp32.h` - System defines
- `fastpin_esp32.h` - Pin helpers
- `fastspi_esp32.h` - SPI abstraction
- `clock_cycles.h` - Timing utilities
- `esp_log_control.h` - Logging control
- `delay.h` - Xtensa delay
- `delay_riscv.h` - RISC-V delay
- `delaycycles.h` - Xtensa cycle-accurate delay
- `delaycycles_riscv.h` - RISC-V cycle-accurate delay

### Step 2.2: Move Files with Git

**Agent Task**: Use `git mv` to preserve history

```bash
git mv src/platforms/esp/32/fastled_esp32.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/led_sysdefs_esp32.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/fastpin_esp32.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/fastspi_esp32.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/clock_cycles.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/esp_log_control.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/delay.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/delay_riscv.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/delaycycles.h src/platforms/esp/32/core/
git mv src/platforms/esp/32/delaycycles_riscv.h src/platforms/esp/32/core/
```

### Step 2.3: Update Include Paths

**Agent Task**: Find all files that include core files and update paths

**Search Pattern**:
```bash
# Find all includes of moved files
grep -r '#include.*fastled_esp32.h' src/ --include="*.h" --include="*.cpp" --include="*.hpp"
grep -r '#include.*led_sysdefs_esp32.h' src/ --include="*.h" --include="*.cpp" --include="*.hpp"
# ... repeat for all 10 files
```

**Update Pattern**:
```cpp
// OLD:
#include "fastled_esp32.h"
#include "../fastled_esp32.h"

// NEW:
#include "core/fastled_esp32.h"
#include "../core/fastled_esp32.h"
```

**Agent Loop**:
1. Search for each moved file's includes
2. For each occurrence:
   - Read the file
   - Determine current relative path
   - Calculate new relative path to `core/`
   - Update include statement using Edit tool
3. Verify no compilation errors after each batch of 10 updates

### Step 2.4: Validate Core Move

**Agent Task**: Verify compilation still works

```bash
# Test ESP32 classic
bash compile --docker esp32 examples/Blink

# Test ESP32-S3
bash compile --docker esp32s3 examples/Blink

# Test ESP32-C3
bash compile --docker esp32c3 examples/Blink
```

**Expected Result**: All compilations succeed with no errors

**If Errors Occur**:
1. Read error messages carefully
2. Identify missing include paths
3. Use Grep to find all includes of the problematic file
4. Update remaining include paths
5. Retry compilation

**Checkpoint**:
```bash
git add src/platforms/esp/32/core/
git commit -m "refactor(esp32): move core system files to core/ directory

Moved 10 core files:
- fastled_esp32.h, led_sysdefs_esp32.h, fastpin_esp32.h
- fastspi_esp32.h, clock_cycles.h, esp_log_control.h
- delay*.h, delaycycles*.h

Updated ~50 include paths across src/

Verified compilation on esp32/esp32s3/esp32c3"
```

---

## Phase 3: Reorganize RMT Drivers (3 hours)

### Step 3.1: Move RMT Directories

**Agent Task**: Move existing RMT directories under `drivers/rmt/`

```bash
# Keep IDF version separation (critical - do NOT merge)
git mv src/platforms/esp/32/rmt_4 src/platforms/esp/32/drivers/rmt/
git mv src/platforms/esp/32/rmt_5 src/platforms/esp/32/drivers/rmt/
```

### Step 3.2: Move RMT Dispatcher

**Agent Task**: Move top-level RMT file

```bash
git mv src/platforms/esp/32/clockless_rmt_esp32.h src/platforms/esp/32/drivers/rmt/
```

### Step 3.3: Update RMT Include Paths

**Agent Task**: Update all includes of RMT files

**Search Patterns**:
```bash
grep -r '#include.*clockless_rmt_esp32.h' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*idf4_clockless_rmt_esp32.h' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*idf5_clockless_rmt_esp32' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*rmt_4/' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*rmt_5/' src/ --include="*.h" --include="*.cpp"
```

**Update Patterns**:
```cpp
// External includes (from outside platforms/esp/32/)
OLD: #include "platforms/esp/32/clockless_rmt_esp32.h"
NEW: #include "platforms/esp/32/drivers/rmt/clockless_rmt_esp32.h"

// Internal RMT dispatcher includes
OLD: #include "rmt_4/idf4_clockless_rmt_esp32.h"
NEW: #include "rmt_4/idf4_clockless_rmt_esp32.h"  // No change - still relative

// Internal rmt_4 cross-references
OLD: #include "idf4_rmt_impl.h"
NEW: #include "idf4_rmt_impl.h"  // No change - same directory

// Internal rmt_5 cross-references
OLD: #include "rmt5_worker.h"
NEW: #include "rmt5_worker.h"  // No change - same directory
```

**Critical**: RMT internal includes should remain unchanged since directories moved together

### Step 3.4: Update fastled_esp32.h Aggregator

**Agent Task**: Update master aggregator in `core/fastled_esp32.h`

**Read File**: `src/platforms/esp/32/core/fastled_esp32.h`

**Find and Replace**:
```cpp
OLD: #include "clockless_rmt_esp32.h"
NEW: #include "../drivers/rmt/clockless_rmt_esp32.h"
```

### Step 3.5: Validate RMT Move

**Agent Task**: Compile with RMT driver

```bash
# RMT is default driver, test on multiple targets
bash compile --docker esp32 examples/Blink
bash compile --docker esp32s3 examples/Blink
bash compile --docker esp32c3 examples/Blink

# Run C++ tests (may include RMT tests)
uv run test.py --cpp
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/drivers/rmt/
git commit -m "refactor(esp32): move RMT drivers to drivers/rmt/

Moved directories:
- rmt_4/ → drivers/rmt/rmt_4/ (IDF 4.x)
- rmt_5/ → drivers/rmt/rmt_5/ (IDF 5.x)
- clockless_rmt_esp32.h → drivers/rmt/

Kept rmt_4 and rmt_5 separation (IDF API incompatibility)

Updated include paths in core/fastled_esp32.h"
```

---

## Phase 4: Reorganize I2S Drivers (1 hour)

### Step 4.1: Move I2S Directory

**Agent Task**: I2S already in subdirectory, just move under `drivers/`

```bash
git mv src/platforms/esp/32/i2s src/platforms/esp/32/drivers/
```

### Step 4.2: Move I2S Clockless Drivers

**Agent Task**: Move top-level I2S clockless headers

```bash
git mv src/platforms/esp/32/clockless_i2s_esp32.h src/platforms/esp/32/drivers/i2s/
git mv src/platforms/esp/32/clockless_i2s_esp32s3.h src/platforms/esp/32/drivers/i2s/
git mv src/platforms/esp/32/clockless_i2s_esp32s3.cpp src/platforms/esp/32/drivers/i2s/
```

### Step 4.3: Update I2S Include Paths

**Agent Task**: Search and update I2S includes

**Search Patterns**:
```bash
grep -r '#include.*i2s/' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*clockless_i2s_esp32' src/ --include="*.h" --include="*.cpp"
```

**Update Patterns**:
```cpp
// From core/fastled_esp32.h
OLD: #include "clockless_i2s_esp32.h"
NEW: #include "../drivers/i2s/clockless_i2s_esp32.h"

OLD: #include "clockless_i2s_esp32s3.h"
NEW: #include "../drivers/i2s/clockless_i2s_esp32s3.h"

// From i2s/ subdirectory files
OLD: #include "../i2s/i2s_esp32dev.h"
NEW: #include "i2s_esp32dev.h"  // Same directory now
```

### Step 4.4: Validate I2S Move

**Agent Task**: Compile with I2S driver enabled

```bash
# I2S driver requires special build flag
# Check if any examples use FASTLED_ESP32_I2S=1

# Fallback: Verify generic compilation still works
bash compile --docker esp32s3 examples/Blink
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/drivers/i2s/
git commit -m "refactor(esp32): move I2S drivers to drivers/i2s/

Moved:
- i2s/ → drivers/i2s/
- clockless_i2s_esp32*.h/cpp → drivers/i2s/

Updated include paths in core/fastled_esp32.h"
```

---

## Phase 5: Reorganize LCD_CAM Drivers (1 hour)

### Step 5.1: Rename and Move LCD Directory

**Agent Task**: Rename `lcd/` → `lcd_cam/` and move under `drivers/`

```bash
# Rename first
git mv src/platforms/esp/32/lcd src/platforms/esp/32/lcd_cam

# Then move under drivers
git mv src/platforms/esp/32/lcd_cam src/platforms/esp/32/drivers/
```

### Step 5.2: Move LCD Clockless Drivers

**Agent Task**: Move top-level LCD clockless files

```bash
git mv src/platforms/esp/32/clockless_lcd_i80_esp32.h src/platforms/esp/32/drivers/lcd_cam/
git mv src/platforms/esp/32/clockless_lcd_i80_esp32.cpp src/platforms/esp/32/drivers/lcd_cam/
git mv src/platforms/esp/32/clockless_lcd_rgb_esp32.h src/platforms/esp/32/drivers/lcd_cam/
git mv src/platforms/esp/32/clockless_lcd_rgb_esp32.cpp src/platforms/esp/32/drivers/lcd_cam/
git mv src/platforms/esp/32/clockless_lcd_esp32s3_impl.hpp src/platforms/esp/32/drivers/lcd_cam/
```

### Step 5.3: Update LCD Include Paths

**Agent Task**: Search and update LCD includes

**Search Patterns**:
```bash
grep -r '#include.*lcd/' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*clockless_lcd' src/ --include="*.h" --include="*.cpp"
```

**Update Patterns**:
```cpp
// From core/fastled_esp32.h
OLD: #include "clockless_lcd_i80_esp32.h"
NEW: #include "../drivers/lcd_cam/clockless_lcd_i80_esp32.h"

// From lcd/ subdirectory files (now lcd_cam/)
OLD: #include "../lcd/lcd_driver_base.h"
NEW: #include "lcd_driver_base.h"

OLD: #include "lcd/lcd_driver_i80_impl.h"
NEW: #include "lcd_cam/lcd_driver_i80_impl.h"
```

### Step 5.4: Validate LCD Move

**Agent Task**: Verify compilation (LCD is S3/P4 specific)

```bash
# Test ESP32-S3 (has I80 LCD support)
bash compile --docker esp32s3 examples/Blink

# ESP32-P4 may not be available yet, skip if needed
# bash compile --docker esp32p4 examples/Blink || echo "P4 not available, skipping"
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/drivers/lcd_cam/
git commit -m "refactor(esp32): rename lcd/ to lcd_cam/ and move to drivers/

Renamed: lcd/ → lcd_cam/ (accuracy: supports both I80 and RGB)
Moved: lcd_cam/ → drivers/lcd_cam/
Moved: clockless_lcd_*.h/cpp → drivers/lcd_cam/

Updated include paths"
```

---

## Phase 6: Reorganize PARLIO Drivers (30 minutes)

### Step 6.1: Move PARLIO Directory

**Agent Task**: Move existing PARLIO directory under `drivers/`

```bash
git mv src/platforms/esp/32/parlio src/platforms/esp/32/drivers/
```

### Step 6.2: Move PARLIO Clockless Driver

**Agent Task**: Move top-level PARLIO clockless files

```bash
git mv src/platforms/esp/32/clockless_parlio_esp32p4.h src/platforms/esp/32/drivers/parlio/
git mv src/platforms/esp/32/clockless_parlio_esp32p4.cpp src/platforms/esp/32/drivers/parlio/
```

### Step 6.3: Update PARLIO Include Paths

**Agent Task**: Search and update PARLIO includes

**Search Patterns**:
```bash
grep -r '#include.*parlio/' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*clockless_parlio' src/ --include="*.h" --include="*.cpp"
```

**Update Patterns**:
```cpp
// From core/fastled_esp32.h
OLD: #include "clockless_parlio_esp32p4.h"
NEW: #include "../drivers/parlio/clockless_parlio_esp32p4.h"

// From parlio/ subdirectory files
OLD: #include "../parlio/parlio_driver.h"
NEW: #include "parlio_driver.h"
```

### Step 6.4: Validate PARLIO Move

**Agent Task**: Verify compilation (PARLIO is P4-only)

```bash
# PARLIO is ESP32-P4 only, may not be testable yet
# Verify generic ESP32-S3 compilation still works
bash compile --docker esp32s3 examples/Blink
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/drivers/parlio/
git commit -m "refactor(esp32): move PARLIO drivers to drivers/parlio/

Moved:
- parlio/ → drivers/parlio/
- clockless_parlio_esp32p4.* → drivers/parlio/

ESP32-P4 specific, updated include paths"
```

---

## Phase 7: Reorganize SPI Drivers (2 hours)

### Step 7.1: Move Hardware SPI Drivers

**Agent Task**: Move spi_hw_*.cpp files to `drivers/spi/`

```bash
git mv src/platforms/esp/32/spi_hw_1_esp32.cpp src/platforms/esp/32/drivers/spi/
git mv src/platforms/esp/32/spi_hw_2_esp32.cpp src/platforms/esp/32/drivers/spi/
git mv src/platforms/esp/32/spi_hw_4_esp32.cpp src/platforms/esp/32/drivers/spi/
git mv src/platforms/esp/32/spi_hw_8_esp32.cpp src/platforms/esp/32/drivers/spi/
git mv src/platforms/esp/32/spi_platform_esp32.cpp src/platforms/esp/32/drivers/spi/
```

### Step 7.2: Move SPI Support Files

**Agent Task**: Move SPI-related headers

```bash
git mv src/platforms/esp/32/spi_output_template.h src/platforms/esp/32/drivers/spi/
git mv src/platforms/esp/32/spi_device_proxy.h src/platforms/esp/32/drivers/spi/
```

### Step 7.3: Move Clockless SPI (WS2812) Drivers

**Agent Task**: Move spi_ws2812 directory under drivers/

```bash
git mv src/platforms/esp/32/spi_ws2812 src/platforms/esp/32/drivers/

# Move clockless SPI header
git mv src/platforms/esp/32/clockless_spi_esp32.h src/platforms/esp/32/drivers/spi_ws2812/
```

### Step 7.4: Update SPI Include Paths

**Agent Task**: Search and update SPI includes

**Search Patterns**:
```bash
grep -r '#include.*spi_hw' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*spi_platform_esp32' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*spi_output_template' src/ --include="*.h" --include="*.cpp"
grep -r '#include.*clockless_spi_esp32' src/ --include="*.h" --include="*.cpp"
```

**Update Patterns**:
```cpp
// From core/fastled_esp32.h or other platform files
OLD: #include "spi_hw_1_esp32.cpp"
NEW: #include "../drivers/spi/spi_hw_1_esp32.cpp"

// From spi/ files cross-referencing
OLD: #include "spi_output_template.h"
NEW: #include "spi_output_template.h"  // Same directory

// For clockless SPI
OLD: #include "clockless_spi_esp32.h"
NEW: #include "../drivers/spi_ws2812/clockless_spi_esp32.h"
```

### Step 7.5: Update Build System (if needed)

**Agent Task**: Check if meson.build or other build files reference SPI files

```bash
# Search for SPI file references in build files
grep -r 'spi_hw' --include="meson.build" --include="CMakeLists.txt"
```

**If Found**: Update paths in build configuration files

### Step 7.6: Validate SPI Move

**Agent Task**: Compile examples that use SPI (e.g., APA102 LEDs)

```bash
# Hardware SPI for clocked LEDs (APA102, SK9822, etc.)
bash compile --docker esp32 examples/Blink  # Generic test

# If APA102 example exists:
# bash compile --docker esp32 examples/APA102 || echo "No APA102 example, skipping"
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/drivers/spi/
git add src/platforms/esp/32/drivers/spi_ws2812/
git commit -m "refactor(esp32): consolidate SPI drivers under drivers/spi/

Moved hardware SPI:
- spi_hw_*.cpp → drivers/spi/ (1/2/4/8-lane variants)
- spi_platform_esp32.cpp → drivers/spi/
- spi_output_template.h, spi_device_proxy.h → drivers/spi/

Moved clockless SPI:
- spi_ws2812/ → drivers/spi_ws2812/
- clockless_spi_esp32.h → drivers/spi_ws2812/

Updated include paths"
```

---

## Phase 8: Move Remaining Driver Aggregators (1 hour)

### Step 8.1: Identify Remaining Clockless Drivers

**Agent Task**: List remaining `clockless_*.h` files at root level

```bash
find src/platforms/esp/32 -maxdepth 1 -name "clockless_*.h" -o -name "clockless_*.cpp"
```

**Expected Remaining** (after previous phases):
- None (all should be moved to driver subdirectories)

**If Any Remain**: Determine which driver category they belong to and move accordingly

### Step 8.2: Update Master Aggregator

**Agent Task**: Review `core/fastled_esp32.h` for completeness

**Read File**: `src/platforms/esp/32/core/fastled_esp32.h`

**Verify All Includes Updated**:
```cpp
// Should now have paths like:
#include "../drivers/rmt/clockless_rmt_esp32.h"
#include "../drivers/i2s/clockless_i2s_esp32.h"
#include "../drivers/lcd_cam/clockless_lcd_i80_esp32.h"
#include "../drivers/parlio/clockless_parlio_esp32p4.h"
#include "../drivers/spi_ws2812/clockless_spi_esp32.h"
```

**If Any Old Paths Remain**: Update them to new driver locations

---

## Phase 9: Update Documentation (2 hours)

### Step 9.1: Update Platform READMEs

**Agent Task**: Update `src/platforms/esp/32/README.md`

**Read File**: `src/platforms/esp/32/README.md`

**Add Section**: Document new directory structure

```markdown
## Directory Structure

- `core/` - Core platform infrastructure
  - `fastled_esp32.h` - Master aggregator
  - `led_sysdefs_esp32.h` - System defines
  - `fastpin_esp32.h` - Pin mappings (all ESP32 variants)
  - `fastspi_esp32.h` - Hardware SPI abstraction
  - Timing utilities (delay*.h, delaycycles*.h, clock_cycles.h)

- `drivers/` - LED output drivers
  - `rmt/` - RMT (Remote Control Module) drivers
    - `rmt_4/` - ESP-IDF 4.x implementation
    - `rmt_5/` - ESP-IDF 5.x implementation (default)
  - `i2s/` - I2S parallel output (up to 24 lanes)
  - `lcd_cam/` - LCD_CAM drivers (I80/RGB, ESP32-S3/P4)
  - `parlio/` - Parallel IO (ESP32-P4 only)
  - `spi/` - Hardware SPI (1/2/4/8-lane, clocked LEDs)
  - `spi_ws2812/` - Clockless SPI (WS2812 over SPI)

- `interrupts/` - Architecture-specific interrupt handlers
  - `xtensa_lx6.hpp` - ESP32 classic
  - `xtensa_lx7.hpp` - ESP32-S2/S3
  - `riscv.hpp` - ESP32-C3/C6/H2/P4

- `audio/` - Audio processing subsystem
- `ota/` - OTA update support

## Driver Selection

Drivers are selected via build flags and auto-detection:

- **RMT** (default): Timing-precise, 1-8 strips per channel
  - Set `FASTLED_RMT5=0` to force IDF4 driver
  - Set `FASTLED_RMT5_V2=0` to use legacy IDF5 driver

- **I2S**: Parallel output, up to 24 strips simultaneously
  - Set `FASTLED_ESP32_I2S=1` to enable

- **LCD_CAM**: High-bandwidth parallel (ESP32-S3/P4 only)
  - I80: Set `FASTLED_ESP32_LCD_I80=1`
  - RGB: Set `FASTLED_ESP32_LCD_RGB=1` (ESP32-P4 only)

- **PARLIO**: Parallel IO (ESP32-P4 only)
  - Set `FASTLED_ESP32_PARLIO=1`

- **Hardware SPI**: For clocked LEDs (APA102, SK9822, etc.)
  - Auto-detected when using `APA102`, `SK9822`, etc. templates
```

### Step 9.2: Update Driver READMEs

**Agent Task**: Update README files in driver subdirectories

**Files to Update**:
- `src/platforms/esp/32/drivers/rmt/rmt_5/README.md` - Already exists, add note about new location
- `src/platforms/esp/32/drivers/lcd_cam/implementation_notes.md` - Update paths

**Read Each File**: Check for outdated path references

**Example Update**:
```markdown
<!-- OLD -->
Include via: `#include "platforms/esp/32/rmt_5/idf5_clockless_rmt_esp32_v2.h"`

<!-- NEW -->
Include via: `#include "platforms/esp/32/drivers/rmt/rmt_5/idf5_clockless_rmt_esp32_v2.h"`
```

### Step 9.3: Create Architecture Diagram

**Agent Task**: Create visual representation of driver organization

**File**: `src/platforms/esp/32/ARCHITECTURE.md`

```markdown
# ESP32 Platform Architecture

## Driver Dispatcher Pattern

```
User Code
    ↓
FastLED.h
    ↓
platforms/esp/32/core/fastled_esp32.h (Master Aggregator)
    ↓
    ├─→ drivers/rmt/clockless_rmt_esp32.h (Default)
    │       ↓
    │       ├─→ rmt_4/ (IDF 4.x)
    │       └─→ rmt_5/ (IDF 5.x, default)
    │
    ├─→ drivers/i2s/clockless_i2s_esp32.h (Parallel)
    │
    ├─→ drivers/lcd_cam/clockless_lcd_i80_esp32.h (S3/P4)
    │
    ├─→ drivers/parlio/clockless_parlio_esp32p4.h (P4 only)
    │
    └─→ drivers/spi_ws2812/clockless_spi_esp32.h (SPI-based)
```

## Target-Specific Code

Target detection uses inline conditional compilation:

```cpp
// core/fastpin_esp32.h
#if CONFIG_IDF_TARGET_ESP32
    // ESP32 classic definitions
#elif CONFIG_IDF_TARGET_ESP32S3
    // S3-specific definitions (USB-JTAG guards, etc.)
#elif CONFIG_IDF_TARGET_ESP32C3
    // C3-specific definitions (RISC-V)
// ... etc for 9+ targets
#endif
```

## IDF Version Branching

```cpp
// drivers/rmt/clockless_rmt_esp32.h
#if ESP_IDF_VERSION < 5
    #include "rmt_4/idf4_clockless_rmt_esp32.h"
#else
    #if FASTLED_RMT5_V2
        #include "rmt_5/idf5_clockless_rmt_esp32_v2.h"  // Default
    #else
        #include "rmt_5/idf5_clockless_rmt_esp32.h"     // Legacy
    #endif
#endif
```

## Key Design Decisions

1. **No Target Directories**: Target-specific code uses `#ifdef` within files (90% of code is target-agnostic)
2. **No Environment Directories**: Arduino vs IDF differences are <10 lines per file
2. **IDF Version Separation**: `rmt_4/` and `rmt_5/` are completely separate (incompatible APIs)
3. **Worker Pool Pattern**: RMT5 V2 uses persistent workers + ephemeral controllers for N > K strip support
```

**Checkpoint**:
```bash
git add src/platforms/esp/32/README.md
git add src/platforms/esp/32/ARCHITECTURE.md
git add src/platforms/esp/32/drivers/*/README.md
git commit -m "docs(esp32): update documentation for new directory structure

Added:
- README.md: Directory structure and driver selection guide
- ARCHITECTURE.md: Visual architecture diagram

Updated:
- Driver README files with new paths
"
```

---

## Phase 10: Final Validation (3 hours)

### Step 10.1: Comprehensive Compilation Tests

**Agent Task**: Test all supported ESP32 targets

```bash
# ESP32 classic
bash compile --docker esp32 examples/Blink
bash compile --docker esp32 examples/ColorPalette

# ESP32-S2
bash compile --docker esp32s2 examples/Blink

# ESP32-S3
bash compile --docker esp32s3 examples/Blink
bash compile --docker esp32s3 examples/ColorPalette

# ESP32-C3
bash compile --docker esp32c3 examples/Blink

# ESP32-C6 (if available)
bash compile --docker esp32c6 examples/Blink || echo "C6 not available, skipping"
```

**Expected Result**: All compilations succeed

### Step 10.2: Run Unit Tests

**Agent Task**: Execute C++ test suite

```bash
uv run test.py --cpp
```

**Expected Result**: All tests pass (same as baseline)

### Step 10.3: QEMU Tests (if available)

**Agent Task**: Run ESP32 QEMU tests

```bash
uv run test.py --qemu esp32s3 || echo "QEMU not available, skipping"
```

### Step 10.4: Verify Include Path Coverage

**Agent Task**: Check for any remaining references to old paths

```bash
# Should return NO results (all paths updated)
grep -r '#include.*platforms/esp/32/rmt_4' src/ --include="*.h" --include="*.cpp" || echo "✓ No old rmt_4 paths"
grep -r '#include.*platforms/esp/32/rmt_5' src/ --include="*.h" --include="*.cpp" || echo "✓ No old rmt_5 paths"
grep -r '#include.*platforms/esp/32/lcd/' src/ --include="*.h" --include="*.cpp" || echo "✓ No old lcd/ paths"
grep -r '"fastled_esp32.h"' src/ --include="*.h" --include="*.cpp" | grep -v 'core/fastled_esp32.h' || echo "✓ All fastled_esp32.h includes use core/"
```

**If Any Found**: Update remaining old paths

### Step 10.5: Check for Broken Symlinks or Missing Files

**Agent Task**: Verify no broken references

```bash
# Find any dangling references
find src/platforms/esp/32 -type f -name "*.h" -o -name "*.cpp" | while read file; do
    # Check if file includes any non-existent headers (basic check)
    echo "Checking $file..."
done
```

### Step 10.6: Validate File Count

**Agent Task**: Ensure no files were lost

```bash
# Should still be ~78 files (just reorganized)
find src/platforms/esp/32 -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) | wc -l
```

**Expected**: Same count as Phase 1 baseline (±2 for new docs)

---

## Phase 11: Cleanup and Finalization (1 hour)

### Step 11.1: Remove Empty Directories

**Agent Task**: Clean up any leftover empty directories

```bash
find src/platforms/esp/32 -type d -empty -delete
```

### Step 11.2: Format Code (if needed)

**Agent Task**: Run linter on modified files

```bash
bash lint --no-fingerprint
```

### Step 11.3: Create Summary Commit

**Agent Task**: Review all changes and create final commit if needed

```bash
git log --oneline HEAD~10..HEAD
# Should show ~7 commits (one per phase)
```

### Step 11.4: Tag Completion

**Agent Task**: Tag the completed refactoring

```bash
git tag refactor/esp32-platform-structure-complete
```

---

## Rollback Procedure

If critical issues are discovered after refactoring:

### Immediate Rollback

```bash
# Return to baseline (WARNING: This will discard all refactoring work)
git reset --hard backup/before-esp32-refactor
```

### Partial Rollback (Cherry-Pick Good Changes)

```bash
# Save current commit hash in case you need it
CURRENT_HEAD=$(git rev-parse HEAD)

# Reset to baseline
git reset --hard backup/before-esp32-refactor

# Cherry-pick specific successful phases from saved commit history
git cherry-pick <phase2-commit>  # Core files
git cherry-pick <phase3-commit>  # RMT drivers
# ... etc

# If you need to reference the old commits, they are still available via:
# git log $CURRENT_HEAD
```

---

## Success Criteria

### ✅ Refactoring Complete When:

1. **Directory Structure Matches Target**:
   - `core/` exists with 10 system files
   - `drivers/rmt/`, `drivers/i2s/`, `drivers/lcd_cam/`, `drivers/parlio/`, `drivers/spi/`, `drivers/spi_ws2812/` exist
   - All files moved with `git mv` (history preserved)

2. **All Compilation Tests Pass**:
   - ESP32, ESP32-S3, ESP32-C3 compile successfully
   - At least 2 examples compile per target

2. **Unit Tests Pass**:
   - `uv run test.py --cpp` shows same results as baseline

3. **Documentation Updated**:
   - README.md documents new structure
   - ARCHITECTURE.md provides visual guide
   - Driver READMEs have correct paths

4. **No Broken References**:
   - Grep searches for old paths return no results
   - No compilation warnings about missing headers

6. **File Count Preserved**:
   - `find` shows ~78 files (same as baseline ±2)

---

## Agent Loop Flowchart

```
START
  ↓
[Phase 1] Create directories
  ↓
[Phase 2] Move core/ files → Update includes → Validate → Commit
  ↓
[Phase 3] Move drivers/rmt/ → Update includes → Validate → Commit
  ↓
[Phase 4] Move drivers/i2s/ → Update includes → Validate → Commit
  ↓
[Phase 5] Move drivers/lcd_cam/ → Update includes → Validate → Commit
  ↓
[Phase 6] Move drivers/parlio/ → Update includes → Validate → Commit
  ↓
[Phase 7] Move drivers/spi* → Update includes → Validate → Commit
  ↓
[Phase 8] Verify all drivers moved
  ↓
[Phase 9] Update documentation → Commit
  ↓
[Phase 10] Full validation suite
  ↓
[Phase 11] Cleanup → Tag completion
  ↓
END

Note: If any validation fails, STOP and debug before proceeding to next phase
```

---

## Estimated Timeline

| Phase | Task | Time | Validation |
|-------|------|------|------------|
| 1 | Create directories | 30m | Tree check |
| 2 | Move core/ | 2h | Compile 3 targets |
| 3 | Move drivers/rmt/ | 3h | Compile + tests |
| 4 | Move drivers/i2s/ | 1h | Compile |
| 5 | Move drivers/lcd_cam/ | 1h | Compile S3 |
| 6 | Move drivers/parlio/ | 30m | Compile |
| 7 | Move drivers/spi* | 2h | Compile + SPI tests |
| 8 | Verify remaining | 1h | Grep checks |
| 9 | Documentation | 2h | Review |
| 10 | Final validation | 3h | Full test suite |
| 11 | Cleanup | 1h | Tag |
| **TOTAL** | **~17 hours** | Agent work + validation |

---

## Notes for Agents

### Key Principles

1. **Use `git mv`**: Always use `git mv` instead of `mv` to preserve file history
2. **Validate After Each Phase**: Never proceed to next phase if compilation fails
2. **Commit Per Phase**: Each phase should have its own commit with descriptive message
3. **Search Thoroughly**: Use comprehensive Grep searches to find ALL include references
4. **Test Multiple Targets**: ESP32 classic, S3, and C3 minimum for each validation

### Common Pitfalls

1. **Relative Path Confusion**: Calculate correct `../` depth based on file location
2. **Internal vs External Includes**: Files within same moved directory may not need path updates
2. **Build System References**: Check meson.build, CMakeLists.txt for hardcoded paths
3. **Documentation Staleness**: Update READMEs immediately after moving files
4. **Premature Optimization**: Do NOT attempt to merge rmt_4 and rmt_5 - they are intentionally separate

### When to Ask for Help

- Compilation errors persist after updating obvious includes
- Grep finds >100 references to update (may indicate wrong search pattern)
- Tests fail that passed in baseline (potential logic error, not just path issue)
- Uncertain whether a file belongs to a particular driver category

---

## Post-Refactoring Tasks (Future)

### Phase 12 (Optional): Advanced Optimizations

1. **Extract Common Driver Code**: If multiple drivers share utilities, create `drivers/common/`
2. **Target Directories (Deferred)**: Only if target-specific code exceeds 100 lines per file
2. **Environment Directories (Deferred)**: Only if Arduino/IDF differences grow significantly
3. **Build System Integration**: Update PlatformIO/Arduino library metadata if needed

---

**Version**: 1.0
**Last Updated**: 2025-11-01
**Status**: Ready for Agent Execution

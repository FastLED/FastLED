# SPI Hardware Manager Implementation Guide

This guide explains how to implement SPI hardware support for a new platform using FastLED's unified hardware manager pattern.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Step-by-Step Implementation](#step-by-step-implementation)
- [Template Code](#template-code)
- [Feature Flags](#feature-flags)
- [Priority System](#priority-system)
- [Testing](#testing)
- [Common Pitfalls](#common-pitfalls)

## Overview

FastLED uses a **unified hardware manager pattern** for initializing SPI controllers across all platforms. This pattern:

- Centralizes hardware initialization in a single entry point
- Uses feature flags for conditional compilation
- Implements priority-based registration for multi-lane controllers
- Follows lazy initialization to avoid static constructor issues
- Provides clean separation between platform detection and implementation

**Key Files Per Platform:**
1. **Manager file**: `spi_hw_manager_<platform>.cpp.hpp` - Contains initialization logic
2. **Dispatch header**: `init_spi_hw.h` - Routes to platform manager
3. **Implementation files**: `spi_hw_N_<platform>.cpp.hpp` - Hardware-specific code

## Architecture

### Dispatch Header Chain

```
platforms/init_spi_hw.h (top-level)
    ↓
platforms/<family>/init_spi_hw.h (family-level: esp, arm, stub)
    ↓
platforms/<family>/<platform>/spi_hw_manager_<platform>.cpp.hpp
```

**Example for ESP32:**
```
platforms/init_spi_hw.h
    → platforms/esp/init_spi_hw.h
        → platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp
```

### Lazy Initialization Flow

```cpp
// User code
SpiHw4::getAll()
    ↓
// First access triggers initialization
platform::initSpiHardware()
    ↓
// Helper functions check feature flags
detail::addSpiHw8IfPossible()
detail::addSpiHw4IfPossible()
detail::addSpiHw2IfPossible()
    ↓
// Register instances with priority
SpiHw4::registerInstance(instance, PRIORITY_HW_4)
```

## Step-by-Step Implementation

### Step 1: Create Feature Flags

Define platform capabilities in a feature flags header (e.g., `platforms/<family>/<platform>/feature_flags.h`):

```cpp
#pragma once

// Example: STM32 feature flags
#if defined(STM32F2) || defined(STM32F4) || defined(STM32F7) || defined(STM32H7)
    #define FASTLED_STM32_HAS_TIM5 1   // Timer 5 available (8-lane support)
    #define FASTLED_STM32_DMA_STREAM_BASED 1
#else
    #define FASTLED_STM32_HAS_TIM5 0
    #define FASTLED_STM32_DMA_STREAM_BASED 0
#endif

#if defined(STM32F1) || defined(STM32L4)
    #define FASTLED_STM32_DMA_CHANNEL_BASED 1
#else
    #define FASTLED_STM32_DMA_CHANNEL_BASED 0
#endif
```

**Best Practices:**
- Use `FASTLED_<PLATFORM>_HAS_<FEATURE>` naming convention
- Set explicit 0/1 values (not just defined/undefined)
- Document hardware requirements for each flag

### Step 2: Create Platform Manager File

Create `platforms/<family>/<platform>/spi_hw_manager_<platform>.cpp.hpp`:

```cpp
#pragma once

/// @file spi_hw_manager_<platform>.cpp.hpp
/// @brief SPI hardware manager for <Platform> - unified initialization
///
/// This file implements the SPI hardware manager pattern for <Platform>.
/// It provides a single entry point (initSpiHardware()) that initializes
/// all available SPI hardware controllers with priority-based registration.
///
/// @see platforms/shared/SPI_MANAGER_PATTERN.md for implementation guide

#include "platforms/<platform>/feature_flags.h"
#include "fl/namespace.h"
#include "fl/ptr.h"

namespace fl {
namespace detail {

/// @brief Priority constants for SPI hardware registration
/// Higher values = higher priority in routing decisions
constexpr int PRIORITY_HW_16 = 9;  // 16-lane (highest)
constexpr int PRIORITY_HW_8 = 8;   // 8-lane
constexpr int PRIORITY_HW_4 = 7;   // 4-lane (quad SPI)
constexpr int PRIORITY_HW_2 = 6;   // 2-lane (dual SPI)
constexpr int PRIORITY_HW_1 = 5;   // 1-lane (lowest)

/// @brief Add SpiHw16 instances if hardware supports it
static void addSpiHw16IfPossible() {
#if PLATFORM_HAS_16_LANE
    #include "platforms/<platform>/spi_hw_16_<platform>.cpp.hpp"

    // Create static instances (Meyer's Singleton pattern)
    static auto controller0 = fl::make_shared<SpiHw16Platform>(0);
    SpiHw16::registerInstance(controller0, PRIORITY_HW_16);

    FL_DBG("<Platform>: Added SpiHw16 controller");
#else
    // No-op if feature not available
#endif
}

/// @brief Add SpiHw8 instances if hardware supports it
static void addSpiHw8IfPossible() {
#if PLATFORM_HAS_8_LANE
    #include "platforms/<platform>/spi_hw_8_<platform>.cpp.hpp"

    static auto controller0 = fl::make_shared<SpiHw8Platform>(0);
    SpiHw8::registerInstance(controller0, PRIORITY_HW_8);

    FL_DBG("<Platform>: Added SpiHw8 controller");
#endif
}

/// @brief Add SpiHw4 instances if hardware supports it
static void addSpiHw4IfPossible() {
#if PLATFORM_HAS_4_LANE
    #include "platforms/<platform>/spi_hw_4_<platform>.cpp.hpp"

    static auto controller0 = fl::make_shared<SpiHw4Platform>(0);
    SpiHw4::registerInstance(controller0, PRIORITY_HW_4);

    FL_DBG("<Platform>: Added SpiHw4 controller");
#endif
}

/// @brief Add SpiHw2 instances if hardware supports it
static void addSpiHw2IfPossible() {
#if PLATFORM_HAS_2_LANE
    #include "platforms/<platform>/spi_hw_2_<platform>.cpp.hpp"

    static auto controller0 = fl::make_shared<SpiHw2Platform>(0);
    SpiHw2::registerInstance(controller0, PRIORITY_HW_2);

    FL_DBG("<Platform>: Added SpiHw2 controller");
#endif
}

/// @brief Add SpiHw1 instances (always available via software fallback)
static void addSpiHw1IfPossible() {
    // Note: SpiHw1 typically uses generic software implementation
    // Only create platform-specific version if hardware-accelerated

#if PLATFORM_HAS_1_LANE_HW
    #include "platforms/<platform>/spi_hw_1_<platform>.cpp.hpp"

    static auto controller0 = fl::make_shared<SpiHw1Platform>(0);
    SpiHw1::registerInstance(controller0, PRIORITY_HW_1);

    FL_DBG("<Platform>: Added SpiHw1 controller");
#endif
}

}  // namespace detail

namespace platform {

/// @brief Initialize all SPI hardware controllers for this platform
///
/// This function is called lazily on first access to SpiHwN::getAll().
/// It registers all available SPI hardware instances in priority order
/// (highest lane count first) to enable optimal routing.
///
/// @note This function is idempotent - safe to call multiple times
void initSpiHardware() {
    FL_DBG("<Platform>: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw16IfPossible();  // Priority 9
    detail::addSpiHw8IfPossible();   // Priority 8
    detail::addSpiHw4IfPossible();   // Priority 7
    detail::addSpiHw2IfPossible();   // Priority 6
    detail::addSpiHw1IfPossible();   // Priority 5

    FL_DBG("<Platform>: SPI hardware initialized");
}

}  // namespace platform
}  // namespace fl
```

### Step 3: Create Dispatch Header

Create `platforms/<family>/init_spi_hw.h`:

```cpp
#pragma once

/// @file init_spi_hw.h
/// @brief Platform dispatch header for SPI hardware initialization
///
/// This header routes to the appropriate platform manager based on
/// compile-time platform detection.

#include "platforms/<family>/is_<family>.h"  // Platform detection

#if defined(PLATFORM_SPECIFIC_MACRO)
namespace fl {
namespace platform {
/// @brief Initialize SPI hardware for this platform
/// @note Implemented in spi_hw_manager_<platform>.cpp.hpp
void initSpiHardware();
}  // namespace platform
}  // namespace fl
#else
// Fallback to shared no-op implementation
#include "platforms/shared/init_spi_hw.h"
#endif
```

### Step 4: Update Top-Level Dispatch Header

Update `platforms/init_spi_hw.h` to include your platform family:

```cpp
#pragma once

#include "fl/namespace.h"

#if defined(FASTLED_TESTING)
    #include "platforms/stub/init_spi_hw.h"
#elif defined(FL_IS_ESP)
    #include "platforms/esp/init_spi_hw.h"
#elif defined(FASTLED_ARM)
    #include "platforms/arm/init_spi_hw.h"
// Add your platform family here:
#elif defined(YOUR_PLATFORM_FAMILY)
    #include "platforms/<family>/init_spi_hw.h"
#else
    // Fallback to shared implementation
    #include "platforms/shared/init_spi_hw.h"
#endif
```

### Step 5: Update Shared Registry Files

The shared registry files (`platforms/shared/spi_hw_N.cpp.hpp`) already call `platform::initSpiHardware()` on first access. No changes needed unless adding a new lane count.

## Template Code

### Minimal Platform Manager Template

```cpp
#pragma once

#include "fl/namespace.h"
#include "fl/ptr.h"

namespace fl {
namespace detail {

constexpr int PRIORITY_HW_4 = 7;
constexpr int PRIORITY_HW_2 = 6;

static void addSpiHw4IfPossible() {
#if YOUR_PLATFORM_HAS_QUAD_SPI
    #include "platforms/your/spi_hw_4_your.cpp.hpp"
    static auto ctrl = fl::make_shared<SpiHw4Your>(0);
    SpiHw4::registerInstance(ctrl, PRIORITY_HW_4);
#endif
}

static void addSpiHw2IfPossible() {
#if YOUR_PLATFORM_HAS_DUAL_SPI
    #include "platforms/your/spi_hw_2_your.cpp.hpp"
    static auto ctrl = fl::make_shared<SpiHw2Your>(0);
    SpiHw2::registerInstance(ctrl, PRIORITY_HW_2);
#endif
}

}  // namespace detail

namespace platform {

void initSpiHardware() {
    detail::addSpiHw4IfPossible();
    detail::addSpiHw2IfPossible();
}

}  // namespace platform
}  // namespace fl
```

## Feature Flags

### Naming Convention

Use this pattern for feature flags:

```
FASTLED_<PLATFORM>_HAS_<FEATURE>
```

**Examples:**
- `FASTLED_ESP32_HAS_I2S` - ESP32 has I2S peripheral for 16-lane SPI
- `FASTLED_STM32_HAS_TIM5` - STM32 has Timer 5 for 8-lane support
- `FASTLED_RP2040_HAS_PIO` - RP2040 has PIO state machines

### Setting Values

Always use explicit 0/1 values (not just defined/undefined):

```cpp
// ✅ Correct
#define FASTLED_STM32_HAS_TIM5 1
#define FASTLED_STM32_HAS_TIM6 0

// ❌ Wrong
#define FASTLED_STM32_HAS_TIM5    // Ambiguous
#undef FASTLED_STM32_HAS_TIM6     // Hard to track
```

### Checking Flags

Use `#if` (not `#ifdef`) for checking feature flags:

```cpp
// ✅ Correct
#if FASTLED_STM32_HAS_TIM5
    // Feature enabled
#endif

// ❌ Wrong
#ifdef FASTLED_STM32_HAS_TIM5
    // Doesn't distinguish between 0 and 1
#endif
```

## Priority System

### Priority Values

Use these standard priority values:

| Lane Count | Priority | Constant | Use Case |
|------------|----------|----------|----------|
| 16 | 9 | `PRIORITY_HW_16` | Highest (I2S parallel, etc.) |
| 8 | 8 | `PRIORITY_HW_8` | 8-lane (Octal SPI) |
| 4 | 7 | `PRIORITY_HW_4` | 4-lane (Quad SPI) |
| 2 | 6 | `PRIORITY_HW_2` | 2-lane (Dual SPI) |
| 1 | 5 | `PRIORITY_HW_1` | Lowest (Single lane) |

### Why Priority Matters

The priority system enables optimal routing when multiple controllers are available:

```cpp
// User requests 4 lanes
fl::Spi spi(CLOCK_PIN, {D0, D1, D2, D3}, fl::SPI_HW);

// SPIBusManager checks available controllers in priority order:
// 1. SpiHw16 (priority 9) - CAN handle 4 lanes
// 2. SpiHw8  (priority 8) - CAN handle 4 lanes
// 3. SpiHw4  (priority 7) - CAN handle 4 lanes ← SELECTED (exact match)
// 4. SpiHw2  (priority 6) - CANNOT handle 4 lanes
// 5. SpiHw1  (priority 5) - CANNOT handle 4 lanes

// Result: SpiHw4 is selected (exact match preferred)
```

**Rules:**
1. Higher lane counts have higher priority
2. Exact lane match is preferred over "can handle"
3. Controllers are tried in priority order

## Testing

### Checklist

After implementing a platform manager, verify:

- [ ] All expected SPI hardware instances are registered
- [ ] Priority ordering is correct (16 > 8 > 4 > 2 > 1)
- [ ] Feature flags correctly enable/disable hardware
- [ ] Lazy initialization works (no static constructor issues)
- [ ] Debug output shows correct initialization sequence
- [ ] Unit tests pass for all lane counts

### Verification Commands

```bash
# Run C++ unit tests
uv run test.py --cpp

# Run specific SPI tests
uv run test.py spi_hw

# Compile for target platform
uv run ci/ci-compile.py <platform> --examples Blink

# Check debug output
# Look for "FL_DBG" messages showing initialization
```

### Example Debug Output

Expected output when SPI hardware is initialized:

```
[FL_DBG] STM32: Initializing SPI hardware
[FL_DBG] STM32: Added TIM5 SpiHw8 controller
[FL_DBG] STM32: Added TIM1 SpiHw4 controller
[FL_DBG] STM32: Added TIM8 SpiHw2 controller
[FL_DBG] STM32: SPI hardware initialized
```

## Common Pitfalls

### 1. Incorrect Feature Flag Usage

**❌ Wrong:**
```cpp
#ifdef FASTLED_STM32_HAS_TIM5  // Doesn't check value
```

**✅ Correct:**
```cpp
#if FASTLED_STM32_HAS_TIM5  // Checks 0/1 value
```

### 2. Including Implementation Too Early

**❌ Wrong:**
```cpp
// Top of file
#include "platforms/stm32/spi_hw_4_stm32.cpp.hpp"

static void addSpiHw4IfPossible() {
#if FASTLED_STM32_HAS_TIM1
    // Implementation already included unconditionally!
#endif
}
```

**✅ Correct:**
```cpp
static void addSpiHw4IfPossible() {
#if FASTLED_STM32_HAS_TIM1
    // Include only when feature is enabled
    #include "platforms/stm32/spi_hw_4_stm32.cpp.hpp"
#endif
}
```

### 3. Wrong Priority Order

**❌ Wrong:**
```cpp
void initSpiHardware() {
    detail::addSpiHw2IfPossible();  // Low priority first
    detail::addSpiHw4IfPossible();
    detail::addSpiHw8IfPossible();  // High priority last
}
```

**✅ Correct:**
```cpp
void initSpiHardware() {
    detail::addSpiHw8IfPossible();  // High priority first
    detail::addSpiHw4IfPossible();
    detail::addSpiHw2IfPossible();  // Low priority last
}
```

### 4. Creating Multiple Instances Unnecessarily

**❌ Wrong:**
```cpp
static void addSpiHw4IfPossible() {
#if FASTLED_HAS_QUAD_SPI
    // Non-static instances recreated on each call!
    auto ctrl0 = fl::make_shared<SpiHw4Platform>(0);
    auto ctrl1 = fl::make_shared<SpiHw4Platform>(1);
#endif
}
```

**✅ Correct:**
```cpp
static void addSpiHw4IfPossible() {
#if FASTLED_HAS_QUAD_SPI
    // Static instances persist (Meyer's Singleton)
    static auto ctrl0 = fl::make_shared<SpiHw4Platform>(0);
    static auto ctrl1 = fl::make_shared<SpiHw4Platform>(1);

    SpiHw4::registerInstance(ctrl0, PRIORITY_HW_4);
    SpiHw4::registerInstance(ctrl1, PRIORITY_HW_4);
#endif
}
```

### 5. Forgetting to Register Instances

**❌ Wrong:**
```cpp
static void addSpiHw2IfPossible() {
#if FASTLED_HAS_DUAL_SPI
    #include "platforms/your/spi_hw_2_your.cpp.hpp"
    static auto ctrl = fl::make_shared<SpiHw2Your>(0);
    // Instance created but never registered!
#endif
}
```

**✅ Correct:**
```cpp
static void addSpiHw2IfPossible() {
#if FASTLED_HAS_DUAL_SPI
    #include "platforms/your/spi_hw_2_your.cpp.hpp"
    static auto ctrl = fl::make_shared<SpiHw2Your>(0);
    SpiHw2::registerInstance(ctrl, PRIORITY_HW_2);  // ← Don't forget!
#endif
}
```

## Reference Implementations

Study these existing implementations for examples:

### Simple Platform (Stub)
- **File:** `platforms/stub/spi_hw_manager_stub.cpp.hpp`
- **Features:** No hardware, no feature flags
- **Good for:** Understanding minimal structure

### Medium Platform (RP2040)
- **File:** `platforms/arm/rp/rpcommon/spi_hw_manager_rp.cpp.hpp`
- **Features:** 2/4/8-lane via PIO state machines
- **Good for:** Multiple lane counts, feature flags

### Complex Platform (ESP32)
- **File:** `platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp`
- **Features:** 1/2/4/8/16-lane, variant detection, I2S
- **Good for:** Multiple hardware backends, complex routing

### Advanced Platform (STM32)
- **File:** `platforms/arm/stm32/spi_hw_manager_stm32.cpp.hpp`
- **Features:** Timer+DMA, variant detection, multiple timers
- **Good for:** Hardware resource management, DMA

## Additional Resources

- **Main Pattern Documentation:** `platforms/readme.md` (SPI Hardware Manager Pattern section)
- **ESP32 Example:** `platforms/esp/32/README.md`
- **STM32 Example:** `platforms/arm/stm32/README.md`
- **Shared Infrastructure:** `platforms/shared/spi_hw_*.cpp.hpp`

## Questions?

If you encounter issues implementing the SPI manager pattern:

1. Check existing platform implementations for reference
2. Verify feature flags are set correctly
3. Look for `FL_DBG` output during initialization
4. Run unit tests to validate behavior
5. Review this guide's "Common Pitfalls" section

The pattern is designed to be consistent across all platforms - if one works, yours should too!

# FastLED Platform: STM32

STM32 family support (e.g., F1, F2, F4 series).

## Supported Cores

FastLED supports multiple STM32 Arduino cores:

1. **STM32duino** (Official STMicroelectronics core): Defines `STM32F1xx`, `STM32F4`, etc.
2. **Roger Clark STM32** (STM32F103C and similar): Defines `__STM32F1__`
3. **Older STM32duino/Arduino_STM32**: Defines `STM32F1`, `STM32F4`
4. **Particle** (STM32F10X_MD, STM32F2XX): For Particle boards

## Files (quick pass)
- `fastled_arm_stm32.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_stm32.h`, `fastpin_arm_stm_legacy.h`, `fastpin_arm_stm_new.h`: Pin helpers/variants.
- `clockless_arm_stm32.h`: Clockless driver for STM32.
- `armpin.h`: Generic ARM pin template utilities.
- `cm3_regs.h`: CM3 register helpers (used by Roger Clark core).
- `led_sysdefs_arm_stm32.h`: System defines for STM32.

Notes:
- Some families prefer different pin backends (legacy/new); choose the appropriate header for your core.
 - Many STM32 Arduino cores map `pinMode` to HAL; direct register variants in `fastpin_*` may differ across series.
- Roger Clark's core requires special handling for ARM detection and placement new operator (handled automatically).

## SPI Hardware Manager

The STM32 platform uses a unified hardware manager pattern for initializing SPI controllers. This provides a clean, maintainable architecture with feature flags and priority-based registration.

### Manager File

`src/platforms/arm/stm32/spi_hw_manager_stm32.cpp.hpp`

This file contains the `initSpiHardware()` function that initializes all available SPI hardware on STM32 platforms.

### Architecture

The manager follows a helper function pattern with feature flags:

```cpp
namespace fl {
namespace detail {

constexpr int PRIORITY_HW_8 = 8;  // 8-lane (Timer+DMA)
constexpr int PRIORITY_HW_4 = 7;  // 4-lane (Timer+DMA)
constexpr int PRIORITY_HW_2 = 6;  // 2-lane (Timer+DMA)

static void addSpiHw8IfPossible() {
#if FASTLED_STM32_HAS_TIM5
    // Include and register 8-lane Timer+DMA implementation
    #include "platforms/arm/stm32/spi_hw_8_stm32.cpp.hpp"
    // Register instances for each timer
    FL_DBG("STM32: Added TIM5 SpiHw8 controller");
#endif
}

}  // namespace detail

namespace platform {

void initSpiHardware() {
    FL_DBG("STM32: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw8IfPossible();   // Priority 8
    detail::addSpiHw4IfPossible();   // Priority 7
    detail::addSpiHw2IfPossible();   // Priority 6
    // Note: SpiHw1 uses generic fallback, not hardware-accelerated

    FL_DBG("STM32: SPI hardware initialized");
}

}  // namespace platform
}  // namespace fl
```

### Feature Flags

The manager uses these feature flags to conditionally compile hardware support:

- `FASTLED_STM32_HAS_TIM1` - Timer 1 available (F1, F2, F4, F7, H7, L4)
- `FASTLED_STM32_HAS_TIM5` - Timer 5 available (F2, F4, F7, H7)
- `FASTLED_STM32_HAS_TIM8` - Timer 8 available (F1, F2, F4, F7, H7)
- `FASTLED_STM32_DMA_CHANNEL_BASED` - DMA uses channel architecture (F1, L4)
- `FASTLED_STM32_DMA_STREAM_BASED` - DMA uses stream architecture (F2, F4, F7, H7)

### Hardware Implementation

STM32 SPI hardware uses Timer + DMA for parallel output:

**Timer Configuration:**
- Advanced timers (TIM1, TIM5, TIM8) generate SPI clock and data signals
- PWM mode creates precise bit timings for SPI protocol
- Multiple channels enable parallel data lanes

**DMA Configuration:**
- Memory-to-peripheral transfer handles data transmission
- Stream-based (F2/F4/F7/H7) or channel-based (F1/L4) architecture
- Double buffering for smooth frame transmission

### Platform Support

| STM32 Family | SpiHw2 | SpiHw4 | SpiHw8 | Timers | DMA Architecture |
|--------------|--------|--------|--------|--------|------------------|
| STM32F1 | ✅ | ✅ | ❌ | TIM1, TIM8 | Channel-based |
| STM32F2 | ✅ | ✅ | ✅ | TIM1, TIM5, TIM8 | Stream-based |
| STM32F4 | ✅ | ✅ | ✅ | TIM1, TIM5, TIM8 | Stream-based |
| STM32F7 | ✅ | ✅ | ✅ | TIM1, TIM5, TIM8 | Stream-based |
| STM32H7 | ✅ | ✅ | ✅ | TIM1, TIM5, TIM8 | Stream-based |
| STM32L4 | ✅ | ✅ | ❌ | TIM1, TIM8 | Channel-based |

**Note:** 8-lane support requires TIM5, which is only available on F2/F4/F7/H7 families.

### Lazy Initialization

Hardware is only initialized on first access to `SpiHwN::getAll()`, following the Meyer's Singleton pattern:
- No static constructors run at startup
- Avoids initialization order issues
- Zero overhead if SPI hardware is not used
- Thread-safe via static local variables

## RGBW Support

STM32 platforms now support RGBW (4-channel) LED strips like SK6812.

### Features
- **WS2812 RGBW (SK6812)**: Uses the same timing as WS2812 RGB, just 4 bytes per LED instead of 3
- **Automatic detection**: RGBW mode is detected automatically based on strip configuration
- **Mixed RGB/RGBW**: Different strips in the same application can use RGB or RGBW independently
- **Color conversion**: Full RGBW color conversion and white channel management

### Memory Usage
- RGB mode: 3 bytes per LED
- RGBW mode: 4 bytes per LED

### Implementation
The clockless driver (`clockless_arm_stm32.h`) automatically detects RGBW mode and processes 4 bytes per pixel when needed, using the same precise timing as RGB mode.

## Optional feature defines

- **`FASTLED_ALLOW_INTERRUPTS`**: Default `0`. Clockless timing on STM32 typically runs with interrupts off for stability.
- **`FASTLED_ACCURATE_CLOCK`**: Enabled automatically when interrupts are allowed to keep timing math correct.
- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model / memory‑mapped flash).
- **`FASTLED_NO_PINMAP`**: Defined to indicate no PROGMEM‑backed pin maps are used on this platform.
- **`FASTLED_NEEDS_YIELD`**: Defined to signal scheduling/yield points might be required in some cores.

Place defines before including `FastLED.h`.

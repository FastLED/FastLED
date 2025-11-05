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

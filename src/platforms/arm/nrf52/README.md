# FastLED Platform: nRF52

Nordic nRF52 family support.

## Files (quick pass)
- `fastled_arm_nrf52.h`: Aggregator; includes pin/SPI/clockless and sysdefs.
- `fastpin_arm_nrf52.h`, `fastpin_arm_nrf52_variants.h`: Pin helpers/variants.
- `fastspi_arm_nrf52.h`: SPI backend.
- `clockless_arm_nrf52.h`: Clockless driver.
- `arbiter_nrf52.h`: PWM arbitration utility (selects/guards PWM instances for drivers).
- `led_sysdefs_arm_nrf52.h`: System defines for nRF52.
- `malloc_wrappers.cpp`: Weak malloc/free wrappers for Adafruit framework compatibility.

Notes:
- Requires `CLOCKLESS_FREQUENCY` definition in many setups; PWM resources may be shared and must be arbitrated.
 - `arbiter_nrf52.h` exposes a small API to acquire/release PWM instances safely across users; ensure ISR handlers are lightweight.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model).
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`.
- **`FASTLED_ALL_PINS_HARDWARE_SPI`**: Enabled by default unless forcing software SPI.
- **`FASTLED_NRF52_SPIM`**: Select SPIM instance (e.g., `NRF_SPIM0`).
- **`FASTLED_NRF52_ENABLE_PWM_INSTANCE0`**: Enable PWM instance used by clockless.
- **`FASTLED_NRF52_NEVER_INLINE`**: Controls inlining attribute via `FASTLED_NRF52_INLINE_ATTRIBUTE`.
- **`FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING`**: Limit pixels per string in PWM-encoded path (default 144).
- **`FASTLED_NRF52_PWM_ID`**: Select PWM instance index used.
- **`FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING`**: Suppress pin-map warnings for unverified boards.

Define before including `FastLED.h`.

## Memory Allocation (malloc wrappers)

### Background

The Adafruit nRF52 Arduino framework adds linker flags to wrap malloc/free/calloc/realloc:
```
-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc
```

These flags were introduced in v1.1.0 (2021.09.24) to provide thread-safe memory allocation for FreeRTOS environments, preventing heap corruption when the RTOS scheduler and Arduino code concurrently access malloc/free.

### The Problem

When PlatformIO's Library Dependency Finder (LDF) processes a sketch:
1. The framework's `platform.txt` **always** includes `--wrap` linker flags
2. The linker expects to find `__wrap_malloc`, `__wrap_free`, etc.
3. However, the framework's wrapper implementations may be conditionally compiled or not linked
4. Result: `undefined reference to '__wrap_malloc'` linker errors

This occurs even when:
- FreeRTOS features aren't being used
- Thread-safe malloc isn't needed
- The code never explicitly calls malloc

It's purely a build system artifact from how the Adafruit framework is structured.

### The Solution: Weak Symbols

`malloc_wrappers.cpp` provides weak (`__attribute__((weak))`) implementations that:

1. **When framework provides wrappers:**
   - Framework's strong symbols override our weak ones
   - You get full thread-safe malloc with mutex protection
   - Our code is discarded by the linker (zero overhead)

2. **When framework doesn't provide wrappers:**
   - Our weak symbols satisfy the linker requirement
   - They pass through directly to `__real_malloc()` (the original libc malloc)
   - No mutex overhead, just normal malloc behavior

3. **No performance impact:**
   - If framework wrappers exist: uses thread-safe version
   - If not: compiler likely inlines the passthrough (zero overhead)
   - Either way: correct behavior with no runtime cost

### Why This Approach?

We can't modify `platform.txt` (it's in the framework package), and removing `--wrap` flags would break thread-safety for users who need it. Weak symbols provide a universal fallback that:
- ✓ Fixes linker errors when framework wrappers are missing
- ✓ Doesn't interfere when framework wrappers exist
- ✓ Works across different build configurations (Arduino IDE, PlatformIO, etc.)
- ✓ Maintains thread-safety for FreeRTOS users
- ✓ Has zero overhead for non-FreeRTOS users

### Technical Details

The `--wrap` linker option works by:
1. Redirecting calls: `malloc()` → `__wrap_malloc()`
2. Your wrapper can add custom logic (locking, logging, etc.)
3. Call original: `__wrap_malloc()` → `__real_malloc()` (actual libc malloc)

Weak linkage means: "Use this symbol **only if** no stronger symbol exists."
- Strong symbols (normal functions) always win
- Multiple weak symbols = linker error
- Weak symbol used as fallback when nothing stronger available

This is perfect for providing "default implementations" that get replaced by better versions when available.

### References

- Adafruit thread-safe malloc issue: https://github.com/adafruit/Adafruit_nRF52_Arduino/issues/35
- Platform.txt with --wrap flags: https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/platform.txt
- GCC --wrap documentation: https://sourceware.org/binutils/docs/ld/Options.html

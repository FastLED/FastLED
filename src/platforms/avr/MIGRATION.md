# AVR Platform Reorganization - Migration Guide

## Overview

The AVR platform has been reorganized from a flat file structure into a hierarchical, family-based organization. This migration improves maintainability by separating code based on hardware capabilities (instruction set, GPIO architecture).

**Key Point**: This reorganization is **backward compatible**. Existing user code continues to work without changes.

## For Users

### No Changes Required

If you use the standard FastLED include pattern, **no changes are needed**:

```cpp
#include <FastLED.h>

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
}
```

FastLED's platform auto-detection and dispatcher headers route to the correct implementations automatically.

### Minimal Changes (Direct AVR Includes)

If you directly include AVR platform headers (uncommon), you may need to update include paths:

**Before**:
```cpp
#include "platforms/avr/math8_avr.h"
#include "platforms/avr/fastpin_avr_legacy.h"
```

**After**:
```cpp
#include "platforms/avr/atmega/common/math8_avr.h"
#include "platforms/avr/atmega/common/fastpin_avr_legacy_dispatcher.h"
// OR use the backward-compat shim (deprecated):
#include "platforms/avr/fastpin_avr_legacy.h"  // Will emit deprecation warning
```

**Recommendation**: Use dispatcher headers in `platforms/avr/` root instead of family-specific headers directly. This ensures your code works across AVR families.

### What Changed

**Directory Organization**:
- Math implementations separated by capability (software vs hardware multiply)
- Pin mappings separated by MCU family (m328p, m32u4, m2560, m4809, attiny)
- Common code remains in `platforms/avr/` root

**Dispatcher Routing**:
- `math8.h` → routes to ATtiny or ATmega math implementations
- `scale8.h` → routes to software or hardware scaling implementations
- `fastpin_avr.h` → routes to VPORT or legacy pin mappings
- `trig8.h` → routes to ATmega trig implementation (requires MUL instruction)

**File Locations** (selected examples):
- `math8_avr.h` → `atmega/common/math8_avr.h`
- `math8_attiny.h` → `attiny/math/math8_attiny.h`
- `scale8_avr.h` → `atmega/common/scale8_avr.h`
- `scale8_attiny.h` → `attiny/math/scale8_attiny.h`
- `fastpin_avr_atmega4809.h` → `atmega/m4809/fastpin_avr_atmega4809.h`

## For Contributors

### New File Locations

When contributing AVR platform code, place files in the appropriate subdirectory based on hardware capability:

**ATtiny-Specific Code** → `platforms/avr/attiny/`
- Math implementations without MUL instruction → `attiny/math/`
- Pin mappings for any ATtiny variant → `attiny/pins/`

**ATmega-Specific Code** → `platforms/avr/atmega/`
- Code shared across all ATmega variants → `atmega/common/`
- ATmega328P family (UNO, Nano) → `atmega/m328p/`
- ATmega32U4 family (Leonardo, Pro Micro) → `atmega/m32u4/`
- ATmega2560 family (MEGA) → `atmega/m2560/`
- ATmega4809 family (Nano Every, VPORT) → `atmega/m4809/`

**Shared AVR Code** → `platforms/avr/` (root)
- Interrupt control, I/O abstractions, delay primitives
- SPI backends, clockless LED controllers
- Millis timer implementations
- Top-level dispatcher headers

### Adding New ATmega Variants

To add support for a new ATmega variant:

1. **Determine the family**:
   - Is it ATmega328P-like? → Add to `atmega/m328p/fastpin_m328p.h`
   - Is it ATmega32U4-like? → Add to `atmega/m32u4/fastpin_m32u4.h`
   - Is it ATmega2560-like? → Add to `atmega/m2560/fastpin_m2560.h`
   - Is it ATmega4809-like (VPORT)? → Add to `atmega/m4809/` files
   - None of the above? → Add to `atmega/common/fastpin_legacy_other.h`

2. **Add MCU-specific preprocessor guards**:
   ```cpp
   #elif defined(__AVR_ATmegaXXXX__)
       // Pin mappings for ATmegaXXXX
       _FL_DEFPIN(0, 0, D); _FL_DEFPIN(1, 1, D);
       // ... (additional pins)
   ```

3. **Add hardware feature defines**:
   ```cpp
   #define MAX_PIN 19
   #define HAS_HARDWARE_PIN_SUPPORT 1
   #define AVR_HARDWARE_SPI 1  // If hardware SPI available
   #define SPI_DATA 11          // MOSI pin
   #define SPI_CLOCK 13         // SCK pin
   #define SPI_SELECT 10        // SS pin
   ```

4. **Update dispatcher** in `atmega/common/fastpin_avr_legacy_dispatcher.h`:
   ```cpp
   #elif defined(__AVR_ATmegaXXXX__)
       #include "../path/to/your/header.h"
   ```

5. **Test compilation**:
   ```bash
   uv run ci/ci-compile.py your_board --examples Blink
   ```

### Adding New ATtiny Variants

To add support for a new ATtiny variant:

1. **Add MCU defines** to `attiny/pins/fastpin_attiny.h`:
   ```cpp
   #elif defined(__AVR_ATtinyXXXX__)
       // Pin mappings for ATtinyXXXX
       _FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B);
       // ... (additional pins)
   ```

2. **Verify math routing**:
   - Ensure `LIB8_ATTINY` is defined for your variant (in `led_sysdefs_avr.h` or equivalent)
   - This ensures software multiply is used (ATtiny has no MUL instruction)

3. **Update dispatcher** in `atmega/common/fastpin_avr_legacy_dispatcher.h`:
   ```cpp
   #elif defined(__AVR_ATtinyXXXX__) || /* ... other ATtiny defines ... */
       #include "../../attiny/pins/fastpin_attiny.h"
   ```

4. **Test compilation**:
   ```bash
   uv run ci/ci-compile.py attiny_board --examples Blink
   ```

### Code Review Checklist

When reviewing AVR platform changes:

- [ ] Files placed in correct subdirectory (attiny/, atmega/common/, atmega/m328p/, etc.)
- [ ] Preprocessor guards match MCU defines (`__AVR_ATmegaXXXX__`)
- [ ] Dispatcher headers updated if adding new family/variant
- [ ] Math routing correct (ATtiny uses `scale8_attiny.h`, ATmega uses `scale8_avr.h`)
- [ ] Pin routing correct (VPORT for ATmega4809, DDR/PORT/PIN for legacy)
- [ ] Compilation tested on relevant AVR boards
- [ ] Include paths use relative references (`../common/`, `../../attiny/`)
- [ ] No circular dependencies introduced

### Understanding Hardware Capabilities

**Instruction Set Differences**:
- **ATtiny (classic)**: No MUL instruction → software multiplication in `attiny/math/`
- **ATmega (all)**: MUL instruction → hardware multiplication in `atmega/common/`
- **Impact**: Math-intensive operations are slower on ATtiny

**GPIO Register Architecture**:
- **Legacy AVR**: DDR/PORT/PIN registers (most ATmega, all ATtiny)
- **Modern ATmega4809**: VPORT registers (faster, single-cycle GPIO)
- **Impact**: Pin templates differ between `atmega/m4809/` and legacy families

**Dispatcher Pattern**:
```
User code (#include <FastLED.h>)
    ↓
Top-level dispatcher (fastpin_avr.h, math8.h)
    ↓
Family dispatcher (fastpin_avr_legacy_dispatcher.h)
    ↓
Family-specific implementation (m328p/, m32u4/, attiny/, etc.)
```

**Preprocessor Routing**:
- `LIB8_ATTINY` → defined for ATtiny → routes to `attiny/math/`
- `__AVR_ATmega4809__` → defined for ATmega4809 → routes to `atmega/m4809/`
- Default → routes to `atmega/common/` or family-specific header

## Breaking Changes

**None**. This migration maintains full backward compatibility.

All existing include patterns continue to work:
- `#include <FastLED.h>` → unchanged (recommended)
- `#include "platforms/avr/fastled_avr.h"` → unchanged
- `#include "platforms/avr/fastpin_avr_legacy.h"` → forwarding header maintained (deprecated)

## Deprecations

### fastpin_avr_legacy.h (Root Directory)

**Status**: Deprecated, will be removed in FastLED v4.0

**Current Behavior**:
- Forwards to `atmega/common/fastpin_avr_legacy_dispatcher.h`
- Emits compiler warning about deprecation

**Migration Path**:
```cpp
// Old (deprecated):
#include "platforms/avr/fastpin_avr_legacy.h"

// New (recommended):
#include "platforms/avr/fastpin_avr.h"  // Use top-level dispatcher
// OR (if you need legacy dispatcher directly):
#include "platforms/avr/atmega/common/fastpin_avr_legacy_dispatcher.h"
```

**Timeline**: Forwarding header will be removed in FastLED v4.0 (breaking change release)

## Testing Your Changes

### Compilation Testing

After making AVR platform changes, test compilation on representative boards:

```bash
# ATmega328P family (UNO, Nano)
uv run ci/ci-compile.py uno --examples Blink ColorPalette

# ATmega32U4 family (Leonardo, Pro Micro)
uv run ci/ci-compile.py atmega32u4_leonardo --examples Blink

# ATtiny family (classic)
uv run ci/ci-compile.py attiny85 --examples Blink

# ATmega4809 family (Nano Every, VPORT)
uv run ci/ci-compile.py uno_wifi_rev2 --examples Blink
```

### Math Routing Validation

Verify correct math implementation is used:

```bash
# ATmega328P should use hardware multiply (scale8_avr.h)
uv run ci/ci-compile.py uno --examples ColorPalette

# ATtiny85 should use software multiply (scale8_attiny.h)
uv run ci/ci-compile.py attiny85 --examples ColorPalette
```

Both should compile successfully. ATtiny binary may be slightly larger due to software multiply.

### Pin Routing Validation

Verify correct pin mappings are used:

```bash
# UNO should use m328p pin mappings (20 digital pins)
uv run ci/ci-compile.py uno --examples Blink

# Leonardo should use m32u4 pin mappings (24 digital pins, USB-capable)
uv run ci/ci-compile.py atmega32u4_leonardo --examples Blink

# Nano Every should use VPORT pin mappings (ATmega4809)
uv run ci/ci-compile.py uno_wifi_rev2 --examples Blink
```

## Rationale

### Why Reorganize?

**Before**: Flat directory with 32 files, including 500-line "god-header" for pin mappings

**Problems**:
1. Hard to find relevant code for specific MCU family
2. Single 500-line header difficult for humans and AI to understand
3. No clear separation between hardware capabilities (MUL instruction, VPORT registers)
4. High cognitive load when adding new variants

**After**: Hierarchical structure organized by hardware capability

**Benefits**:
1. Clear separation by instruction set (ATtiny vs ATmega)
2. Family-specific pin mappings in focused files (50-100 lines each)
3. Common code shared in `common/` subdirectories
4. Easy to locate code for specific board (UNO → `atmega/m328p/`)
5. Reduced cognitive load for contributors

### Design Principles

1. **Hardware Capability Separation**: Code grouped by instruction set and GPIO architecture
2. **Backward Compatibility**: All existing code continues to work unchanged
3. **Transparent Dispatching**: Preprocessor routes to correct implementation automatically
4. **Minimal Duplication**: Common code shared via `common/` subdirectories
5. **Git History Preservation**: All moves use `git mv` to preserve blame/history

## Support

If you encounter issues related to this reorganization:

1. **Check dispatcher routing**: Verify preprocessor defines route to correct family
2. **Verify include paths**: Ensure relative paths use correct `../` references
3. **Test compilation**: Compile example on your target board to verify correctness
4. **Report issues**: Open GitHub issue with board type, compiler output, and error message

For questions about contributing AVR platform code, see the [README.md](README.md) directory structure section.

## Migration Timeline

- **FastLED v3.x**: Reorganization complete, backward compatibility maintained
- **FastLED v4.0** (future): Remove deprecated forwarding headers (breaking change)

**Current Status**: Migration complete (as of iteration 11), all tests passing.

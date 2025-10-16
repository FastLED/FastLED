# FastLED Platform: shared

Common, platform‑agnostic components used by multiple targets (data export, UI, SPI drivers).

## Subdirectories
- `active_strip_data/`: Zero‑copy strip data export and screenmap tracking across frames. See `active_strip_data.h` for `ActiveStripData` manager and JSON helpers.
- `ui/`: JSON‑driven UI system. `ui/json/` contains `JsonUiManager`, components (Slider, Button, Checkbox, Dropdown, NumberField, Audio, Title, Description), and integration guide (`readme.md`).

## Multi-Lane SPI Support

Shared platform code includes multi-lane SPI transposer infrastructure supporting 1/2/4/8 parallel data lanes:

### Unified SPI Transposer (spi_transposer.*)
**NEW**: Unified bit-interleaving engine for all parallel SPI widths. Transposes sequential pixel data into parallel bitstreams for simultaneous multi-strip output.

**Supported Widths:**
- 1-lane (single-SPI)
- 2-lane (dual-SPI) via `transpose2()`
- 4-lane (quad-SPI) via `transpose4()`
- 8-lane (octo-SPI) via `transpose8()` ✨

**Key Features:**
- **Single unified API**: All transpose operations in `SPITransposer` class
- **Consistent naming**: `transpose2()`, `transpose4()`, `transpose8()` methods
- **Optimized algorithms**: Width-specific interleaving for each configuration
- **Platform-agnostic**: Used by both hardware and software implementations
- **Zero-copy operation**: Efficient memory handling

**Legacy Compatibility:**
- `spi_transposer_dual.*` - Deprecated wrappers for 2-lane (calls `transpose2()`)
- `spi_transposer_quad.*` - Deprecated wrappers for 4-lane/8-lane (calls `transpose4()`/`transpose8()`)

### SPI Hardware Interfaces
Platform-independent SPI configuration APIs organized by lane count:

**Interfaces:**
- `spi_hw_1.h` - Single-lane SPI (`SpiHw1`)
- `spi_hw_2.h` - Dual-lane SPI (`SpiHw2`)
- `spi_hw_4.h` - 4-lane SPI (`SpiHw4`)
- `spi_hw_8.h` - 8-lane SPI (`SpiHw8`) ✨ **NEW**

**Example Configuration:**
```cpp
namespace fl {
struct SpiHw8 {
    struct Config {
        uint8_t data0, data1, data2, data3;  // First 4 data pins
        uint8_t data4, data5, data6, data7;  // Next 4 data pins (8-lane)
        uint8_t clock;                        // Shared clock pin
        // ... other config
    };
};
}
```

**Configuration supports:**
- Explicit lane count per interface (no ambiguity)
- Clock pin assignment
- Hardware vs software fallback selection
- Per-platform capabilities detection

### Hardware vs Software
The transposer and interfaces are used by both:
- **Hardware SPI** (ESP32-P4 octal, ESP32 quad): DMA-accelerated, ~40× faster
- **Software SPI** (bitbang ISR/blocking): Platform-agnostic fallback for all platforms

See platform-specific READMEs for implementation details:
- `src/platforms/esp/32/README.md` - ESP32 hardware multi-lane SPI
- `src/platforms/shared/spi_bitbang/README.md` - Software multi-width SPI

## Behavior and integration
- `ActiveStripData` listens to `EngineEvents` to clear and publish per‑frame data; exposes legacy and new JSON creators and parsing helpers, plus on‑demand screenmap updates.
- The UI system uses weak pointers for safe component lifecycle, and a manager that batches outbound JSON and applies inbound updates on frame boundaries.

## Example export flow (frame lifecycle)

1. During `show()`, the engine renders pixels. When the frame completes, `EngineEvents::onEndShowLeds` fires.
2. `ActiveStripData` listens and clears per‑frame accumulators, then snapshots strip/screenmap data for export.
3. The platform bridge pulls JSON or raw buffers (depending on target) and forwards them to the UI/consumer.
4. After UI updates arrive, `EngineEvents::onEndFrame` applies any pending control changes via the UI manager.

Bridge stub outline:

```cpp
// Pseudocode for a host bridge loop
void pump() {
    if (auto json = ActiveStripData::createLegacyJson(/*alloc*/)) {
        platform_send_json(json.ptr);
        ActiveStripData::freeJson(json.ptr);
    }
    if (auto incoming = platform_recv_ui_json()) {
        JsonUiManager::instance().updateUiComponents(incoming);
    }
}
```

# FastLED Platform: shared

Common, platform‑agnostic components used by multiple targets (data export, UI, SPI drivers).

## Subdirectories
- `active_strip_data/`: Zero‑copy strip data export and screenmap tracking across frames. See `active_strip_data.h` for `ActiveStripData` manager and JSON helpers.
- `ui/`: JSON‑driven UI system. `ui/json/` contains `JsonUiManager`, components (Slider, Button, Checkbox, Dropdown, NumberField, Audio, Title, Description), and integration guide (`readme.md`).

## Multi-Lane SPI Support

Shared platform code includes multi-lane SPI transposer infrastructure supporting 1/2/4/8 parallel data lanes:

### SPI Transposer (spi_transposer_quad.*)
Bit-interleaving engine for parallel SPI transmission. Transposes sequential pixel data into parallel bitstreams for simultaneous multi-strip output.

**Supported Widths:**
- 1-lane (single-SPI)
- 2-lane (dual-SPI)
- 4-lane (quad-SPI)
- 8-lane (octo-SPI) ✨ New in ESP32-P4 hardware implementation

**Key Features:**
- Automatic width detection based on configured data pins
- Optimized bit-interleaving algorithms for each width
- Platform-agnostic (used by both hardware and software implementations)
- Zero-copy operation where possible

### SPI Configuration (spi_quad.h)
Platform-independent SPI configuration API:

```cpp
namespace fl {
struct SPIQuad {
    struct Config {
        uint8_t data0, data1, data2, data3;  // Quad-SPI pins
        uint8_t data4, data5, data6, data7;  // Octo-SPI pins (ESP32-P4)
        uint8_t clock;
        // ... other config
    };
};
}
```

**Configuration supports:**
- Variable width (1-8 data pins)
- Clock pin assignment
- Hardware vs software fallback selection
- Per-platform capabilities detection

### Hardware vs Software
The transposer is used by both:
- **Hardware SPI** (ESP32-P4 octal, ESP32 quad): DMA-accelerated, ~40× faster
- **Software SPI** (ESP32-C3 ISR/blocking): Bit-banged fallback for all platforms

See platform-specific READMEs for implementation details:
- `src/platforms/esp/32/README.md` - ESP32 hardware octal-SPI
- `src/platforms/esp/32/parallel_spi/README.md` - Software multi-width SPI

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

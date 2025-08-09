# FastLED Platform: shared

Common, platform‑agnostic components used by multiple targets (data export, UI).

## Subdirectories
- `active_strip_data/`: Zero‑copy strip data export and screenmap tracking across frames. See `active_strip_data.h` for `ActiveStripData` manager and JSON helpers.
- `ui/`: JSON‑driven UI system. `ui/json/` contains `JsonUiManager`, components (Slider, Button, Checkbox, Dropdown, NumberField, Audio, Title, Description), and integration guide (`readme.md`).

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

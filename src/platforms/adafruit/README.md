# FastLED Platform: adafruit

Adafruit_NeoPixel integration providing a FastLED‑compatible clockless controller implemented on top of Adafruit’s driver.

## Files (quick pass)
- `clockless.h`: Adapter layer. Exposes a `ClocklessController` template that marshals FastLED pixel data to an `Adafruit_NeoPixel` instance. Requires `Adafruit_NeoPixel.h` to be available; timing (T1/T2/T3) is managed by the Adafruit library.

## Usage and detection
- Controlled by `FASTLED_USE_ADAFRUIT_NEOPIXEL` (defaults to undefined unless building docs). If enabled but `Adafruit_NeoPixel.h` is missing, the adapter disables itself with an error.
- Color order: FastLED’s `PixelController` applies RGB ordering; the adapter always feeds RGB into Adafruit’s API.
- Timings: `T1/T2/T3` template params are ignored here; Adafruit’s driver handles signal generation and timing per platform.

## Notes
- This path is useful when Adafruit’s platform backends (e.g., some boards/cores) are preferred or more stable for your setup.
- Performance characteristics and memory usage follow Adafruit_NeoPixel; expect different throughput vs native FastLED clockless drivers.

## Optional feature defines

- **`FASTLED_USE_ADAFRUIT_NEOPIXEL`**: Default undefined (unless building docs via `FASTLED_DOXYGEN`). When defined, enables the Adafruit adapter; requires `Adafruit_NeoPixel.h`.

Define before including `FastLED.h`.
## Compatibility and color order

Supported color orders: FastLED’s `PixelController` handles byte reordering before passing data to Adafruit_NeoPixel. Typical orders like GRB/RGB/BRG are supported transparently.

Constraints vs native FastLED timing:

- Timing is wholly delegated to Adafruit_NeoPixel. The `T1/T2/T3` template parameters are ignored; use Adafruit’s platform timings.
- Throughput and CPU usage may differ from FastLED’s native clockless or RMT/I2S backends. If you need multi‑strip parallelism or strict ISR windows, consider native drivers instead.

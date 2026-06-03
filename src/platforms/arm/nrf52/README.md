# FastLED Platform: nRF52

Nordic nRF52 family support.

The maintained backend targets the Adafruit nRF52 / Bluefruit Arduino BSP used
by FastLED CI. That BSP provides the Nordic SDK headers, FreeRTOS integration,
SoftDevice support, PWM EasyDMA, and SPIM HAL APIs this backend includes.
Arduino mbed OS nRF52 boards such as Arduino Nano 33 BLE and Nano 33 BLE Sense
are not supported by this backend today.

## Clocked SPI LEDs

Clocked chipsets such as APA102, SK9822, and DotStar use the nRF52 SPIM
peripheral. `DATA_RATE_MHZ()` and `DATA_RATE_KHZ()` are honored by selecting
the fastest standard nRF52 SPIM rate that does not exceed the requested rate:
125 kHz, 250 kHz, 500 kHz, 1 MHz, 2 MHz, 4 MHz, or 8 MHz. The default
`FASTLED_NRF52_SPIM` is `NRF_SPIM0`, so requests above 8 MHz are clamped to
8 MHz. SPIM3 on nRF52840 can run faster, but this backend does not assume
SPIM3 because most board pin maps and sketches use SPIM0 by default.

## Clockless LEDs and BLE

WS2812/SK6812-style chipsets use PWM EasyDMA sequence playback. The current
clockless path uses one PWM arbiter instance by default and a static sequence
buffer sized by `FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING` (default 144). BLE
SoftDevice activity, RAM pressure, and multiple independent clockless strips
can make large SK6812/WS2812 installations unreliable on nRF52. For BLE-heavy
applications, prefer clocked LEDs such as APA102/SK9822/DotStar, reduce the
number of clockless strips, or use a single shorter clockless output.

WS2812/SK6812 LEDs also do not expose a separate update/latch signal. FastLED
sends the pixel waveform and then leaves the data line idle for the reset time.
If an application needs precisely scheduled visual updates independent of data
transfer time, use a clocked LED chipset or schedule the start of `show()` so
the protocol reset interval lands at the desired update time.

## Files (quick pass)
- `fastled_arm_nrf52.h`: Aggregator; includes pin/SPI/clockless and sysdefs.
- `fastpin_arm_nrf52.h`, `fastpin_arm_nrf52_variants.h`: Pin helpers/variants.
- `fastspi_arm_nrf52.h`: SPI backend.
- `clockless_arm_nrf52.h`: Clockless driver.
- `arbiter_nrf52.h`: PWM arbitration utility (selects/guards PWM instances for drivers).
- `led_sysdefs_arm_nrf52.h`: System defines for nRF52.


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
- **`FASTLED_NRF52_ALLOW_NFC_PINS`**: Re-enable `P0.09` / `P0.10` (NFC1/NFC2) as valid FastLED output pins on every nRF52840 variant block (SuperMini, nice!nano v2, nRFMicro, XIAO nRF52840, XIAO nRF52840 Sense). Defining this macro is necessary but **not** sufficient: you also need either `CONFIG_NFCT_PINS_AS_GPIOS=1` in the BSP/build flags, **or** a call to `NFC_PINS_AS_GPIO()` at boot (Adafruit nRF52 core helper).
- **`FASTLED_NRF52_ALLOW_USR_LED`**: Re-enable on-board user-LED pins (`P0.15` SuperMini/nice!nano v2, `P1.10` nRFMicro, and the XIAO RGB LEDs at `P0.26`/`P0.06`/`P0.30`) as valid FastLED output pins. Be careful: defining this risks a clash with any sketch (or BSP background task) that calls `digitalWrite(LED_BUILTIN, ...)` on the same pin.

`EXT_VCC` enable pins (`P0.13` SuperMini/nice!nano v2, `P1.09` nRFMicro), VBAT-sense pins, HICHG/~CHG, and QSPI flash pins remain invalid unconditionally — toggling `EXT_VCC` mid-`show()` browns out the regulator, and the others are not safe for FastLED output.

Define before including `FastLED.h`.

### References

- Adafruit thread-safe malloc issue: https://github.com/adafruit/Adafruit_nRF52_Arduino/issues/35
- Platform.txt with --wrap flags: https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/platform.txt
- GCC --wrap documentation: https://sourceware.org/binutils/docs/ld/Options.html

# FastLED Platform: Teensy 3.x (K20)

Teensy 3.0/3.1/3.2 support (MK20DX family).

## Files (quick pass)
- `fastled_arm_k20.h`: Aggregator; includes pin/SPI/clockless and helper controllers.
- `fastpin_arm_k20.h`: Pin helpers.
- `fastspi_arm_k20.h`: SPI output backend.
- `clockless_arm_k20.h`: Single-lane clockless driver using DWT cycle counter.
- `clockless_block_arm_k20.h`: Block/multi-lane variant.
- `clockless_objectfled.*`: ObjectFLED experimental clockless implementation.
- `octows2811_controller.h`: OctoWS2811 parallel output integration.
- `ws2812serial_controller.h`: UART-style serial WS2812 controller.
- `smartmatrix_t3.h`: SmartMatrix support for Teensy 3.x.
- `led_sysdefs_arm_k20.h`: System defines for Teensy 3.x.

Notes:
- DWT cycle counter is used for precise timing; interrupt windows are checked to punt frames when overrun.
 - Consider `FASTLED_ALLOW_INTERRUPTS=1` for responsiveness; long ISRs risk jitter and retries.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `1` on some Teensy3 cores.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `1`. Enables `FASTLED_ACCURATE_CLOCK`.
- **ObjectFLED**
  - **`FASTLED_OBJECTFLED_LATCH_DELAY`**: WS2812 latch delay microseconds for ObjectFLED path (default `300`).

Place defines before including `FastLED.h`.

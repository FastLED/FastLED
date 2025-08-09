# FastLED Platform: Ambiq Apollo3 (Artemis)

## Optional feature defines

- `FASTLED_ALLOW_INTERRUPTS` (0|1): Allow brief interrupts during show; default 1.
- `FASTLED_USE_PROGMEM` (0|1): PROGMEM usage; default 0 (flat memory model).
- `FASTLED_FORCE_SOFTWARE_PINS` (defined): Force software pin access (disables fast GPIO path).
- `FASTLED_ALL_PINS_HARDWARE_SPI` (defined): Enabled automatically for this platform; not typically overridden.

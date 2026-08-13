# Chipintelli CI13XX

This backend adds clockless LED output for the 32-bit RISC-V CI1302, CI1303,
and CI1306 Arduino targets. It uses the machine cycle counter for waveform
timing and masked GPIO data registers for single-store pin transitions.

The implementation is selected only when `ARDUINO_ARCH_CI13XX` is defined.
Other FastLED platform paths and their pin implementations are unchanged.

## Supported pins

- CI1302/CI1303: PA2-PA6, PB5-PB6 and PC4. PA0-PA1 are also available when
  the internal oscillator is selected.
- CI1306 / CI-D06GT01D: PA2-PA7, PB0-PB7, PC0-PC5, PD0-PD1 and PD3-PD4.

The Arduino pin aliases (for example `PA4` or `PD0`) can be used directly as
the `DATA_PIN` template argument. PD2 and PD5 are not bonded on the supported
CI1306 boards and are intentionally rejected at compile time.

The CI13XX clockless implementation keeps interrupts disabled while a frame is
transmitted so WS2812-family timing is not disturbed.

## Nuclei GCC 9.2 LTO compatibility

The CI13XX Arduino toolchain's GCC 9.2 linker can fail internally while
processing weak constructor aliases emitted in multiple FastLED unity-build
objects. The small `s16x16` integer constructor is force-inlined so those
out-of-line aliases are not emitted; CI13XX builds can retain LTO without extra
flags.

Older FastLED packages that predate this fix can use the following fallback:

```console
arduino-cli compile --build-property compiler.cpp.extra_flags=-fno-lto ...
```

The fallback is scoped to the CI13XX build and is not required by other FastLED
targets.

# FastLED Platform: arm

ARM support for a wide range of MCUs (Teensy 3.x/4.x, SAMD21/SAMD51, RP2040, STM32, Renesas UNO R4, nRF5x, etc.).

## Top-level files (quick pass)
- **arm_compile.hpp**: Hierarchical include hook for ARM (e.g., Teensy 4.x `__IMXRT1062__`). Source inclusion is handled by CMake globs.
- **compile_test.hpp**: Compile-time assertions for platform flags on ARM families (PROGMEM usage, interrupts policy, F_CPU, memory hints).
- **int.h**: ARM-friendly integer and pointer typedefs in `fl::`.

## Subplatform directories
- **common/**: Shared helpers for Cortex-M0/M0+; `m0clockless.h` provides SysTick fallback and clockless ASM macros used by SAMD21, etc.
- **d21/**: SAMD21 family (e.g., Arduino Zero-class). Includes `clockless_arm_d21.h` and pin helpers.
- **d51/**: SAMD51 family (e.g., Feather M4, ItsyBitsy M4, Wio Terminal). Clockless + SPI core integration.
- **giga/**: Arduino GIGA R1 (STM32H747). LED sysdefs and clockless/SPI wiring for this target.
- **k20/**: Teensy 3.x (MK20DX). Clockless + block clockless, `fastspi_arm_k20.h`, and integrations: `octows2811_controller.h`, `ws2812serial_controller.h`, `smartmatrix_t3.h`.
- **k66/**: Teensy 3.6 (K66). Similar to k20 with clockless and block variants; reuses some k20 controllers.
- **kl26/**: Teensy LC (KL26Z64). Clockless and `ws2812serial_controller` support.
- **mxrt1062/**: Teensy 4.x (i.MX RT1062). Clockless and block clockless, plus OctoWS2811 and SmartMatrix integrations.
- **nrf51/**: Nordic nRF51. Clockless and pin/SPI wiring for this series.
- **nrf52/**: Nordic nRF52. Adds `arbiter_nrf52.h` (PWM resource arbitration), clockless, pin/SPI, and sysdefs.
- **renesas/**: Renesas RA4M1 (Arduino UNO R4). Clockless + SPI core integration and sysdefs.
- **rp2040/**: Raspberry Pi Pico (RP2040). Clockless via PIO; `pio_gen.h` builds a PIO program from T1/T2/T3 at runtime.
- **sam/**: Arduino Due (SAM3X). Clockless and block clockless, pin/SPI wiring for SAM.
- **stm32/**: STM32 (e.g., F1). Pin helpers and clockless driver for STM32 family.

## Quick guidance per subplatform

- d21 (SAMD21): `FASTLED_USE_PROGMEM=0`; clockless via `arm/common/m0clockless.h`. Keep ISRs short; SysTick‑based timing.
- d51 (SAMD51): Higher clocks; enable interrupts cautiously. Prefer minimal critical sections.
- k20/k66 (Teensy 3.x): DWT cycle counter timing; long ISRs can force retries. OctoWS2811/SmartMatrix available.
- mxrt1062 (Teensy 4.x): Very high frequency; mind DWT and interrupt thresholds. Parallel output offload recommended for large installations.
- rp2040: PIO‑driven; ensure T1/T2/T3 match LED timing; regenerate PIO program when timings change.
- nrf51/nrf52: Budget interrupt windows conservatively. On nrf52, arbitrate PWM instances via `arbiter_nrf52.h`.
- renesas (UNO R4): Ensure sysdefs set `FASTLED_USE_PROGMEM=0`; verify cli/sei equivalents and F_CPU.
- stm32/giga: Use ARM IRQ wrappers in sysdefs; direct register access varies by core.

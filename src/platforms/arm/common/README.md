# FastLED Platform: arm/common

Shared helpers for ARM Cortex-M0/M0+ clockless output.

- `m0clockless.h`: SysTick fallback and macro/ASM infrastructure used by M0/M0+ clockless drivers (e.g., SAMD21). Provides delay macros, bit-write helpers, and frame loop structure for tight WS281x timing.

Notes:
- Designed to be included by target-specific clockless headers (e.g., `clockless_arm_d21.h`).

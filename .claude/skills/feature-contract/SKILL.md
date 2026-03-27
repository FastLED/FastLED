---
name: feature-contract
description: Generate a structured implementation contract before making any code changes to FastLED. Defines scope, affected files, API changes, platform impact, risk assessment, and test plan. Use before implementing any feature, bug fix, driver addition, or refactoring.
argument-hint: <feature or bug description>
context: fork
agent: feature-contract-agent
---

Generate an implementation contract for a FastLED change. The contract must be reviewed before any code is written.

$ARGUMENTS

## Why Use a Contract

FastLED runs on 100+ microcontroller platforms. A change that seems simple can break:
- Platform-specific driver behavior (ESP32, Teensy, AVR, STM32, RP2040, nRF52...)
- Public API compatibility (users pin to specific versions)
- Timing-sensitive LED protocols (a single wrong bit ruins the strip)
- Build-system portability (meson, PlatformIO, Arduino IDE all involved)

The contract documents what changes, what risks exist, and how to prove it works — before a line of code is written.

## Steps

### 1. Understand the Request

Determine:
- What is the requested change? (feature / bug fix / refactor / platform port / driver addition)
- Which LED chipsets or platforms are affected?
- What is the expected behavior after the change?
- Are there hardware constraints? (specific ESP32 variant, specific LED protocol, pin count)
- Is this a public API change or internal implementation?

### 2. Define Scope

Document:
- **Summary**: One-paragraph description of the change
- **Non-goals**: What this change explicitly does NOT do
- **Affected files**: Every file to be created, modified, or deleted
- **APIs touched**: Public functions, types, or macros that will change

### 3. Assess Platform Impact

For each affected area, evaluate impact:

| Platform Area | Impact | Notes |
|--------------|--------|-------|
| AVR (Arduino Uno/Mega/Nano) | [None/Low/Med/High] | RAM-constrained, no FPU |
| ESP32 (all variants) | [None/Low/Med/High] | RMT/I2S/SPI/PARLIO drivers |
| Teensy (3.x, 4.x) | [None/Low/Med/High] | ARM Cortex-M, bitbang & DMA |
| RP2040 (Raspberry Pi Pico) | [None/Low/Med/High] | PIO-based drivers |
| STM32 | [None/Low/Med/High] | HAL dependencies |
| WASM (web browser target) | [None/Low/Med/High] | Emulation only |
| nRF52 | [None/Low/Med/High] | BLE coexistence |

### 4. Assess Risks

Evaluate each risk category:

**Timing Risks** (critical for LED protocols):
- Does the change affect bit-timing (T0H, T1H, reset pulse)?
- Could added latency cause LED flicker or color errors?
- Is the change tested on hardware, not just WASM/emulation?

**API Compatibility Risks**:
- Does this change any public header or function signature?
- Will existing user sketches compile without modification?
- Is a deprecation path needed?

**Memory Risks**:
- Does this add to SRAM usage (critical for AVR with 2KB)?
- Does this add to flash usage?
- Are heap allocations properly checked for NULL?

**Concurrency Risks** (ESP32 / RTOS platforms):
- Shared state accessed from multiple tasks or ISR?
- DMA buffer lifetime during transfer?
- Interrupt-safe API used where required?

**Build System Risks**:
- Does this require changes to meson.build, library.properties, or component.mk?
- Is the new code compiled only on relevant platforms (guards correct)?

### 5. Define Test Plan

Specify the test layers that apply:

| Layer | Applies | Tests |
|-------|---------|-------|
| Host unit tests (`bash test`) | Always | Logic, math, data structures |
| WASM compile check (`bash compile wasm`) | Always | Catches most compile errors |
| Platform compile (`bash compile <platform>`) | If driver code | Catches platform-specific errors |
| Hardware validation (`bash validate`) | If driver or timing change | Only definitive proof for LED output |

List specific test cases:
- **Positive**: Expected behavior with valid inputs
- **Negative**: Error handling, boundary values
- **Regression**: Existing tests that must still pass

### 6. Define Rollback Strategy

Document:
- How to revert the change (git revert, feature flag, etc.)
- Whether any NVS/EEPROM data migration is needed
- Whether users need to update their code if the API changes

### 7. Generate Contract Document

```markdown
# Implementation Contract: [Title]

## Change Summary
[One-paragraph description of what changes and why]

## Non-Goals
- [What this explicitly does NOT include]

## Affected Files
| File | Action | Description |
|------|--------|-------------|
| src/platforms/esp/32/foo.h | MODIFY | [What changes] |
| src/fl/bar.h | CREATE | [Purpose] |

## APIs Touched
- `FastLED.addLeds<...>()` — [How it changes, if at all]
- `CRGB` — [Impact]

## Platform Impact
| Platform | Impact | Rationale |
|---------|--------|-----------|
| AVR | None | No new code on AVR path |
| ESP32 | High | New RMT encoder implementation |
| ... | ... | ... |

## Risk Assessment

### Timing: [LOW / MED / HIGH]
[Details — will LED protocols still work?]

### API Compatibility: [LOW / MED / HIGH]
[Details — will existing user code still compile and run?]

### Memory: [LOW / MED / HIGH]
[Details — RAM/flash impact, especially for AVR]

### Concurrency: [LOW / MED / HIGH]
[Details — ISR safety, DMA lifetime, task interaction]

### Build System: [LOW / MED / HIGH]
[Details — meson.build, guards, new dependencies]

## Test Plan
- [ ] Host unit test: [test name / description]
- [ ] WASM compile: `bash compile wasm --examples Blink`
- [ ] Platform compile: `bash compile <platform>`
- [ ] Hardware validation: [specific test on which board]
- [ ] Regression: `bash test --cpp` — all existing tests pass

## Rollback Strategy
[Steps to safely revert this change]

## Acceptance Criteria
- [ ] [Specific, measurable criterion]
- [ ] All platform compiles pass
- [ ] No regressions in host test suite
- [ ] [Hardware test evidence if applicable]
```

## Rules

1. **Never skip the contract for non-trivial changes.** A "small" change to a shared header can break 100 platform builds.
2. **Define platform impact explicitly.** Unknown = HIGH risk. Investigate before marking LOW.
3. **Hardware validation is required for any driver or timing change.** WASM compilation is not sufficient proof.
4. **Public API changes need a compatibility note.** FastLED users should not be surprised by breaking changes.
5. **Update the contract if scope changes.** Scope creep must be re-reviewed.

## What Makes a Good Acceptance Criterion

Good: "WS2812 output on ESP32 shows correct colors at 1000 LEDs without flicker, confirmed by hardware observation"
Bad: "It works"

Good: "All existing host tests pass with `bash test --cpp`"
Bad: "Tests pass"

Good: "Sketch compiles on Arduino IDE 2.x with no warnings"
Bad: "Compiles somewhere"

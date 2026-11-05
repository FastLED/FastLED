# Cross-platform software SPI audit

Parent: https://github.com/FastLED/FastLED/issues/5024
Baseline: `94dab9c6b720ea277ec9177c93129c7dc646c5c3`.

## Plan

- [x] Research the shared production writer and establish the parent issue.
- [x] Run three disjoint Sol discovery audits; verify 27 native child relationships.
- [ ] Consolidate all source candidates, platform aliases and negative findings.
- [x] Read the children back and choose a serial implementation order.
- [ ] Establish a generated discovery gate coupled to behavioral evidence.
- [ ] Fix each positive with one implementation agent active at a time.
- [ ] Run focused RED → GREEN and restored-defect mutation checks for each fix.
- [ ] Complete native/platform validation, review and merged implementation PRs.
- [ ] Have an independent Sol `judge` rerun discovery and audit the final evidence.
- [ ] Close the children and parent only after their acceptance criteria pass.

## Discovery ownership

1. AVR and Teensy: AVR implementations, K20/K66/KL26/i.MX RT and relocated aliases.
2. Other ARM: every remaining ARM platform, including dispatch and variant banks.
3. Shared/ESP/miscellaneous: root dispatch, generic and ISR software SPI, ESP32,
   ESP8266 exceptions, Apollo3, CI13xx, stub, WASM and Arduino fallback paths.

Discovery agents may file issues but do not edit production code. The parent
orchestrator owns validation. Implementation begins only after reading the
discovery results back; implementation agents run serially.

## Preliminary independent judge findings

- ESP32 Hz versus cycle-divider mismatch needs its own correctness finding.
- Apollo3 has a direct platform include route; central FastPin dispatch alone
  cannot establish its reachability.
- Runtime SPIBusManager and BitBangChannelDriver loops are separate candidates.
- Legacy duplicate headers, pin banks and inverted mappings need explicit cases.
- The baseline denominator includes all 2,986 tracked `src` files, including
  zero-query matches; five query families currently identify 160 candidates.
- Preserve all six evidence requirements: immutable source accounting, actual
  production execution, configuration matrices, timing units/boundaries,
  restored-defect mutations, and distinct evidence classes.
- Preflight is not final certification. The judge must rerun the completed gate.

## Gate contract

Searches discover candidates; they are not proof of a fix. Freeze the baseline
inventory independently of the patterns so renaming a class or deleting an
optimization token cannot hide an existing backend. New candidate files and
changed dispatch/backend source require classification. Keep aliases and
negative findings visible, with concrete source evidence.

For each positive, the acceptance gate must exercise the actual production
implementation or its compiled target output. A metadata status, policy flag,
closed GitHub issue, copied algorithm, disabled test or reduced query scope
cannot establish completion. Restore the original defect and prove its focused
signal fails. Include boundaries, empty transfers, tails, start bits, bank
selection, atomic mode and phase timing where applicable.

Separate physical waveform/throughput evidence from compilation, instruction
inspection and simulation. Never transfer ESP8266 latency constants to another
chip without evidence. Do not claim hardware measurements for unattached boards.

## Next steps

The sole implementation agent is writing RED fixtures for #5031. The parent
will run and record RED before allowing its implementation. Then fix #5051
and #5027 rate correctness, #5032 shared delay, and each backend/runtime child
serially. The independent judge must look for omitted configurations, vacuous
gates and source/manifest drift.

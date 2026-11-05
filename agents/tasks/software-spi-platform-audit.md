# Cross-platform software SPI audit

Parent: https://github.com/FastLED/FastLED/issues/5024
Baseline: `94dab9c6b720ea277ec9177c93129c7dc646c5c3`.

## Plan

- [x] Research the shared production writer and establish the parent issue.
- [x] Run three disjoint Sol discovery audits; verify 32 native child relationships (including judge- and regression-discovered gaps).
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
- Native AVR SPI/UART GPIO methods (#5052) and SAMD SERCOM GPIO handoff
  (#5053) invalidate broad hardware-only exclusions; classify per entrypoint.
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

The #5031 foundation has 31 passing Python tests and passing actual generic
writer traces in normal/debug mode. The inverted-clock target is GREEN for
#5044, and three actual production mutations fail during execution. The audit
CLI accounts for all 2,986 baseline/current source files. There were 31 native
children before the new #5057 init-clock finding; the complete gate remains
unfinished. No target runner or fabricated receipt is accepted as proof.

After the foundation lint handoff, validate #5057's observed first-edge RED
and minimal logical idle-low initialization fix, then address #5056 forced-pin
build failures, #5052 AVR bit selection, #5051/#5027 rate correctness, #5032 shared
delay and the remaining backend/runtime children serially. #5031 remains open
until real execution/configuration/mutation runners and a valid positive
control are wired. The judge must independently certify the final result.

## Foundation code review follow-up

The independent Sol judge reviewed pinned `94dab9c6b..579ddbb51` and found
four medium defects to fix before publishing:

- Correct the SAMD GPIO emitter identity/header in the fixed registry.
- Exercise upper-word sentinels and high data/clock bits; narrowing must fail.
- Recheck all fingerprinted test/fixture and source inputs after native execution.
- Immediately debug-rerun the same native target after any quick-mode failure.

Width coverage is included in the #5044 test work. All four foundation review
fixes are implemented; the judge accepted the scoped Python follow-up review.
No final judge
certification has been granted.

## Additional native AVR finding

Source and Microchip documentation confirmed #5055: USART/SPI GPIO bits need
peripheral pin ownership release, prior-byte drain, and exact state restoration.
This is independent of #5052 selected-bit predicates and must be included in
the fixed gate registry/configurations before final certification.

## Current #5044 evidence

Normal and inverted production debug targets pass. Three actual production
mutations (old clock words, XOR to OR, u8 narrowing) fail during execution and
restore to the fixed source. Real stock ClearCore builds pass with normal and
forced-atomic mode, including production streaming on IO0/1 and IO2/3. The
forced-software-pin integration probe initially failed actual header declarations
and left unsafe NoMap use in the main object (#5056). It now builds after the
shared fallback fix. Both include orders and the real Uno forced mapped-pin
control also pass. The judge confirmed the
enabled SAME53 restored-three-store mutation (24 versus 16 stores) with a
matching object hash, alongside host 73 versus 49 and 149 versus 101 store
counts. Source-bound final records and complete gate certification remain
pending. Existing ESP8266
regression: all 41 physical captures pass and 512-byte throughput remains
5.36 Mbit/s at CPU80 and 6.66 Mbit/s at CPU160. No ClearCore physical timing
measurement is claimed.

## Coverage consolidation review

The judge found further #5031 work before classification data can be accepted:

- ARM's `alias` category includes independent implementations, delay code,
  dispatch and pin maps. Normalize roles; shared issue ownership is not aliasing.
- Reject self/cyclic aliases and require source-bound forwarding evidence.
- Normalize positive rows to a nonempty list of finding IDs; multiple findings
  share production files. Validate each ID against the fixed registry.
- Only 356 ARM files have individual research rows. Expand AVR/shared groups
  against the pinned baseline and retain unknowns for uncovered paths.
- Runtime pin adapters are dependencies of the runtime manager/channel routes,
  rather than blanket negatives. Retain method/guard distinctions and legacy
  independent copies as separate identities.

The classification file must contain all 2,986 baseline paths with hashes,
reasons and evidence. Current-source changes require separate review/execution
resolution; refreshing a hash or adding a passed status cannot resolve them.
The judge has not certified coverage or completion.

## Initialization first-edge follow-up (#5057)

Root observed a genuine debug RED after priming the existing clock latch HIGH:
production init leaves it HIGH, and the first byte samples seven bits rather
than eight (`.build/software-spi-init-clock-red.log`). The minimal fix drives
the clock pin's logical lo() before select release. Normal, inverted, streaming
and independent-port fixture variants cover the init guarantee without changing
the edge oracle. Both root debug targets pass, and the judge accepted the scoped
correctness evidence. This establishes logical initialization, not electrical
startup or physical timing.

## Forced-pin fallback follow-up (#5056)

Native tests execute the actual extracted forced-pin component, lookup-only
mapped controls and existing stub GPIO callbacks. Baseline execution faulted on
an actual null-pointer write in `lo()` (GDB registers and ELF source location
retained). Fixed debug tests pass. Restoring the old component and independently
removing generic null-port routing both fail during execution; restored fixed
source returns GREEN. Real ClearCore forced, normal and atomic builds pass.
Both include orders pass on ClearCore and Uno; Uno's retained final ELF proves
actual mapping-table initialization and byte-width register operations. The
fixed native runner fingerprints and executes the forced-pin target. The judge
accepted these scoped results; #5031 run-backed acceptance is still unfinished.
No physical measurements are claimed for ClearCore or Uno.

The divider-16 native test first exposed an independent missing
`fl::delaycycles<0>()` link symbol under #5032. Divider 8 isolates #5056 without
replacing the production delay implementation. The zero-delay defect remains
required work; the isolated test does not establish its correctness.

## First implementation PR validation

Root's broad native run passed 334 unit targets and 95 examples. All 31 focused
Python tests pass in normal and development mode, and full repository lint
passes. The actual audit runner executes all three native targets successfully
while overall completion remains RED: 2,986 baseline files, 2,987 current files,
32 required findings and 3,162 unresolved checks. The judge reviewed all 39
changed files in `579ddbb51..ace1b6455` with no blocking code findings. This is
validation for the first production fix batch, not final platform certification.

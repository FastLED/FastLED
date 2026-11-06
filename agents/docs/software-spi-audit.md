# Software SPI audit validation

Parent issue: https://github.com/FastLED/FastLED/issues/5024.

`bash spi-audit` is an opt-in completion gate. It enumerates every file in the
pinned baseline Git tree (2,986 `src` files) and every current tracked or
nonignored untracked file under `src`. Keyword queries do not determine that
universe. Generated WASM `node_modules` and `dist` directories are explicitly
reported exclusions. Other ignored source boundaries and changes to the pinned
root `.gitignore` policy fail the audit, so an ignore rule cannot hide a new
platform implementation.
Changed, missing, renamed and rerouted source remains unresolved until there is
review and run-backed evidence; changing a classification hash is insufficient.
The report accounts for all paths in `.build/software-spi-audit-report.json`.
Inventory validation uses actual pinned baseline bytes for changed/deleted
sources, reading their Git blobs lazily. Unchanged current bytes are reused only
after their Git blob identity matches. A Git object ID is not source content.
Malformed JSON records produce a structured RED report rather than bypassing
classification through a traceback.

The required entrypoint/configuration registry is independent of editable
findings. Mixed hardware/GPIO headers have method-level requirements, including
native AVR SPI/UART GPIO bits and SAMD SERCOM GPIO bits. Whole-file hardware
labels cannot remove those methods. Block/ISR helpers and other semantic query
misses are named separately. The six known clockless/peripheral/documentation
exclusions are bound to unchanged baseline sources.

Inventory positive rows use a nonempty, duplicate-free `findings` list of known
fixed-registry IDs. A legacy singular `finding` alone is insufficient; multiple
concerns may bind to one source without collapsing their evidence requirements.
Alias rows require their SHA-bound source to be a pure quoted-include forwarder
to the declared canonical path, allowing comments and optional pragma once.
Quoted includes resolve an existing local header before the project `src` root;
a shadowing local implementation cannot be skipped. Self aliases and cyclic
chains fail. An unrelated existing file,
an independent implementation body or an include plus its own writer is not a
valid alias. C++ delegating wrappers and conditional dispatch outside that narrow
contract require explicit dependency/review classification rather than an alias
exemption. Passing source classification still does not certify execution.
No macro guard is inferred to be unconditional: even a conventional-looking
`#ifndef` can depend on an already defined build macro. Guard wrappers, inactive
`#if 0`, unknown feature conditions, independent macro bodies and `#else`
branches require explicit dependency/unknown review rather than an alias
exemption. Logical LF/CRLF/CR newlines are normalized before ordered comment
recognition. Backslash and trigraph forms are conservatively unsupported, so
translation-phase splices cannot turn a commented-out include into proof.
Alias chains must terminate at an existing classified source. This narrow
repository include model does not certify arbitrary external include paths.

These classification checks are deliberately conservative. A valid include
forwarder and a valid plural finding list are positive controls for source
accounting only. They do not discharge production execution, target pin/config
matrices, restored-defect evidence or physical measurements. Full classification
coverage and run-backed resolution of changed sources remain required before
the overall audit can pass.

Reviewed baseline classifications use a versioned JSON document with exactly
`schema_version: 1`, `baseline` (the pinned baseline commit) and `files` keys.
`files` maps every pinned source path to `baseline_blob_oid`, `baseline_sha256`,
`disposition`, `role`, `findings`, `reason` and `source_evidence`. Evidence anchors
contain an integer `line` and the literal source-line `text`; aliases alone also
declare `canonical`. Roles describe negative, dependency, forwarding-alias,
dispatch, delay, implementation or pin-map responsibilities. Unknown dispositions
remain explicit errors. Unknown keys, Boolean version/line values, malformed
records and duplicate JSON keys are rejected; duplicate-key rejection also
applies to a supplied findings file.

Binary evidence may instead use exactly `{"kind":"bytes","offset":0,"hex":"0061736d"}`.
The offset must be a nonnegative integer (not Boolean), and hex must contain
nonempty lowercase byte pairs matching the exact baseline slice within bounds.
Whole-file Git blob and SHA-256 bindings remain mandatory. Byte anchors establish
review identity only; they do not make an executable WASM module a noncode asset,
waive context/reachability review or certify execution. Empty evidence remains
invalid, and new, changed or deleted binary files retain their RED obligations.

Use `bash spi-audit --classifications <reviewed-v1.json> --report <report.json>`
to load reviewed source bindings. The loader independently verifies the exact
pinned Git path set, blob identities, SHA-256 hashes and literal anchors against
baseline bytes. It validates aliases against those baseline bytes, while current
new, changed or deleted sources still produce RED errors. Hashes are never
refreshed to authorize changed code. Legacy path-keyed diagnostic rows retain
their existing checks; malformed versioned documents cannot fall back to them.
The reviewed baseline input is supplied at
`ci/fixtures/software_spi_audit/reviewed_baseline.json`, with all 2,986 pinned
source paths and all 51 mandatory finding associations. Use it explicitly:

```sh
bash spi-audit --classifications ci/fixtures/software_spi_audit/reviewed_baseline.json --report .build/software-spi-audit-report.json
```

Its adjacent README and compact provenance record exact manifest/checkpoint
identities and review-depth limits; initial inventories and scoped independent
reviews are not promoted into an every-line independent reread claim. This is
source accounting, never execution or target-configuration proof. New/changed/
deleted source obligations and all required target/configuration/mutation
checks remain RED until actual fixed runners establish their evidence. CLI
default behavior is unchanged.

The registry currently contains 51 distinct required findings. Native AVR
selected-bit predicates (#5052) and peripheral/GPIO handoff (#5055) are separate
requirements even though both bind to `fastspi_avr.h`. The latter covers all
five UBRR1/UBRR0/SPI0_CTRLA/SPSR/SPSR0 method variants, prior-byte shifter drain,
mux release, prior-state restoration and following-byte transfer. SM16716 and
the shared pixel writer are caller dependencies. Predicate-only evidence cannot
certify handoff: mux-aware production tests, restored-drain/release/restore
mutations and real target/configuration runners remain required and unresolved.

Forced software pins without a pin map (#5056) require safe Arduino GPIO
fallback, include-order coverage and avoidance of null register snapshots or
unproven whole-port streaming. Clock initialization (#5057) separately requires
logical LOW before select release and eight rising edges for the first byte
after a primed-HIGH latch, including normal/inverted and independent-port
models. Registry presence is bookkeeping; neither finding is certified by its
declaration or another finding's passing evidence.

Native KL26 single-bit output (#5059) is separate from generic KL26 pin-policy
work (#5039). Both the Teensy LC canonical header and independent legacy header
are required source identities. Native SPI0 pins 11/13 reach SM16716 start/header
bits through the actual dispatch and shared pixel writer; empty native `writeBit`
methods cannot be covered by generic-template success. Each implementation needs
all-bit/all-value and protocol evidence, with mux release, shifter drain, state
restoration and following-byte transfer, or a proved whole-protocol fallback.
The registry declaration does not satisfy those unresolved requirements.

ObjectFLED SPI pin restoration (#5065) requires actual init/show/wait/deinit
evidence through the direct AutoResearch `objectfledSpiSelfTest` RPC. Its
hardware-assisted GPIO serializer remaps selected pads from GPIO6 to GPIO1;
ordinary vendor `pinMode` does not restore that GPR26 selection. Tests must prove
safe DMA drain and restoration of bank, mux, pad, direction and latch state,
including failed initialization, timeout cleanup, unrelated bits and subsequent
ordinary GPIO/FastPin reuse. Restoring the old missing-bank-restore and omitted
pad/state paths must independently fail. The channel engine's `canHandle`
lookup is not claimed as an emitting producer. Concurrent whole-bank writer
coordination remains a separate unproved concern. Registry/source bindings do
not fix this defect or supply target execution or physical measurements.

Native ESP32 single-bit output (#5060) requires separate proof for the SPI proxy,
ESP-IDF implementation and independent Arduino implementation. Substituting a
byte of zeroes or ones emits eight clocks for a one-bit request; generic
software-SPI correctness does not certify those methods. Required cases include
single/quad routing, buffering/flush, bit-byte-bit transitions, consecutive frames,
SM16716's 25 bits per pixel and 50-bit header, real Xtensa/RISC-V/Arduino builds
and preservation of byte-only APA102 output. Each method needs its own restored
defect signal and a true single-bit API, proved whole-protocol fallback or
explicit rejection. These are unresolved evidence slots, not completed fixes.

The guarded public `ArdunioCoreSPIOutput` single-bit method (#5061) separately
requires proof for its empty `writeBit` body and `FLAG_START_BIT` pixel caller.
The D51 guard/include inputs are bound dependencies. No current in-tree selector
has been established; that limitation does not waive a public callable positive.
Real Arduino SDK instantiation, selected-bit payloads, start-bit pixels,
bit-byte-bit transitions, peripheral drain/mux ownership and state restoration
remain unresolved. A true single-bit implementation or explicit compile-time
rejection must be verified, and restoring the empty body must fail its test.

Parallel SPI target backend selection (#5062) requires real vendor register/bank
identity for the platform macros, all six independent blocking transmitters and
the shared ISR engine. Classic ESP32, C3/C2, unsupported non-ESP rejection and
explicit custom backends need separate evidence. Restoring unconditional C3/C2
addresses must fail; host simulation cannot certify hardware register addresses.

Parallel SPI pin mapping (#5063) separately requires all six blocking mapping
methods and ISR1/2/4 pin-index and mask variants. Invalid indices, integer-width
boundaries, duplicate pins, clock collisions and failed reconfiguration must
preserve valid state without GPIO writes or undefined shifts. ISR8/16/32 raw
masks have a separate contract; 32 independent data pins plus clock cannot
silently alias one bank. Each independent old mapping defect needs a restored
mutation failure. Both findings concern public callable implementations;
`ParallelDevice` currently rejects its begin/write routes, so no active producer
through that wrapper is claimed. Registry bindings do not discharge these tests.

Native Teensy4 pending bits (#5064) require independent proof for the canonical
and legacy `Teensy4HardwareSPIOutput` bodies. Their public bit API buffers partial
groups while completion methods currently fail to flush them, and raw repeat
writes bypass pending state. Partial counts 1–7, 8, 9 and mixed byte/word/repeat/
pixel operations must preserve exact order and drain before select release;
completed frames must not leak pending bits into a later transaction. Real
LPSPI frame-width/ownership evidence and separate restored-no-flush/bypass
mutations remain required. The current proxy exposes no `writeBit`, so no active
SM16716 producer through it is claimed. A direct backend protocol case remains
relevant; public callable correctness is not waived by an absent selector.
Registry presence is unresolved bookkeeping, not a protocol fix or target proof.

MY9221 DDR timing (#5066) is a separate required implementation. Its built-in
and legacy selectors reach direct FastPin data writes/clock toggles and four DI
latch pulses, bypassing GenericSoftwareSPIOutput. Required evidence must cover
both-edge setup/hold and clock minimum/maximum duration, every latch pulse,
cascade-dependent post-stop timing, chain padding/order and repeated frames on
actual supported vendor pins/CPU/compiler configurations. Restoring omitted
phase/latch/stop waits must fail an independent timing oracle; the existing
bit-order golden model alone cannot establish timing. This source-confirmed
missing guarantee is not a measured short pulse or electrical-compliance claim.
The intentionally unused SPI_SPEED compatibility parameter is not a separate
defect, and ordinary single-edge SPI must not replace this DDR protocol.

Run `UV_NO_SYNC=1 bash test spi_legacy_golden --debug --clean` for the public
MY9221 pin/time observation cases. They cover partial/repeated BGR chains,
five-chip stop timing, final-HIGH parking, allocation failure/retry and prior
interrupt states. Virtual nanoseconds establish requested scheduling only.
Removing the stop wait must fail the five-chip case: the final 240ns low wait
already satisfies the one-chip stop bound and is insufficient as that control.

ESP8266 GCC10.3 ignores the tested template-member section attributes; even an
inline specialized facade can leave literals in flash. Inspect the actual ELF
for the ordinary kernel, selected FastPin callbacks, their literal pools and
all masked-region callees. An IRAM annotation or successful build is not proof.
ESP32 GCC14.2 also placed the typed emitter in flash in an actual C6 build.
Its ordinary kernel and callbacks need the same ELF inspection on RISC-V and
Xtensa SDK configurations. User-defined `FASTLED_UNUSABLE_PIN_MASK` values must
remain C++ constant expressions: include-only and valid-pin producers must
compile with typed and named-constexpr masks, while selecting an excluded pin
must fail its validity assertion. Preserve mandatory SDK/platform exclusions in
those probes; an unrelated USB-pin assertion is not the expected rejection.
Separately check the real vendor interrupt backend: changing AVR SREG restore
to unconditional `sei` must be rejected even though it compiles. The host
specialized guard cannot establish that vendor behavior. Retained manual runs
remain feasibility evidence until a managed adapter binds the actual inputs,
commands, configuration, artifacts and independent rejection controls; they
must not clear the seven MY9221 requirement groups through imported receipts.

Focused validation commands:

- `uv run pytest ci/tests/test_software_spi_audit.py -q` tests gate rejection
  behavior. Passing these tests is not completion evidence for the audit.
- `bash test generic_software_spi --clean` executes the actual production generic writer
  against GPIO observation models: 8/16/32-bit registers, same/different ports,
  scalar/word/bulk/repeat/pixel paths, callbacks/start bits, streaming store
  counts, atomic interference and phase-delay/select/release observations.
- `bash test generic_software_spi_inverted --debug --clean` checks the #5044 inverted
  bulk-clock regression. Its original five-byte reproducer failed with 41
  sampled bits instead of 40 before the fix. Additional cases cover all 256
  byte values and all eight scalar bit positions, independently inverted data
  and clock, legacy/streaming paths, and independent-port atomic writes.
- `UV_NO_SYNC=1 bash spi-audit --run-native` executes the three fixed native
  targets with `--clean`, retaining a failed quick run and its immediate debug
  rerun. A cached test-result summary is not fresh host execution; ordinary
  compiler zccache hits are permitted. Exit codes, source/test fingerprints and
  exact log hashes are recorded. A manifest cannot supply another command or a
  success receipt.
  Fingerprints include tracked/nonignored CI and test code plus root launcher
  and build definitions; ignored generated dependencies are excluded. Log hashes
  cover the same encoded bytes saved to disk, including on Windows.
- The fixed native inventory also executes `bash test forced_software_pin --debug --clean`
  and fingerprints `tests/fl/system/forced_software_pin.cpp`. This is host
  fallback behavior evidence; it does not certify vendor include-order builds.

The same `--run-native` path also invokes the fixed isolated #5057 runner. It
builds and executes the exact production init testcase in four stages: baseline,
missing-clock-LOW mutation, LOW-after-release mutation, and exact restoration.
The JSON report's `init_clock_low_proof` preserves its entire internally produced
typed provenance: retained workspace, source/input identities, tool identities,
Meson options/configuration/environment digests, executed compiler/link commands,
logs/binary/PCH/dependency hashes, Gitlink identity and host32-fixture4 scope.
Per-stage raw generated configuration snapshots remain in the retained attempt.
The source checkout is retained under the owned `~/.clud/worktrees` root and
stage artifacts under `.build/spi-init-proof`; exact source restoration is checked
on every exit. Failure, timeout or restoration errors keep `complete=false` and
report the retained attempt path. No external provenance document is loaded as
proof, and this host32 subproof clears no broader required configuration slot.
A previous accepted isolated run is dated evidence, not a substitute for the
fresh CLI execution. Target/vendor/other-width/codegen and physical obligations
remain unresolved in the overall audit.

The `spi-audit` wrapper anchors execution to its own repository directory and
project, so invocation from another working directory cannot select a foreign
`ci.software_spi_audit` module.

The foundation deliberately reports RED for all unwired positive findings.
Native GPIO models establish production-template semantics only. They do not
establish actual vendor pin maps, real board compilation, target instruction
cost, interrupt latency or physical throughput. Target/configuration and
restored-defect runners must be wired before an individual finding can pass;
ultimately a valid real positive control and restored-old-path failure are
required. No externally supplied JSON claim currently certifies execution.

Keep this opt-in gate separate from routine test success while findings remain
open. Do not mark #5031 complete merely because the gate's unit tests pass or
because required source/coverage declarations are present.

## SAME53 polarity verification (#5044)

The generic same-port words derive the clock bit from FastPin hival/loval,
preserving every other data-word bit. Streaming toggles the correctly encoded
logical-low clock bit with XOR; OR-high is valid only for normal polarity.
GPIO observation models use upper data/clock bits on 16/32-bit ports and check
all unrelated bits, including high-bit sentinels, so narrowing a production
word to eight bits cannot pass as width coverage.

The audit-only AutoResearch branch instantiates the actual software-SPI writer
on logical pins 0/1 and 2/3. Its setup retains `software_spi_probe_*` function
addresses through volatile RAM accesses, forcing their dependencies to survive
final linking. It never calls the GPIO or delay probes; loop is empty. This
does not drive pins or establish electrical timing. The branch is absent from
normal AutoResearch builds. Use the existing compile wrapper, with a 15-minute
or longer build deadline:

```sh
FBUILD_DEV_MODE=1 bash compile clearcore --examples AutoResearch --no-filter --defines FL_SOFTWARE_SPI_COMPILE_PROBE=1 --build-info .build/software-spi-clearcore-5044-build-info.json
FBUILD_DEV_MODE=1 bash compile clearcore --examples AutoResearch --no-filter --defines FL_SOFTWARE_SPI_COMPILE_PROBE=1,FAST_SPI_INTERRUPTS_WRITE_PINS=1 --build-info .build/software-spi-clearcore-5044-atomic-build-info.json
```

Inspect retained symbols and instructions in the final ELF from each build
with that toolchain's objdump. An object can contain unresolved delay calls
that section garbage collection hides during linking; an object-only success
is insufficient. Check scalar/atomic reversed OUTCLR (logical high) and OUTSET (logical
low), bulk/repeat/pixel full-OUT words, native pin setup, register width, and
minimum delays. Save each build's object/disassembly before the next build.
ClearCore IO0-IO3 permit exclusive-port streaming with all conservative padding
retained and `kMinWriteCycles` zero. Forced interrupt-safe output keeps atomic
writes; `FASTLED_FORCE_SOFTWARE_PINS` selects the unspecialized software-pin
policy, and unsupported pins remain invalid. The runtime length/span and pixel
count probe parameters include empty output without substituting a copied
writer. Target code generation and the connector's physical timing are not
established by host GPIO models. Measurements are required before crediting
GPIO latency or skipping conservative padding; no physical speed is claimed.

Required restored-defect mutations, with restoration and GREEN after each:

- Restore the baseline OR-high/AND-low word construction in all three generic
  bulk entrypoints; the original inverted five-byte regression must fail.
- Restore `low | clock.mask()` in the streaming helper alone; inverted-clock
  streaming cases must fail while the normal-polarity control still passes.
- Narrow production composed/stored port words to `u8`; upper-bit 16/32-bit
  models and unrelated-bit preservation assertions must fail.

These commands and mutations are verification procedures, not passing results.

## Shared default timing (#5032)

`BitbangTiming` provides a SPI-local inline backend; it does not change the
global clockless `fl::delaycycles` API. Nonpositive requests emit no padding.
Phase arithmetic and template arguments use `i64`, preserving the full `u32`
SPI divider range even when AVR's unrelated `cycle_t` is signed 16-bit.
Uncalibrated policies request the full rounded-up half-period or 35 ns floor,
whichever is larger, without subtracting unproved call or GPIO costs.
ESP8266 explicitly retains its existing calibrated policy and GPIO16 exception.

AVR uses compiler cycle-delay builtins with bounded arguments. Cortex-M and
the nine reviewed ESP32 application-core targets use retained instruction
padding and bounded counted loops. Two additions per requested cycle use a
documented maximum issue-width bound; they do not assume NOP latency or credit
dependent-instruction stalls. This intentionally overpads single-issue cores.
ESP32 does not poll per-core counters: migration and SDK counter rewrites must
not prematurely end a wait. Its guarantee assumes the actual CPU clock never
exceeds configured `F_CPU`; slower clocks and preemption add time. Pico SDK
minimum-cycle waits are used where available. CI13xx and the ESP8266 forced
fallback retain their startup-dependent cycle-counter contracts. Stub/WASM
timing establishes logical behavior only. Unrecognized hardware targets require
a reviewed backend for positive waits; zero remains a no-op.

The retained leaf probes cover counts 0, 1, 2, 3, 15, 16, 17, 55, 57, 40,000
and 2^31. Inspect actual target code for tiny inline padding, correct loop
initialization/backedges/tails, bounded emitted size, and absence of old
out-of-line calls or ESP32 counter polling. Literal native phase budgets
include odd dividers, 80,000 and `u32` maximum, but those observations alone
do not establish target timing. Restore the old out-of-line backend and the
old assumed-overhead accounting independently; each must fail its intended
linkage or production-argument check, then return GREEN after restoration.

CPU padding, vendor compilation and retained code do not prove peripheral
write completion, electrical pulse widths or exact requested throughput.
Physical claims require separate measurements. The #5032 acceptance matrix,
including CI13xx target compilation, remains required before closing it.


ESP32 forced FastPin selection (#5067) remains independently unresolved. Native
explicit specializations currently override the forced primary template, even
with `FASTLED_NO_PINMAP`; #5056 fallback safety does not prove this selection.
Acceptance requires actual SDK selected-type and fallback-call proof for Xtensa
and RISC-V, forced on/off and pin-map configurations, generic SPI and MY9221
producers, and restoration of the unguarded specializations must fail the
selection oracle. Macro presence and a native IRAM ELF cannot satisfy this
requirement; fallback cache placement and phase timing remain separate.


The managed MY9221 host adapter uses the actual `fl_channels_spi_legacy_golden`
translation unit and its six public-controller pin/time observation cases.
It runs an isolated baseline, five independent production wait removals, and an
exact restored build. The five-chip stop case must fail its literal strict
250ns bound when the cascade wait is removed. Only the internally fixed target,
cases and mutations can be executed; `--run-native` serializes the resulting
full typed provenance. No imported JSON creates or authenticates this proof.

A successful actual adapter run establishes only the observed host92/93 requested schedule and
public-controller behavior, with modeled pin cost and timing-policy hooks. It
cannot certify the real wait backend, vendor interrupt state, IRAM/SRAM placement,
physical phase bounds or other pin/CPU/configuration matrices. All seven full
MY9221 requirement groups stay RED. The existing init adapter remains a separate
host32 subproof. Both retain immutable input/tool/environment/build-recipe
identities, actual executed commands, binaries/PCH/dependencies/logs and exact
source restoration; failed attempts keep their owned isolated locations.

The opt-in actual MY adapter acceptance is:

```sh
FASTLED_RUN_MY9221_PROOF=1 UV_NO_SYNC=1 uv run --no-sync pytest ci/tests/test_software_spi_audit.py -q -k my9221_managed_host_schedule_real_acceptance
```

A parser/schema unit test or old manual mutation record is not an actual fresh
adapter acceptance run. Actual selected-case diagnostic captures remain required
when the framework output contract changes.

Seven additional forced-selection findings (#5068–#5074) remain unresolved across ESP8266, nRF51, nRF52 variants, GIGA, MGM240, modern LPC and legacy LPC. Native explicit specializations can override the forced primary; selected-type and actual fallback-route controls are required for every supported variant. Existing optimization matrices, including nRF51 #5047, remain mandatory. Source bindings do not certify execution, cache safety or timing.


The separate opt-in RP2040 saved-PRIMASK adapter uses the same fixed local
`MY9221Controller<2, 3, RGB>` public producer and frozen vendor build profile as
the placement adapter. Run it explicitly with
`FASTLED_RUN_MY9221_RP2040_IRQ_PROOF=1 UV_NO_SYNC=1 uv run --no-sync pytest ci/tests/test_software_spi_audit.py -q -k my9221_rp2040_irq_managed_real_three_stage_acceptance`.
It builds baseline, the single `restore_interrupts(mSaved)` to
`restore_interrupts(0)` control, and exact restoration. Its internally generated
record retains source/staged-header bytes, tools, SDK, per-build serving-engine
identity, planned compile database, ELF/map and typed finite IRQ analysis.
Both initial PRIMASK values must reach valid normal returns with restored SP/LR
and terminal pin parking; the mutant requires matching concrete entry-1/exit-0
outcomes and counterexamples. This is a bounded static property, not observed
hardware IRQ behavior, loop termination, electrical timing or complete executed
compiler arguments. It does not run through `--run-native` and does not resolve
any of the seven full MY9221 configuration groups. Actual managed qualification
requires a fresh successful three-stage run and independent artifact review.


The fixed MY9221 host baseline/restoration selection also includes ordinary and
legacy global GRB dispatch against literal partial-chain payloads, plus public
`showLeds` scaling/correction and deterministic binary-dither literal frames.
The latter exercises direct controller dispatch; global `FastLED.show` may
suppress dithering at low measured refresh rates. The real allocation-failure
case checks nonzero retry payloads, an updated frame and empty-frame silence.
All five prescribed wait mutants retain their original designated assertion
cases. These additional cases extend functional/requested-schedule scope only;
physical phase limits and the seven full target configuration groups remain
unresolved until independently qualified.


ESP32 forced-pin selection (#5067) is distinct from MY9221 qualification
(#5066). A defined `FASTLED_FORCE_SOFTWARE_PINS`, including value0, selects the
software primary instead of native explicit specializations. Ordinary FastLED
and generic software-SPI inclusion remains supported. The ESP32 MY9221 forced
provider currently has no qualified cache-safe operation closure, so invoking
that controller produces a dependent unsupported-provider diagnostic; inclusion
alone does not fail. This explicit unresolved prerequisite does not establish
MY9221 forced timing, SRAM closure, or any full target configuration evidence.

ESP32 forced pins deterministically use the existing `fl::pinMode`,
`fl::digitalWrite` and `fl::digitalRead` provider without optional register
mapping. FastLED intentionally excludes Arduino mapper macros from ESP32
library headers; choosing their availability per translation unit would make
Arduino-first sketches and FastLED-first library sources disagree. This policy
does not define or override `FASTLED_NO_PINMAP`, and other platforms retain
their mapped control. It establishes neither a cache-safe MY9221 provider nor
physical timing guarantees.

ESP32 hardware SPI, PARLIO and clockless RMT/SPI consumers validate output pins
against a shared bounded SDK output-mask and user-exclusion policy. That policy
is independent of the selected forced software GPIO provider; native FastPin
declarations use the same policy. Negative and >=64 pin indices are invalid
without forming an out-of-range shift. This preserves hardware pin rejection
without inventing a `validpin` method on forced pins or enabling unsupported
MY9221 forced emission.

The ESP32 SDK output-mask defaults and bounded user-exclusion predicate live in
`platforms/esp/32/core/output_pin_policy.h`, which does not include a FastPin or
GPIO-provider implementation. Native declarations, hardware consumers and
forced output operations share that policy. Forced `setOutput`, hi/lo, toggle,
set and fastset (including non-null port overloads) reject invalid output pins
at instantiation; `setInput` remains separate so input-only pins can be used.
This is output legality, not a forced MY9221 cache/timing qualification. The
new policy and top-level dispatcher remain source-accounting obligations; the pinned
2,986-path baseline and its hashes are not refreshed.

The native ESP32 `PinMode::Output` mapping enables push-pull drive and input
readback (`GPIO_MODE_INPUT_OUTPUT`). SDK `gpio_get_level` requires input to be
enabled; forced toggle uses the existing read/invert/write route. This fixes the
configuration prerequisite without introducing shadow output state. Source and
SDK mode checks are distinct from a physical pad-level toggle measurement or
MY9221 timing/cache qualification.

The separate C6 output-mode adapter is explicitly opt-in:
`FASTLED_RUN_ESP32_C6_MODE_PROOF=1 UV_NO_SYNC=1 uv run --no-sync pytest ci/tests/test_software_spi_audit.py -q -k esp32_c6_mode_real_three_stage_acceptance`.
It uses the fixed forced GPIO2/3 Generic producer and a FastLED-first companion
translation unit, global FORCE=1 without NO_PINMAP, Arduino3.3.5/IDF5.5.1,
GCC14.2.0_20260121, and the fixed 160MHz rv32imac C6 profile. Three isolated
builds retain the current mapping, the exact
old `GPIO_MODE_OUTPUT` assignment, and exact restoration. A bounded encoded
RISC-V contract binds the actual producer calls and `gpio_config_t::mode` store
to compiled SDK/public-enum exports. Both PWM lookup outcomes are considered
under an explicit normal-callee saved-register ABI assumption. The old mapping
must compile and produce the intended mode mismatch. Unknown instruction or
binding forms reject; raw caller-provided records cannot certify this property.
The retained compile database describes planned arguments, not complete executed
argv. This static configuration property clears no full #5067 or #5066 group
and makes no runtime GPIO, physical-toggle, IRQ, cache or timing claim.
Remaining work includes qualified runtime readback/toggle behavior, the other
selection/configuration and invalid-pin controls, target-specific code/cache
closure and the independent MY9221 min/max timing and memory requirements.
The adapter does not auto-run from `--run-native`; failed attempts retain their
isolated workspace, logs and restoration diagnostics without publishing proof.

The first managed C6 attempt (`rfo516cb`) remains unaccepted: its baseline
restored a byte-identical ELF through fbuild's native warm link cache, which
does not restore an ordinary map. The inherited RP map precondition stopped
that attempt before classification or the old-mode control. C6's authority is
the retained ELF sections/PT_LOAD/symbol bytes, compiled exports and frozen
SDK ROM linker definition; it does not consume an ordinary map. C6 now records
missing/present/ambiguous ordinary maps as diagnostics, retaining each present
candidate under a distinct name with its hash and freshness explicitly
unproven. It never substitutes a bootstrap map or skips semantic assessment.
RP placement and saved-PRIMASK profiles continue to require their unique map.

The subsequent `95z1rpgj` attempt also remains unaccepted. Its ELF exposed the
pinned C6 SDK's intentional `.flash_rodata_dummy (NOLOAD)` reservation, which
shares code's virtual range and is padded before real rodata. The extractor
qualifies only this exact script-bound geometry and its zero padding/coherent
data LOAD; other overlapping sections or LOADs still reject. Both SDK linker
scripts and layout diagnostics are retained. This is conditional ELF analysis:
it does not establish firmware BIN segment authority, runtime address mapping
or physical execution, and no generic NOBITS/LOAD exemption is provided.

Attempt `071zeek4` remains unaccepted too: its real old-assignment control
compiled successfully, but GCC emitted a distinct 142-byte register-reuse
layout instead of the 138-byte layout modeled by the initial synthetic cell
test. Both reviewed layouts now retain exact branch/register/SP contracts and
decoded call targets. The stored value comes from the decoded `a5` immediate
on the selected public-Output path, never from a stage or grammar label. The
actual old capture and reciprocal synthetic opcode test qualify parser
behavior only; a fresh managed current/control/restored attempt is required.

#5086 protects the selected Teensy4 forced mapped toggle path: writing a sampled
input register is not a portable output-toggle contract. Actual separate
input/output behavior, Teensy40/41 SDK codegen across banks, AVR PINx and NoPinMap
controls, and restored-defect rejection remain required. ESP8266 reachability
still depends on #5068; no universal atomicity or electrical timing is claimed.

#5087 protects SAMD21/D51 FORCE plus NO_PINMAP readback: Output clears PINCFG
while digitalRead samples IN, which requires INEN. Effective physical port/bit
qualification is separate from Arduino numbering and native FastPin work.
Input/pull-up preservation, realistic sampler behavior and old-path restoration
remain required; no exact disabled-input return value or physical measurement
is inferred. These two rows and baseline associations are bookkeeping only.

#5089 protects Apollo3 forced default-NoPinMap output readback. Configured
Arduino_Apollo3 core2.2.0 binds an upstream mbed gitlink containing HAL2.4.2;
that is not an actually compiled release-archive/header qualification. Output
disables input sampling, while INPUT_READ requests RDA/RDB rather than WTA/WTB
output-latch state. Require actual provider/release/mapping controls, both
initial latch states, two toggles/strobe, unrelated-bit preservation and the
restored-defect behavioral control. Zeroed eIntDir means LO2HI, not INTDIR_NONE;
no exact disabled-input result or electrical claim is inferred. Native SPI
#5030 and missing-map safety #5056 remain separate obligations.

#5070 additionally retains a latent forced-plus-explicit-NoPinMap readback
requirement after its native-variant selector defect is fixed. The actual
runtime file is `platforms/arm/nrf52/pin_nrf52.hpp`. Adafruit1.6.1 is admitted by
the configured `^1.10601.0` package range, not proof of the resolved package or
legacy Mbed HAL. Qualify latch/readback behavior and restoration independently
while preserving every original variant/native optimization requirement.
Surveyed SPI manager/channel and non-ESP generic writers use explicit hi/lo;
these dependency bindings establish no new current SPI readback positive.
This metadata snapshot does not refresh the accepted C6 receipt's historical
43b1 input qualification or close any runtime, target or physical requirement.

The classic Xtensa helper currently classifies conditional Output-only ELF
slices. It has no managed factory or evidence-admission path. Authentic
current, old-assignment and restored captures from snapshot
`84405dc080e7c8e0b6e60c3ca8d55f157e9fa456` contain configured modes 3, 2 and 3;
the old capture yields four mode-mismatch counterexamples. These captures
remain parser data, not an accepted property receipt. The conclusion assumes
arrival at the two bound CALL8 sites, valid nonaliasing producer-prefix and
volatile-sink storage, normal windowed-ABI returns, and no uncontracted writes
to the caller's config frame. The bound memset zeros exactly 16 bytes at
SP+16 through SP+31; it does not initialize the whole 24-byte config. Other
modes, BIN/runtime mapping, GPIO behavior, timing, cache safety and full
finding groups remain unqualified.

An adversarial opcode-bit-7 mutation exposed a real false acceptance in the
MOVI.N family check. Root observed the regression fail, then verified the
repair with 38 focused cases and a 65,536-word mode-cell probe admitting only
the encoded values 2 and 3. This validates that cell, not arbitrary Xtensa
instructions or production execution. The next steps are an independently
bound raw ELF/SDK factory and a closed managed current/control/restored build
proof. Historical C6 receipts remain unchanged.

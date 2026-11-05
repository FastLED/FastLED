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

The required entrypoint/configuration registry is independent of editable
findings. Mixed hardware/GPIO headers have method-level requirements, including
native AVR SPI/UART GPIO bits and SAMD SERCOM GPIO bits. Whole-file hardware
labels cannot remove those methods. Block/ISR helpers and other semantic query
misses are named separately. The six known clockless/peripheral/documentation
exclusions are bound to unchanged baseline sources.

The registry currently contains 32 distinct required findings. Native AVR
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

Focused validation commands:

- `uv run pytest ci/tests/test_software_spi_audit.py -q` tests gate rejection
  behavior. Passing these tests is not completion evidence for the audit.
- `bash test generic_software_spi` executes the actual production generic writer
  against GPIO observation models: 8/16/32-bit registers, same/different ports,
  scalar/word/bulk/repeat/pixel paths, callbacks/start bits, streaming store
  counts, atomic interference and phase-delay/select/release observations.
- `bash test generic_software_spi_inverted --debug` checks the #5044 inverted
  bulk-clock regression. Its original five-byte reproducer failed with 41
  sampled bits instead of 40 before the fix. Additional cases cover all 256
  byte values and all eight scalar bit positions, independently inverted data
  and clock, legacy/streaming paths, and independent-port atomic writes.
- `bash spi-audit --run-native` executes those fixed native targets and records
  their actual exit codes, source/test input fingerprint and output log hashes.
  A manifest cannot supply another command or a success receipt.
- The fixed native inventory also executes `bash test forced_software_pin --debug`
  and fingerprints `tests/fl/system/forced_software_pin.cpp`. This is host
  fallback behavior evidence; it does not certify vendor include-order builds.

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
on logical pins 0/1 and 2/3 and retains `software_spi_probe_*` functions in the sketch
object for target disassembly. Its setup/loop are empty: compilation does not
drive pins or establish electrical timing. The branch is absent from normal
AutoResearch builds. Use the existing compile wrapper, with a 15-minute or
longer build deadline:

```sh
bash compile clearcore --examples AutoResearch --no-filter --defines FL_SOFTWARE_SPI_COMPILE_PROBE=1 --build-info .build/software-spi-clearcore-5044-build-info.json
bash compile clearcore --examples AutoResearch --no-filter --defines FL_SOFTWARE_SPI_COMPILE_PROBE=1,FAST_SPI_INTERRUPTS_WRITE_PINS=1 --build-info .build/software-spi-clearcore-5044-atomic-build-info.json
```

Inspect the retained sketch object from each build with that toolchain's
objdump. Check scalar/atomic reversed OUTCLR (logical high) and OUTSET (logical
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

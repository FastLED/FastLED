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

The registry currently contains 34 distinct required findings. Native AVR
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

Native ESP32 single-bit output (#5060) requires separate proof for the SPI proxy,
ESP-IDF implementation and independent Arduino implementation. Substituting a
byte of zeroes or ones emits eight clocks for a one-bit request; generic
software-SPI correctness does not certify those methods. Required cases include
single/quad routing, buffering/flush, bit-byte-bit transitions, consecutive frames,
SM16716's 25 bits per pixel and 50-bit header, real Xtensa/RISC-V/Arduino builds
and preservation of byte-only APA102 output. Each method needs its own restored
defect signal and a true single-bit API, proved whole-protocol fallback or
explicit rejection. These are unresolved evidence slots, not completed fixes.

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
  Fingerprints include tracked/nonignored CI and test code plus root launcher
  and build definitions; ignored generated dependencies are excluded. Log hashes
  cover the same encoded bytes saved to disk, including on Windows.
- The fixed native inventory also executes `bash test forced_software_pin --debug`
  and fingerprints `tests/fl/system/forced_software_pin.cpp`. This is host
  fallback behavior evidence; it does not certify vendor include-order builds.

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

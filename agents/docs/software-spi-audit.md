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

Focused validation commands:

- `uv run pytest ci/tests/test_software_spi_audit.py -q` tests gate rejection
  behavior. Passing these tests is not completion evidence for the audit.
- `bash test generic_software_spi` executes the actual production generic writer
  against GPIO observation models: 8/16/32-bit registers, same/different ports,
  scalar/word/bulk/repeat/pixel paths, callbacks/start bits, streaming store
  counts, atomic interference and phase-delay/select/release observations.
- `bash test generic_software_spi_inverted --debug` isolates the existing
  inverted bulk-clock defect in #5044. This is a real failing production path,
  not a skipped or simulated passing vendor result.
- `bash spi-audit --run-native` executes those fixed native targets and records
  their actual exit codes, source/test input fingerprint and output log hashes.
  A manifest cannot supply another command or a success receipt.

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

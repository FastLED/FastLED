# ESP memory reductions: #3179 and #4733

- [x] Inspect live issues, production paths, and attached chips.
- [x] Confirm ESP32-S3 /dev/ttyACM1, ESP32-C6 /dev/ttyACM0, ESP8266 /dev/ttyUSB0 through chip detection.
- [x] Capture focused failing tests before implementation.
- [x] Restore public typed channel addition that registers only its selected driver; preserve runtime registration compatibility.
- [x] Implement bounded true-SPI DMA staging, ISR refill, async adapter progression, and source lifetime protection.
- [x] Add durable AutoResearch memory/output/throughput probes using the public APIs.
- [x] Measure matched hardware evidence, including full-frame framing, memory accounting, repeated frames, and teardown.
- [x] Run targeted and full native tests, affected Python tests, board checks, lint, and code review.
- [ ] Create PR with native-coverage labels at creation; address reviews and required checks; observe merge.

Scope: true clocked APA102/SK9822 SPI (#3179), not the separate WS2812 clockless-over-SPI engine. Buffer savings and total heap savings must be reported separately, including descriptors and SDK queue allocations. No new workflow files or one-off sketches.

Memory evidence and pre-review hardware regressions are complete. GPIO AutoResearch passes on S3 (GPIO 1→2) and C6 (GPIO 0→1); all four S3 explicit-host/registration-order controls passed on the baseline and final normal firmware; existing `--spi` AutoResearch passed. The matched pre-streaming S3 baseline and measured candidate capture all 4132 bytes of 1000-pixel APA102 and SK9822 frames across 20 changing frames, with zero mismatches and stable heap. Scoped transfer DMA/internal allocation peak is 4576 → 1044 bytes (4.38× smaller), with 592 bytes more retained state and 2940 bytes estimated steady-transfer headroom gained from allocation deltas. Final median show+wait is 9374.5 µs APA102 / 9364 µs SK9822, versus baseline 10119.5 / 10089.5 µs. Application image bytes are 1918064 → 1920416 (+2352); this is not an attributed-flash or Blink comparison.

Matched typed/runtime binding uses the same explicitly selected backend and reports actual names. S3 RMT registration is 12016 → 2840 bytes (6 → 1 drivers), saving 9180 bytes steady SRAM. C6 PARLIO registration is 11540 → 916 bytes (5 → 1 drivers), saving 10636 bytes steady SRAM. All four runs remain flat from frame three through frame twenty. Full evidence and limitations are in `docs/ESP_MEMORY_3179_4733.md` and the paired raw JSON. ELFs/maps are retained under `.build/memory-evidence/`. Measured SPI ELF SHA-256 is `7c38f75157d32d2e52da251df28a85b75249ba1744db26b4bc1c8187d4425b01`.

Focused native debug tests passed after their initial RED cases. Full native debug validation passed 329/329 unit binaries and 95/95 examples. Final affected Python checks passed 291 tests. Primary pre-push review is clean after callback publication, failed-submission diagnostics, macro naming, and benchmark boundary annotations were corrected. The Rust C++ lint hit its 300-second tool timeout twice under host load; `bash lint --cpp --no-rust` then passed. Other full-lint lanes passed. Final SPI rerun initially hit a fbuild daemon connection failure; the daemon was recovered and the final-source retry passed both chipsets and the host memory gate. PR #5002 is open with ci-full and ci-platforms-unit-test applied at creation. Hosted Linux CI exposed eight existing destructor-shim compile failures; the SDK shim was updated without changing its ownership assertions, and all eight cases then passed locally. Hosted checks/review and observed merge remain open.

Hardware evidence predates the review-only helper extraction and warning/help/CLI/provenance fixes. No final-binary size or final-source timing claim is made for those edits; focused native/SDK-shim tests and hosted board builds validate the extracted definitions.

Review follow-up: all six initial CodeRabbit findings addressed (unsupported fixed-workload options, exact release ELF, mode-specific RPC help, erased-SPI warning, platform markers, helper definition layout). Additional ignored pin/lane/pattern options now reject explicit values. Seventy focused Python cases and the focused native SPI adapter debug test pass; targeted lint passes. Full initial board matrix passed; final head uses persistent native coverage plus explicit ESP32 board dispatches because ci-full is consumed and late labels are forbidden.

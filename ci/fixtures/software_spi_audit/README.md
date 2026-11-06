# Software SPI reviewed baseline classifications

`reviewed_baseline.json` is the exact reviewed version 1 source-classification
input for baseline `94dab9c6b720ea277ec9177c93129c7dc646c5c3`: all 2,986 pinned
`src` paths, with associations for all 48 mandatory findings. Its SHA-256 is
`842d083a8cb4ac8ea420229130d7bd16a56f180ef601a0b686424495030a1ff1`.

From the repository root:

```sh
bash spi-audit --classifications ci/fixtures/software_spi_audit/reviewed_baseline.json --report .build/software-spi-audit-report.json
```

The existing loader independently verifies the pinned Git universe, blob and
SHA-256 identities, literal line/byte anchors, row schema and strict forwarding
aliases. The JSON remains readable and uncompressed; no sharded-loader contract
or default CLI behavior is introduced.

`source_review_provenance.json` records source-review checkpoint digests,
finding identities and actual review-depth limits. Initial inventory labels do
not imply that every line received an independent full-body reread. Later
reviews and targeted selection ranges retain their narrower attribution;
checkpoint digests are historical source-review identities, not executable
receipts. Each manifest row retains its source-specific reason and anchors.

This is source accounting only. A zero semantic-review queue does not establish
production correctness, target configuration coverage or physical timing.
Current new/changed/deleted sources and required runtime/codegen/restored-defect
checks remain RED until their actual fixed runners establish the required
proof. Do not refresh baseline hashes, remove rows or change roles to authorize
an implementation. No runtime receipt is loaded from this directory, and the
overall audit remains incomplete.

Seven additional forced-selection findings (#5068–#5074) remain unresolved across ESP8266, nRF51, nRF52 variants, GIGA, MGM240, modern LPC and legacy LPC. Native explicit specializations can override the forced primary; selected-type and actual fallback-route controls are required for every supported variant. Existing optimization matrices, including nRF51 #5047, remain mandatory. Source bindings do not certify execution, cache safety or timing.

# Software SPI reviewed baseline classifications

`reviewed_baseline.json` is the exact reviewed version 1 source-classification
input for baseline `94dab9c6b720ea277ec9177c93129c7dc646c5c3`: all 2,986 pinned
`src` paths, with associations for all 41 mandatory findings. Its SHA-256 is
`e1f0bb28228cb0b41b8975d466f7fbe5918c863e7393768329b24718c09a1896`.

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

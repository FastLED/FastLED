# Software SPI reviewed baseline classifications

`reviewed_baseline.json` is the exact reviewed version 1 source-classification
input for baseline `94dab9c6b720ea277ec9177c93129c7dc646c5c3`: all 2,986 pinned
`src` paths, with associations for all 51 mandatory findings. Its SHA-256 is
`0dfa396606be6feb790ea9b62affab2b835a6c22274e6a9ab4ec1680b0a618cf`.

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

Apollo #5089 promotes only the pinned actual runtime readback API source;
all old associations/anchors and raw baseline identities are retained.
nRF52 #5070 adds runtime dependencies behind its selector plus explicit NoMap,
not a current SPI-emitter positive. Upstream HAL binding and an admitted
Adafruit version are not resolved-release build or hardware proof. The new
source-only provenance records targeted review depth and prior row hashes;
no C6 execution receipt is imported or requalified for this metadata snapshot.

Seven additional forced-selection findings (#5068–#5074) remain unresolved across ESP8266, nRF51, nRF52 variants, GIGA, MGM240, modern LPC and legacy LPC. Native explicit specializations can override the forced primary; selected-type and actual fallback-route controls are required for every supported variant. Existing optimization matrices, including nRF51 #5047, remain mandatory. Source bindings do not certify execution, cache safety or timing.

Two additional forced-provider correctness requirements remain unresolved:
#5086 binds the mapped input-register toggle assumption to the original pinned
FastPin body (the current component is a dependency), and #5087 binds SAMD
Output input-disable/read-IN to explicit FORCE plus NO_PINMAP. Their source
associations preserve all old bindings, hashes and blob IDs. Neither mapped
plain-memory input writes nor another platform's readback fix certifies them.
# C6 mode classifier fixture

`esp32_c6_output_mode_capture.json` contains selected file-backed ELF32 symbol
bytes extracted by the root-owned SDK capture from ELF SHA-256
`66023b25bf9fd9926eef9bc11a110d8cf39e27d2bb16bb65e14c7bdec370a22f`.
It also binds the actual ELF ABS `__ashldi3` symbol to the retained SDK linker
definition. No ROM implementation bytes or execution receipt are supplied.
`esp32_c6_mode_producer.cpp.txt` and `esp32_c6_mode_startup.cpp.txt` retain the
successful producer body and FastLED-first companion source. Compile-only
closed-profile guards were subsequently added to the code-owned producer;
the historical byte capture precedes those guards and is not a new build
receipt. These are fixed
managed-profile inputs, not an alternative GPIO writer. Classifier unit tests
can validate only bounded static decoding; authentic builds and admission
remain internal to the opt-in adapter. No full finding group is certified.

Reproduce the closed C6 profile with
`FASTLED_RUN_ESP32_C6_MODE_PROOF=1 UV_NO_SYNC=1 uv run --no-sync pytest ci/tests/test_software_spi_audit.py -q -k esp32_c6_mode_real_three_stage_acceptance`.
The three builds compare current, exact old Output assignment, and restoration;
both PWM lookup outcomes assume normal RISC-V ABI-preserving callee returns.
The SDK/public enum and structure exports are compiled from the actual headers.
The raw database remains planned argv; full-library executed argv, runtime
GPIO readback/toggle, electrical effects and all full finding groups remain
unproved. The compact ELF built in parser unit tests is a format fixture using
these captured bytes, not a retained vendor build or an accepted proof.

C6 ordinary maps are unused diagnostics: absent/present/ambiguous status and
all present candidate hashes are retained, with freshness unproven. A missing
map supplies no exemption for ELF/SDK decoding, intended old-mode rejection or
required artifacts. No map from another stage is substituted. The separate RP
profiles retain their mandatory unique-map contract.

`esp32_c6_sdk_layout.json` retains root-extracted allocated section/PHDR metadata
from failed, unaccepted ELF `64fabd190204d8fc746a0304856ec001f8be439ce761fb4d9517b68b94ef490d`.
The paired `esp32c6.memory.ld.txt` and `esp32c6.sections.ld.txt` are exact pinned
SDK source scripts. The new format fixture uses that metadata and selected
captured bytes but synthetic tables and zero-filled uninspected content. It
tests only the narrow NOLOAD reservation exception and tamper rejection; it is
not a real firmware image, runtime mapping certificate or managed proof.

`esp32_c6_output_mode_actual_old_capture.json` is the exact selected-byte
capture from the successfully compiled old assignment in failed attempt
`071zeek4`; its sidecar records staged-source, ELF, capture and build-log hashes.
GCC's 142-byte path reuses the actual `a5` constant at the Output store. Tests
also change only that encoded constant from 2 to 3 as reciprocal parser input;
this synthetic control is not a source mutation or another SDK build. The
failed attempt remains unaccepted and the original captures are unchanged.

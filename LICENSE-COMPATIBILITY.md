# Custom-license consumer compatibility

Checked 2026-08-24 for `LicenseRef-FastLED-Reciprocal-1.0`.

| Consumer | Observed/defined behavior | FastLED treatment |
|---|---|---|
| SPDX source tooling | SPDX permits a user-defined `LicenseRef-` in license expressions and source-file identifiers, provided recipients can determine the corresponding text. | Use the exact `LicenseRef-FastLED-Reciprocal-1.0` identifier and ship `LICENSE` plus `MPL-2.0.txt` in every source archive. |
| GitHub | GitHub's Licensee-based detector compares `LICENSE` against a limited set of known licenses. A custom combined instrument should not be expected to receive an MPL or OSI badge. | Keep the custom name explicit and do not interpret absent/unknown detection as a licensing failure or MPL recognition. |
| PlatformIO 6.1.19 | `pio pkg pack` rejects `LicenseRef-FastLED-Reciprocal-1.0` with `Invalid SPDX license identifier`; packaging succeeds when the optional field is omitted. | Omit the `library.json` license field instead of falsely substituting MIT or MPL; retain the complete license files in the package and repeat this validation before publication. |
| Arduino Library Manager | The Arduino library specification's `library.properties` metadata fields do not define a license field. The library archive itself therefore carries the authoritative license documents. | Do not invent a non-standard property; include all root license files in the published Arduino archive. |
| Other package registries | A custom `LicenseRef` may be displayed literally, classified as non-standard, or rejected by registry-specific validation. | Test the exact publication candidate and never substitute `MPL-2.0`, `MIT`, or an OSI-approved label merely to satisfy display tooling. |

Primary references:

- SPDX, “Using SPDX short identifiers in source files”: <https://spdx.github.io/spdx-spec/v2.3/using-SPDX-short-identifiers-in-source-files/>
- GitHub, “Licensing a repository”: <https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository>
- PlatformIO, `library.json` license field: <https://docs.platformio.org/en/latest/manifests/library-json/fields/license.html>
- Arduino library specification: <https://docs.arduino.cc/arduino-cli/library-specification>

This report records tool behavior, not OSI approval or legal advice. Re-run the
package validations before the transition release because hosted consumers can
change independently of FastLED.

# FastLED license provenance and transition record

## Canonical source

- Repository: <https://github.com/FastLED/license>
- Release-candidate PR: <https://github.com/FastLED/license/pull/1>
- Vendored source commit: `b3c60107c223836a3f1e86e94e7249fb6400cd17`
- Reviewed release tag: **PENDING LEGAL REVIEW**
- Vendored artifact hashes: `LICENSE-ARTIFACTS.sha256`

The root `header-policy.toml` is the FastLED-specific ownership policy derived
from the canonical example. Its integration hash replaces the example-policy
hash in `LICENSE-ARTIFACTS.sha256`; the legal documents, schema, notice, and
tool remain byte-for-byte pinned to the source commit above.

## Transition

The reciprocal-license transition commit, FastLED tag, and release are
**PENDING LEGAL REVIEW**. This branch is a technical release candidate and must
not be merged or advertised as a completed license transition until:

1. an open-source licensing attorney records approval in `FastLED/license`;
2. the canonical repository publishes an immutable `v1.0.0` tag;
3. this file is updated to that tag and commit and all hashes are reverified;
4. contributor provenance for the in-scope source inventory is approved; and
5. the FastLED transition commit, tag, and first reciprocal release are named
   here before publication.

All FastLED versions distributed before that transition remain available under
the MIT terms under which they were released. `LICENSE-MIT-LEGACY` preserves
that text; no historical grant is withdrawn or changed.

## Ownership boundary

`header-policy.toml` is the machine-readable inventory. It excludes separately
licensed third-party source, the vendored Three.js tree, PJRC/SdFat-derived
Teensy source, and the complete Animartrix implementation with a reason and
provenance pointer for every boundary. The initial migration updated 2,083
owned release-source files and left 253 excluded files byte-for-byte unchanged.

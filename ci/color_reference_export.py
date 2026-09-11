"""Emit the C++ reference-vector table from `ci/golden/color-reference-v1.json`.

The golden corpus is the P5 deliverable and the thing FastLED#4040 (P6) is
measured against. Before this, `tests/fl/gfx/pipeline.cpp` carried 48 of its
285 vectors as hand-typed literals with the file named only in a comment --
so the C++ side could drift from the corpus silently, and nothing said which
48 or why. That is the same defect FastLED#4279 fixed elsewhere: a test that
reads a copy is not reading the thing it names.

This writes the vectors out mechanically instead. `ci/tests/
test_color_reference_export.py` regenerates and compares, so a stale header
fails rather than lying.

Usage:

    uv run python ci/color_reference_export.py            # write the header
    uv run python ci/color_reference_export.py --check    # exit 1 if stale
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parent.parent
GOLDEN_FILE = PROJECT_ROOT / "ci" / "golden" / "color-reference-v1.json"
HEADER_FILE = (
    PROJECT_ROOT / "tests" / "fl" / "gfx" / "test_utils" / "color_reference_vectors.hpp"
)

# The device this table covers.
#
# Only `rgb` for now, deliberately and not silently: the other four device
# profiles in the corpus (`rgbw`, `rgbww`, `rgbww_two_white`, `non_d65_white`)
# need emitter profiles on the C++ side that do not exist there yet, and
# emitting vectors nothing can run would be worse than emitting none. The
# count is asserted below so this cannot quietly become a smaller subset.
kDeviceProfile = "rgb"

# Source profiles, in the order `SourceProfile` exposes them.
kSourceProfiles = ("srgb_bt709", "display_p3", "bt2020", "linear_srgb")


@typechecked
@dataclass
class Vector:
    """One reference vector, reduced to what the C++ conformance case uses."""

    identifier: str
    source_profile: str
    encoded: tuple[int, int, int]  # noqa: DCT002
    emitter_light: tuple[float, float, float]  # noqa: DCT002
    mapped_xyz: tuple[float, float, float]  # noqa: DCT002


@typechecked
def _triple(values: Any, what: str, identifier: str) -> tuple[float, float, float]:  # noqa: DCT002
    """The three-element float triple at `values`, or a clear failure."""

    if not isinstance(values, list) or len(values) != 3:
        raise ValueError(f"{what} for {identifier} is not a three-element list")
    result: list[float] = []
    for value in values:
        if not isinstance(value, (int, float)):
            raise ValueError(f"{what} for {identifier} holds a non-number")
        result.append(float(value))
    return (result[0], result[1], result[2])


@typechecked
def load_vectors(golden: dict[str, Any]) -> list[Vector]:
    """The `rgb`-device vectors, in corpus order."""

    raw = golden.get("vectors")
    if not isinstance(raw, list):
        raise ValueError("golden corpus has no vector list")

    vectors: list[Vector] = []
    for entry in raw:
        if not isinstance(entry, dict):
            raise ValueError("golden corpus holds a non-object vector")
        if entry.get("device_profile") != kDeviceProfile:
            continue
        identifier = entry.get("id")
        if not isinstance(identifier, str):
            raise ValueError("golden vector has no string id")
        source_profile = entry.get("source_profile")
        if source_profile not in kSourceProfiles:
            raise ValueError(f"{identifier} names an unknown source {source_profile}")
        stages = entry.get("stages")
        if not isinstance(stages, dict):
            raise ValueError(f"{identifier} has no stage payload")

        encoded_floats = _triple(stages.get("encoded_rgb8"), "encoded_rgb8", identifier)
        encoded: list[int] = []
        for value in encoded_floats:
            code = int(round(value))
            if code < 0 or code > 255 or float(code) != value:
                raise ValueError(f"{identifier} has a non-8-bit source code {value}")
            encoded.append(code)

        vectors.append(
            Vector(
                identifier=identifier,
                source_profile=source_profile,
                encoded=(encoded[0], encoded[1], encoded[2]),
                emitter_light=_triple(
                    stages.get("emitter_light"), "emitter_light", identifier
                ),
                mapped_xyz=_triple(stages.get("mapped_xyz"), "mapped_xyz", identifier),
            )
        )

    if not vectors:
        raise ValueError(f"no vectors for device profile {kDeviceProfile}")
    return vectors


@typechecked
def render_header(vectors: list[Vector], schema_version: int) -> str:
    """The generated C++ header, as text."""

    lines: list[str] = []
    lines.append("// GENERATED FILE -- DO NOT EDIT BY HAND.")
    lines.append("//")
    lines.append("// Written by `ci/color_reference_export.py` from")
    lines.append(f"// `ci/golden/color-reference-v1.json` (schema {schema_version}),")
    lines.append(f"// device profile `{kDeviceProfile}`.")
    lines.append("//")
    lines.append("// `ci/tests/test_color_reference_export.py` regenerates this and")
    lines.append("// compares, so editing it by hand fails that test rather than")
    lines.append("// quietly detaching the C++ side from the corpus it is measured")
    lines.append("// against.")
    lines.append("")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "fl/stl/int.h"')
    lines.append("")
    lines.append("namespace fl {")
    lines.append("namespace reference_corpus {")
    lines.append("")
    lines.append("/// Which source profile a vector was decoded with.")
    lines.append("enum class SourceKind : fl::u8 {")
    for index, name in enumerate(kSourceProfiles):
        suffix = "," if index + 1 < len(kSourceProfiles) else ","
        lines.append(f"    {_enumerator(name)} = {index}{suffix}")
    lines.append("};")
    lines.append("")
    lines.append("/// One vector: what the reference was given, and what it produced.")
    lines.append("///")
    lines.append("/// `emitter_light` is the reference's per-emitter normalized linear")
    lines.append("/// flux at identity brightness -- directly comparable to what")
    lines.append("/// `processPixelQ16` returns. `mapped_xyz` is the gamut-mapped")
    lines.append("/// target the solve was aiming at.")
    lines.append("struct Vector {")
    lines.append("    const char* id;")
    lines.append("    SourceKind source;")
    lines.append("    fl::u8 encoded[3];")
    lines.append("    double emitter_light[3];")
    lines.append("    double mapped_xyz[3];")
    lines.append("};")
    lines.append("")
    lines.append(f"const int kVectorCount = {len(vectors)};")
    lines.append("")
    lines.append("const Vector kVectors[kVectorCount] = {")
    for vector in vectors:
        codes = ", ".join(str(value) for value in vector.encoded)
        light = ", ".join(_literal(value) for value in vector.emitter_light)
        target = ", ".join(_literal(value) for value in vector.mapped_xyz)
        lines.append(f'    {{"{vector.identifier}",')
        lines.append(f"     SourceKind::{_enumerator(vector.source_profile)},")
        lines.append(f"     {{{codes}}},")
        lines.append(f"     {{{light}}},")
        lines.append(f"     {{{target}}}}},")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace reference_corpus")
    lines.append("}  // namespace fl")
    lines.append("")
    return "\n".join(lines)


@typechecked
def _enumerator(source_profile: str) -> str:
    """The C++ enumerator for a corpus source-profile name."""

    parts = source_profile.split("_")
    result = ""
    for part in parts:
        result += part[:1].upper() + part[1:]
    return result


@typechecked
def _literal(value: float) -> str:
    """A double literal that round-trips the corpus value exactly."""

    return repr(value)


@typechecked
def build_header() -> str:
    """The header text for the current golden corpus."""

    golden = json.loads(GOLDEN_FILE.read_text())
    if not isinstance(golden, dict):
        raise ValueError("golden corpus is not an object")
    schema_version = golden.get("schema_version")
    if not isinstance(schema_version, int):
        raise ValueError("golden corpus has no integer schema_version")
    return render_header(load_vectors(golden), schema_version)


@typechecked
def main(argv: list[str]) -> int:
    """Write or check the generated header."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit 1 when the header on disk differs from what would be written",
    )
    args = parser.parse_args(argv)

    rendered = build_header()
    if args.check:
        if not HEADER_FILE.exists():
            print(f"color-reference-export: {HEADER_FILE} does not exist")
            return 1
        if HEADER_FILE.read_text() != rendered:
            print(
                "color-reference-export: "
                f"{HEADER_FILE.relative_to(PROJECT_ROOT)} is stale; "
                "run `uv run python ci/color_reference_export.py`"
            )
            return 1
        print("color-reference-export: header matches the golden corpus")
        return 0

    HEADER_FILE.write_text(rendered)
    print(f"color-reference-export: wrote {HEADER_FILE.relative_to(PROJECT_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

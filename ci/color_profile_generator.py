"""Artifact -> C++ `EmitterProfile` generation, and the gate in front of it (P3, #4037).

`FastLED/datasheets/measured-profiles/` is the append-only calibration
registry. Firmware never parses those files; this is the reviewed bridge that
turns them into checked-in constexpr C++.

The interesting part is not the rendering, it is the **refusal**. The registry
deliberately admits artifacts whose optical data is incomplete -- a datasheet
that publishes dominant wavelength and millicandela but no chromaticity
produces a catalog record, not a profile -- and the schema is explicit that
generation must not paper over that:

    An artifact may be a catalog record with unavailable photometry, but only
    a profile having usable finite xy/Y channel data can generate an
    `EmitterProfile`. [...] it does not turn missing PDF information into a
    fallback profile.

Both artifacts shipped today are exactly that case. Converting their dominant
wavelengths to spectral-locus chromaticities would produce numbers that look
like a profile and are not one, which is the failure this module exists to
make impossible rather than merely discouraged.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked


# The only artifact schema major this generator understands. A v1 reader must
# reject an unknown major outright; it may accept an unknown minor only while
# ignoring optional fields, which rendering does.
SUPPORTED_SCHEMA_MAJOR = 1

# Channel names each topology requires, exactly -- no extras, no duplicates.
TOPOLOGY_CHANNELS: dict[str, tuple[str, ...]] = {
    "rgb": ("red", "green", "blue"),
    "rgbw": ("red", "green", "blue", "white"),
    "rgbww": ("red", "green", "blue", "warm_white", "cool_white"),
}


@typechecked
@dataclass(frozen=True, slots=True)
class Artifact:
    """One parsed profile artifact, with the bytes it came from.

    `content_sha256` is over the original bytes rather than a re-serialisation:
    the provenance header has to identify the file that was read, and
    round-tripping JSON through Python does not preserve it.
    """

    profile_id: str
    schema_version: str
    payload: dict[str, object]
    content_sha256: str
    source_name: str


@typechecked
@dataclass(frozen=True, slots=True)
class Refusal:
    """Why an artifact cannot generate an `EmitterProfile`."""

    profile_id: str
    reasons: tuple[str, ...]


@typechecked
@dataclass(frozen=True, slots=True)
class Admission:
    """An artifact cleared to generate, with its channel data resolved."""

    artifact: Artifact
    topology: str
    channels: tuple[str, ...]


@typechecked
def parse_artifact(text: str, source_name: str) -> Artifact:
    """Parse one artifact, rejecting an unknown schema major.

    Raises rather than returning a refusal: an unparseable or
    future-major artifact is a tooling problem, not a statement about the
    part's optical data, and conflating the two would let a schema bump read
    as "this LED is not characterised".
    """

    payload = json.loads(text)
    if not isinstance(payload, dict):
        raise ValueError(f"{source_name}: artifact is not a JSON object")

    schema_version = payload.get("schema_version")
    if not isinstance(schema_version, str):
        raise ValueError(f"{source_name}: missing schema_version")
    major_text = schema_version.split(".")[0]
    if not major_text.isdigit() or int(major_text) != SUPPORTED_SCHEMA_MAJOR:
        raise ValueError(
            f"{source_name}: schema_version {schema_version!r} has an "
            f"unsupported major; this generator understands "
            f"{SUPPORTED_SCHEMA_MAJOR}.x only"
        )

    profile_id = payload.get("profile_id")
    if not isinstance(profile_id, str) or not profile_id:
        raise ValueError(f"{source_name}: missing profile_id")

    return Artifact(
        profile_id=profile_id,
        schema_version=schema_version,
        payload=payload,
        content_sha256=hashlib.sha256(text.encode("utf-8")).hexdigest(),
        source_name=source_name,
    )


@typechecked
def admit(artifact: Artifact) -> Admission | Refusal:
    """Decide whether an artifact may generate an `EmitterProfile`.

    The rules are the schema's, not this module's invention, and the first one
    does most of the work: *an absent top-level flag means false, even if
    individual channels have usable observations*. Inferring admissibility
    from the presence of channel data is precisely how a catalog record
    becomes a fake profile.
    """

    reasons: list[str] = []
    payload = artifact.payload

    if payload.get("runtime_admissible") is not True:
        reasons.append(
            "runtime_admissible is not true (absent counts as false, even when "
            "channels carry usable observations)"
        )

    topology = payload.get("topology")
    if not isinstance(topology, str) or topology not in TOPOLOGY_CHANNELS:
        reasons.append(
            "topology is absent or unknown; firmware generation requires one of "
            + ", ".join(sorted(TOPOLOGY_CHANNELS))
        )
        topology = ""

    channel_names: list[str] = []
    photometric = payload.get("photometric")
    channels_field = (
        photometric.get("channels") if isinstance(photometric, dict) else None
    )
    if not isinstance(channels_field, list):
        reasons.append("photometric.channels is absent or not a list")
        channels_field = []
    for entry in channels_field:
        if isinstance(entry, dict) and isinstance(entry.get("name"), str):
            channel_names.append(entry["name"])

    if topology:
        required = TOPOLOGY_CHANNELS[topology]
        if len(channel_names) != len(set(channel_names)):
            reasons.append("photometric.channels contains duplicate names")
        if tuple(sorted(channel_names)) != tuple(sorted(required)):
            reasons.append(
                f"topology {topology!r} requires exactly {list(required)}, found "
                f"{sorted(channel_names)}"
            )

    for entry in channels_field:
        if not isinstance(entry, dict):
            continue
        name = entry.get("name")
        missing = _missing_optical_fields(entry)
        if missing:
            reasons.append(f"channel {name!r} lacks {', '.join(missing)}")

    if reasons:
        return Refusal(artifact.profile_id, tuple(reasons))
    return Admission(artifact, topology, tuple(channel_names))


@typechecked
def _missing_optical_fields(channel: dict[str, object]) -> tuple[str, ...]:
    """The xy/Y data a channel needs, and does not have.

    A `*_range` observation is deliberately not accepted in place of a value:
    the schema keeps ranges precisely so a min/typical/max table is preserved
    rather than collapsed into an invented midpoint, and picking one here
    would re-introduce the invention one layer up.
    """

    missing: list[str] = []
    if channel.get("runtime_admissible") is not True:
        missing.append("runtime_admissible: true")
    if not _is_finite_xy(channel.get("chromaticity")):
        missing.append("a finite chromaticity {x, y}")
    if not _is_finite_observation(channel.get("relative_y")):
        missing.append("a finite relative_y")
    return tuple(missing)


@typechecked
def _is_finite_observation(value: object) -> bool:
    if not isinstance(value, dict):
        return False
    number = value.get("value")
    if isinstance(number, bool) or not isinstance(number, (int, float)):
        return False
    return number == number and number not in (float("inf"), float("-inf"))


@typechecked
def _is_finite_xy(value: object) -> bool:
    if not isinstance(value, dict):
        return False
    for axis in ("x", "y"):
        number = value.get(axis)
        if isinstance(number, bool) or not isinstance(number, (int, float)):
            return False
        if number != number or number in (float("inf"), float("-inf")):
            return False
    return True


@typechecked
def symbol_name(profile_id: str) -> str:
    """The versioned C++ symbol for a canonical profile ID (C8.3).

    Every segment survives, including the report ID, because two reports on
    one part are different profiles and pinning one is the whole point of a
    versioned symbol. The floating alias that drops the report ID is a
    separate, advancing thing.
    """

    cleaned = []
    for character in profile_id:
        cleaned.append(character.upper() if character.isalnum() else "_")
    symbol = "".join(cleaned)
    while "__" in symbol:
        symbol = symbol.replace("__", "_")
    return symbol.strip("_")


@typechecked
def alias_name(profile_id: str) -> str:
    """The floating alias for a profile ID: identity without the report."""

    segments = profile_id.split("/")
    if len(segments) < 2:
        return symbol_name(profile_id)
    return symbol_name("/".join(segments[:-1]))


@typechecked
def render_header(
    admissions: list[Admission],
    refusals: list[Refusal],
    datasheets_commit: str,
    generated_on: str,
) -> str:
    """The generated C++ header, provenance first (C9.2).

    Refusals are rendered as comments rather than dropped. A generated file
    that silently omits a part reads as "not catalogued"; one that names the
    part and says why it produced nothing reads as what it is, and stops the
    next person re-deriving the same dead end from the same PDF.
    """

    lines: list[str] = []
    lines.append("// AUTO-GENERATED by ci/color_profile_generator.py -- DO NOT EDIT.")
    lines.append("//")
    lines.append("// Source: FastLED/datasheets measured-profiles/")
    lines.append(f"// Datasheets commit: {datasheets_commit}")
    lines.append(f"// Generated on: {generated_on}")
    lines.append("//")
    for admission in admissions:
        lines.append(
            f"//   {admission.artifact.profile_id}  "
            f"sha256:{admission.artifact.content_sha256}"
        )
    lines.append("")
    lines.append('#include "fl/gfx/colorimetric_response.h"')
    lines.append("")
    lines.append("namespace fl {")
    lines.append("namespace generated_profiles {")
    lines.append("")

    for admission in admissions:
        lines.extend(_render_profile(admission))
        lines.append("")

    if refusals:
        lines.append("// Catalogued, and deliberately not generated. Each of these")
        lines.append("// is a real part whose published optical data cannot support an")
        lines.append("// EmitterProfile; see ci/color_profile_generator.py.")
        for refusal in refusals:
            lines.append(f"//")
            lines.append(f"//   {refusal.profile_id}")
            for reason in refusal.reasons:
                lines.append(f"//     - {reason}")
        lines.append("")

    lines.append("}  // namespace generated_profiles")
    lines.append("}  // namespace fl")
    return "\n".join(lines) + "\n"


@typechecked
def _render_profile(admission: Admission) -> list[str]:
    payload = admission.artifact.payload
    photometric = payload.get("photometric")
    assert isinstance(photometric, dict)
    entries = photometric.get("channels")
    assert isinstance(entries, list)
    by_name: dict[str, dict[str, object]] = {}
    for entry in entries:
        assert isinstance(entry, dict)
        by_name[str(entry["name"])] = entry

    chip = payload.get("chip_encoding")
    depth = 8
    if isinstance(chip, dict) and isinstance(chip.get("native_code_depth"), int):
        depth = int(chip["native_code_depth"])

    symbol = symbol_name(admission.artifact.profile_id)
    lines: list[str] = []
    lines.append(f"// {admission.artifact.profile_id}")
    lines.append(f"// sha256:{admission.artifact.content_sha256}")
    lines.append(
        f"constexpr colorimetric_response::EmitterProfile {symbol} = "
        "colorimetric_response::EmitterProfile::rgb("
    )
    lines.append(f'    "{admission.artifact.profile_id}",')
    for name in ("red", "green", "blue"):
        chromaticity = by_name[name]["chromaticity"]
        assert isinstance(chromaticity, dict)
        lines.append(
            f"    Chromaticity({float(chromaticity['x']):.6f}f, "
            f"{float(chromaticity['y']):.6f}f),"
        )
    values: list[str] = []
    for name in ("red", "green", "blue"):
        relative = by_name[name]["relative_y"]
        assert isinstance(relative, dict)
        values.append(f"{float(relative['value']):.6f}f")
    lines.append(f"    {', '.join(values)},")
    provenance = payload.get("provenance")
    kind = "unknown"
    report = "unknown"
    if isinstance(provenance, dict):
        kind = str(provenance.get("kind", "unknown"))
        report = str(provenance.get("report_id", "unknown"))
    lines.append(f'    "{kind}", "{report}");')
    lines.append(f"// native code depth: {depth}")
    lines.append(
        f"// floating alias candidate: {alias_name(admission.artifact.profile_id)}"
    )
    return lines


@typechecked
def load_artifacts(directory: Path) -> list[Artifact]:
    """Every `*.profile.json` under `directory`, in stable name order."""

    artifacts: list[Artifact] = []
    for path in sorted(directory.glob("*.profile.json")):
        artifacts.append(parse_artifact(path.read_text(encoding="utf-8"), path.name))
    return artifacts

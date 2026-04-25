"""
Asset Scanner for the FastLED v1 Asset Pipeline (issue #2284).

Walks a sketch's ``data/`` directory looking for ``*.lnk`` files, parses each
one for a URL plus optional ``key=value`` metadata lines, and emits a JSON
manifest that fbuild consumers (notably the WASM loader) can read.

v1 scope:
    - Platforms: WASM + host/stub only. ESP32 LittleFS is future work.
    - Metadata keys parsed but NOT enforced: ``sha256=<hex>``, ``fallback=<url>``.
      The scanner records them so the shipping manifest is forward-compatible
      with future integrity/retry features; the runtime ignores them in v1.
    - ``.lnk`` file naming: ``<asset>.<ext>.lnk`` (e.g. ``track.mp3.lnk``). The
      scanner reports the relative path WITHOUT the trailing ``.lnk``, so
      sketches can write ``fl::asset("data/track.mp3")`` and get a hit.

Manifest shape::

    {
        "data/track.mp3": {
            "url": "https://example.com/track.mp3",
            "sha256": null,
            "fallback": null
        }
    }

The same dict can be emitted to a JSON file and/or serialized to a
``window.fastledAssetManifest = {...}`` bootstrap snippet for HTML injection.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path


# -----------------------------------------------------------------------------
# Public types
# -----------------------------------------------------------------------------


@dataclass
class AssetEntry:
    """Resolved metadata for a single ``*.lnk`` file.

    Attributes:
        url: Primary URL from the first non-comment line of the ``.lnk``.
        sha256: Optional hex digest declared in the ``.lnk`` (``sha256=...``).
            v1 runtime ignores this; parser captures it for forward-compat.
        fallback: Optional mirror URL declared in the ``.lnk`` (``fallback=...``).
            v1 runtime ignores this; parser captures it for forward-compat.
    """

    url: str
    sha256: str | None = None
    fallback: str | None = None


@dataclass
class AssetScanResult:
    """Result of scanning a sketch's ``data/`` directory.

    Attributes:
        manifest: Dict mapping relative asset paths (POSIX form, e.g.
            ``"data/track.mp3"``) to ``AssetEntry``. Missing ``data/`` or an
            empty one yields an empty dict — not an error.
        warnings: Non-fatal issues encountered during scanning (malformed
            ``.lnk`` files, unreadable files, etc.). Callers should log them
            but the build should NOT fail on these.
    """

    manifest: dict[str, AssetEntry] = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)

    def to_json_dict(self) -> dict[str, dict[str, str | None]]:
        """Return manifest as a plain nested dict for JSON serialization."""
        return {key: asdict(entry) for key, entry in self.manifest.items()}


# -----------------------------------------------------------------------------
# Internals
# -----------------------------------------------------------------------------


def _parse_lnk_content(content: str) -> AssetEntry | None:
    """Parse the text of a single ``.lnk`` file.

    Mirrors the C++ ``fl::parse_lnk_with_metadata()`` logic in
    ``src/fl/stl/url.h``:

    - Comment lines (``#...``) and blank lines are skipped.
    - First non-comment line is the primary URL.
    - Subsequent ``key=value`` lines are recorded as metadata. Recognized
      keys: ``sha256``, ``fallback``. Unknown keys are silently ignored.

    Returns ``None`` if no URL was found.
    """
    primary_url: str | None = None
    sha256: str | None = None
    fallback: str | None = None

    for raw_line in content.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        if primary_url is None:
            primary_url = line
            continue

        if "=" not in line:
            # Unknown non-kv line — forward-compat, ignore.
            continue

        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if key == "sha256":
            sha256 = value
        elif key == "fallback":
            fallback = value
        # else: unknown metadata key, ignore.

    if primary_url is None:
        return None

    return AssetEntry(url=primary_url, sha256=sha256, fallback=fallback)


# -----------------------------------------------------------------------------
# Public entry points
# -----------------------------------------------------------------------------


def scan_sketch_assets(sketch_dir: Path) -> AssetScanResult:
    """Scan ``<sketch_dir>/data/`` for ``*.lnk`` asset links.

    Only the ``data/`` subdirectory is walked. Files whose names do NOT end in
    ``.lnk`` are ignored — they ship as-is as normal assets and don't need a
    manifest entry.

    Args:
        sketch_dir: Path to the sketch directory (the one containing the
            ``.ino`` / ``.cpp`` file).

    Returns:
        :class:`AssetScanResult` with the manifest plus any warnings.
    """
    result = AssetScanResult()
    data_dir = sketch_dir / "data"
    if not data_dir.is_dir():
        return result

    for lnk_path in sorted(data_dir.rglob("*.lnk")):
        if not lnk_path.is_file():
            continue

        # Relative asset path, with the ``.lnk`` suffix stripped so the key
        # matches the name the sketch will write (e.g. ``"data/track.mp3"``).
        rel_with_lnk = lnk_path.relative_to(sketch_dir).as_posix()
        if not rel_with_lnk.endswith(".lnk"):
            # Defensive — rglob should never hand us one of these, but be safe.
            continue
        rel_without_lnk = rel_with_lnk[: -len(".lnk")]

        try:
            content = lnk_path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            result.warnings.append(
                f"asset-scan: {lnk_path}: not valid UTF-8 ({exc}); skipped"
            )
            continue
        except OSError as exc:
            result.warnings.append(
                f"asset-scan: {lnk_path}: read failed ({exc}); skipped"
            )
            continue

        entry = _parse_lnk_content(content)
        if entry is None:
            result.warnings.append(
                f"asset-scan: {lnk_path}: no URL found in .lnk; skipped"
            )
            continue

        result.manifest[rel_without_lnk] = entry

    return result


def write_manifest_json(scan: AssetScanResult, out_path: Path) -> None:
    """Serialize the manifest to JSON on disk.

    The format matches what the WASM bootstrap expects for injection as
    ``window.fastledAssetManifest``. ``None`` fields are preserved so future
    JS consumers can detect absent metadata without extra contains-checks.

    Args:
        scan: Result of :func:`scan_sketch_assets`.
        out_path: Destination ``.json`` file. Parent directories are created
            on demand.
    """
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(scan.to_json_dict(), f, indent=2, sort_keys=True)
        f.write("\n")


def manifest_to_js_bootstrap(scan: AssetScanResult) -> str:
    """Produce a ``window.fastledAssetManifest = {...}`` JS snippet.

    Used by the WASM HTML template to inject the manifest before the FastLED
    runtime boots. Callers splice the returned string into a ``<script>`` tag.
    """
    payload = json.dumps(scan.to_json_dict(), sort_keys=True)
    return f"window.fastledAssetManifest = {payload};\n"


def manifest_to_cpp_header(scan: AssetScanResult) -> str:
    """Produce a C++ header that registers all manifest entries at startup.

    Used by the WASM (and future host) C++ build to plug
    ``fl::register_asset(path, url)`` calls into the runtime so
    ``fl::resolve_asset()`` sees the entries without a JS round-trip.

    Emits a self-contained translation unit ready to be compiled alongside
    the sketch. Relies on a function-local static + ``[[maybe_unused]]``
    variable to force construction-order independence.
    """
    lines: list[str] = []
    lines.append("// AUTO-GENERATED by ci/compiler/asset_scanner.py — do not edit.")
    lines.append("// Registers the sketch's v1 asset manifest (issue #2284).")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "fl/asset/asset.h"')
    lines.append('#include "fl/stl/url.h"')
    lines.append("")
    lines.append("namespace {")
    lines.append("struct FastledAssetManifestRegistrar {")
    lines.append("    FastledAssetManifestRegistrar() {")
    for key in sorted(scan.manifest.keys()):
        entry = scan.manifest[key]
        path_lit = _cpp_string_literal(key)
        url_lit = _cpp_string_literal(entry.url)
        lines.append(f"        ::fl::register_asset({path_lit}, ::fl::url({url_lit}));")
    lines.append("    }")
    lines.append("};")
    lines.append(
        "static FastledAssetManifestRegistrar s_fastled_asset_manifest_registrar;"
    )
    lines.append("}  // namespace")
    lines.append("")
    return "\n".join(lines)


def _cpp_string_literal(s: str) -> str:
    """Escape ``s`` as a C++ double-quoted string literal."""
    escaped = (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'

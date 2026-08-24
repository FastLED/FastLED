"""
Asset Scanner for the FastLED v1 Asset Pipeline (issue #2284).

Walks a sketch's ``data/`` directory looking for asset declarations and emits a
JSON manifest that consumers (notably the WASM loader) can read.

This is the single Python entry point for parsing asset declarations. It
accepts every form in use so callers never need to care which one a sketch
picked:

    - ``*.lnk``, **text form** — the format the C++ runtime parses
      (``fl::parse_lnk_with_metadata()`` in ``src/fl/stl/url.h``): first
      non-comment line is the URL, then ``key=value`` metadata.
    - ``*.lnk``, **JSON form** — what ``fbuild lnk add`` writes:
      ``{"v": 1, "url": ..., "sha256": ..., "size": ..., "extract": ...}``.
    - ``assets.json`` — one file declaring several assets at once.

Format detection sniffs the first non-blank, non-comment character: ``{``
means JSON, anything else is text. When a ``.lnk`` and an ``assets.json``
entry name the same asset, the ``.lnk`` wins, so a sketch can override one
entry of a shared manifest.

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
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, cast


#: Optional multi-asset manifest living beside the ``.lnk`` sidecars.
ASSETS_JSON = "assets.json"

#: Storage targets an asset may declare, from FastLED/fbuild#1356. A target
#: says where the bytes should END UP; the url says where they come from.
STORAGE_TARGETS = frozenset({"littlefs", "spiffs", "sdcard", "firmware", "vfs", "none"})

#: Targets backed by on-chip flash, i.e. the ones `fl::getEmbeddedFs()` serves.
#: Declaring one of these is what pulls the embedded filesystem into a build.
EMBEDDED_FS_TARGETS = frozenset({"littlefs", "spiffs"})

#: One serialized manifest entry. This is a JSON wire shape, not a domain
#: type — it is handed straight to ``json.dump`` — so it stays a plain dict
#: rather than a dataclass.
ManifestRecord = dict[str, "str | int | None"]


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
        size: Optional expected byte count. Only the JSON form carries this;
            used for a cheap mismatch check before hashing.
        extract: Optional archive handling (``"file"``/``"zip"``/``"tar.gz"``).
            Only the JSON form carries this; captured for forward-compat.
        storage: Optional storage target — where the bytes should end up on the
            device (``littlefs``, ``spiffs``, ``sdcard``, ``firmware``, ``vfs``,
            ``none``). Absent means undeclared, which is not an error: a sketch
            that never says where an asset goes still gets a manifest. Declaring
            an on-chip target is what enables the embedded filesystem for the
            build — see ``AssetScanResult.embedded_fs_assets``.
    """

    url: str
    sha256: str | None = None
    fallback: str | None = None
    size: int | None = None
    extract: str | None = None
    storage: str | None = None


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

    @property
    def storage_targets(self) -> set[str]:
        """Every distinct storage target declared by any asset."""
        return {e.storage for e in self.manifest.values() if e.storage}

    def embedded_fs_assets(self) -> list[str]:
        """Asset paths that ask to live on on-chip flash, sorted.

        A non-empty result is what pulls the embedded filesystem into a build:
        the assets have declared they need it, so nothing else should have to.
        """
        return sorted(
            path
            for path, entry in self.manifest.items()
            if entry.storage in EMBEDDED_FS_TARGETS
        )

    def unknown_storage_targets(self) -> set[str]:
        """Declared targets outside the known vocabulary.

        Not an error -- a newer tool may write a target this scanner predates.
        Reported so a typo (``littleFS``, ``lfs``) does not silently behave as
        "undeclared", which would look identical to working.
        """
        return {t for t in self.storage_targets if t not in STORAGE_TARGETS}

    def to_json_dict(self) -> dict[str, ManifestRecord]:
        """Return manifest as a plain nested dict for JSON serialization.

        ``url``/``sha256``/``fallback`` are always present (existing consumers
        rely on the shape, nulls included). ``size``/``extract`` are emitted
        only when set, so a text-format ``.lnk`` serializes byte-identically to
        how it did before those fields existed.
        """
        out: dict[str, ManifestRecord] = {}
        for key, entry in self.manifest.items():
            record: ManifestRecord = {
                "url": entry.url,
                "sha256": entry.sha256,
                "fallback": entry.fallback,
            }
            if entry.size is not None:
                record["size"] = entry.size
            if entry.extract is not None:
                record["extract"] = entry.extract
            if entry.storage is not None:
                record["storage"] = entry.storage
            out[key] = record
        return out


# -----------------------------------------------------------------------------
# Internals
# -----------------------------------------------------------------------------


def _storage_of(spec: dict[str, Any], default: str | None = None) -> str | None:
    """Read a storage target from a JSON spec.

    Accepts both shapes in circulation: a flat ``"storage": "littlefs"`` and
    fbuild's proposed ``"dest": {"target": "littlefs"}`` (FastLED/fbuild#1356).
    An unrecognised target is passed through rather than dropped -- the scanner
    reports it and lets the build decide, so a newer descriptor written by a
    newer tool is not silently discarded by an older scanner.
    """
    raw: Any = spec.get("storage")
    if raw is None:
        dest: Any = spec.get("dest")
        if isinstance(dest, dict):
            raw = cast("dict[str, Any]", dest).get("target")
    if raw is None:
        return default
    return str(raw).strip().lower()


def _parse_assets_json(content: str) -> tuple[dict[str, AssetEntry], list[str]]:
    """Parse an ``assets.json`` declaring several assets at once.

    ``{"assets": {"video.rgb": {"urls": [...], "sha256": "...", "size": N}}}``

    Returns the entries plus a list of non-fatal problems, so one bad entry
    does not discard the rest.
    """
    problems: list[str] = []
    try:
        parsed: Any = json.loads(content)
    except json.JSONDecodeError as exc:
        return {}, [f"invalid JSON ({exc})"]
    if not isinstance(parsed, dict):
        return {}, ["top level must be an object"]

    top: dict[str, Any] = cast("dict[str, Any]", parsed)
    raw_entries: Any = top.get("assets")
    if not isinstance(raw_entries, dict):
        return {}, ["missing 'assets' object"]

    # A file-level default so many assets can share one target without
    # repeating it (FastLED/fbuild#1356). Per-entry always wins.
    raw_defaults: Any = top.get("defaults")
    default_storage = (
        _storage_of(cast("dict[str, Any]", raw_defaults))
        if isinstance(raw_defaults, dict)
        else None
    )

    out: dict[str, AssetEntry] = {}
    for name, raw_spec in cast("dict[str, Any]", raw_entries).items():
        if not isinstance(raw_spec, dict):
            problems.append(f"entry {name!r} is not an object; skipped")
            continue
        spec = cast("dict[str, Any]", raw_spec)

        raw_urls: Any = spec.get("urls")
        if isinstance(raw_urls, list):
            urls = [str(u) for u in cast("list[Any]", raw_urls)]
        elif "url" in spec:
            urls = [str(spec["url"])]
        else:
            urls = []
        if not urls:
            problems.append(f"entry {name!r} has no url/urls; skipped")
            continue

        size: Any = spec.get("size_bytes", spec.get("size"))
        sha: Any = spec.get("sha256")
        out[str(name)] = AssetEntry(
            url=urls[0],
            sha256=str(sha) if sha is not None else None,
            fallback=urls[1] if len(urls) > 1 else None,
            size=int(size) if size is not None else None,
            storage=_storage_of(spec, default_storage),
        )
    return out, problems


def _parse_lnk_json(content: str) -> AssetEntry | None:
    """Parse fbuild's JSON ``.lnk`` schema.

    ``{"v": 1, "url": "...", "sha256": "...", "size": N, "extract": "file"}``
    — the form written by ``fbuild lnk add``. ``url`` may also be a list, in
    which case the first entry is primary and the second becomes ``fallback``.

    Returns ``None`` if the document is not usable as a ``.lnk``.
    """
    try:
        parsed: Any = json.loads(content)
    except json.JSONDecodeError:
        return None
    if not isinstance(parsed, dict):
        return None
    doc_typed: dict[str, Any] = cast("dict[str, Any]", parsed)

    raw_url: Any = doc_typed.get("url")
    if isinstance(raw_url, list):
        urls = [str(u) for u in cast("list[Any]", raw_url)]
    elif raw_url is not None:
        urls = [str(raw_url)]
    else:
        urls = []
    if not urls:
        return None

    fallback = doc_typed.get("fallback")
    size = doc_typed.get("size")
    sha = doc_typed.get("sha256")
    extract = doc_typed.get("extract")

    return AssetEntry(
        url=urls[0],
        sha256=str(sha) if sha is not None else None,
        fallback=(
            urls[1]
            if len(urls) > 1
            else (str(fallback) if fallback is not None else None)
        ),
        size=int(size) if size is not None else None,
        extract=str(extract) if extract is not None else None,
        storage=_storage_of(doc_typed),
    )


def _parse_lnk_content(content: str) -> AssetEntry | None:
    """Parse a single ``.lnk`` file in either supported format.

    Two formats exist in the ecosystem and both are accepted here, so a sketch
    can use whichever its toolchain emits:

    **Text** — the format the C++ runtime parses
    (``fl::parse_lnk_with_metadata()`` in ``src/fl/stl/url.h``):

    - Comment lines (``#...``) and blank lines are skipped.
    - First non-comment line is the primary URL.
    - Subsequent ``key=value`` lines are recorded as metadata. Recognized
      keys: ``sha256``, ``fallback``, ``storage``. Unknown keys are silently
      ignored.

    **JSON** — the format ``fbuild lnk add`` writes
    (``{"v": 1, "url": ..., "sha256": ..., "size": ...}``).

    The format is detected by sniffing the first non-blank, non-comment
    character: ``{`` means JSON, anything else is text. Returns ``None`` if no
    URL was found in either form.
    """
    for probe in content.splitlines():
        stripped = probe.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("{"):
            return _parse_lnk_json(content)
        break
    primary_url: str | None = None
    sha256: str | None = None
    fallback: str | None = None
    storage: str | None = None

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
        elif key == "storage":
            storage = value.lower()
        # else: unknown metadata key, ignore.

    if primary_url is None:
        return None

    return AssetEntry(
        url=primary_url, sha256=sha256, fallback=fallback, storage=storage
    )


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

    # assets.json first, so a same-named .lnk below overrides it.
    manifest_path = data_dir / ASSETS_JSON
    if manifest_path.is_file():
        rel_dir = data_dir.relative_to(sketch_dir).as_posix()
        try:
            entries, problems = _parse_assets_json(
                manifest_path.read_text(encoding="utf-8")
            )
        except (OSError, UnicodeDecodeError) as exc:
            entries, problems = {}, [f"read failed ({exc})"]
        for problem in problems:
            result.warnings.append(f"asset-scan: {manifest_path}: {problem}")
        for name, entry in entries.items():
            result.manifest[f"{rel_dir}/{name}"] = entry

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


def embedded_fs_defines(scan: AssetScanResult) -> list[str]:
    """Preprocessor defines a build needs to satisfy the sketch's assets.

    Separate from the reporting below so the decision is testable on its own
    and has exactly one home. Empty unless some asset declares on-chip storage.

    Today this is only non-empty for ESP8266. ESP32 needs no flag -- its
    embedded filesystem is already selected automatically by
    `platforms/embedded_fs.h` -- so on the platform people actually use for
    LittleFS, an asset declaring `storage=littlefs` already works.

    Nothing applies these yet: build defines are assembled per-board in
    `ci/boards.py`, and this is per-sketch. Applying them would also currently
    break the ESP8266 builds it targets, because that backend cannot compile
    until FastLED/fbuild#1380 ships the core's littlefs submodule. Tracked in
    FastLED#4020.
    """
    return ["FL_ESP8266_EMBEDDED_FS"] if scan.embedded_fs_assets() else []


def announce_storage_requirements(scan: AssetScanResult) -> None:
    """Report what the sketch's assets need from the build.

    An asset that declares ``storage=littlefs`` (or ``spiffs``) is stating a
    build requirement: the firmware has to be able to read on-chip flash. This
    prints, in green, which assets asked for it and where that requirement is
    already met.

    It does not claim the build enabled anything. ESP32 genuinely is automatic
    -- `platforms/embedded_fs.h` selects LittleFS with no flag -- but ESP8266
    still needs `FL_ESP8266_EMBEDDED_FS`, and `embedded_fs_defines` is not yet
    consumed by any build layer (FastLED#4020). Saying "enabled automatically"
    on ESP8266 would report work that did not happen, which is worse than
    saying nothing: the user would stop looking.

    Green because it is neither a warning nor an error. An unrecognised target
    is the yellow case -- a typo like `littleFS` would otherwise behave exactly
    like "undeclared", which looks identical to working.

    Returns nothing on purpose. An earlier version returned the defines from
    here and the caller dropped them, which read as if the build were applying
    something it was not. `embedded_fs_defines` owns that decision.
    """
    from ci.util.color_output import print_green, print_yellow

    unknown = scan.unknown_storage_targets()
    if unknown:
        print_yellow(
            "assets: unrecognized storage target(s): "
            + ", ".join(sorted(unknown))
            + f". Known: {', '.join(sorted(STORAGE_TARGETS))}. "
            "Treated as undeclared -- the asset will not pull in a filesystem."
        )

    needs_fs = scan.embedded_fs_assets()
    if not needs_fs:
        return

    targets = sorted(scan.storage_targets & EMBEDDED_FS_TARGETS)
    shown = ", ".join(needs_fs[:4])
    if len(needs_fs) > 4:
        shown += f", +{len(needs_fs) - 4} more"
    print_green(
        f"assets: {len(needs_fs)} asset(s) declare on-chip storage "
        f"({'/'.join(targets)}): {shown}"
    )
    print_green(
        "  ESP32 selects its embedded filesystem automatically. ESP8266 needs "
        "-DFL_ESP8266_EMBEDDED_FS, which no build applies yet (FastLED#4020)."
    )

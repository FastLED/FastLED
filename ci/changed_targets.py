"""Classify changed files by the platform families they can affect (#4933).

The memory report and the ESP32-S3 bloat gate both spend minutes building
board firmware. A change under one platform directory cannot move another
platform's binary, so both tools ask this module which platforms a pull
request actually touches and skip the rest.

Classification rules:

- `src/platforms/wasm/**` and `src/platforms/stub/**` are ignored: the host
  and WASM builds are not size targets and the user does not care about them.
- `src/platforms/<family>/**` selects that family. `src/platforms/esp/8266`
  is ESP8266, `src/platforms/esp/32` is ESP32, and anything else directly
  under `src/platforms/esp/` affects both.
- `src/platforms/shared/**`, dispatcher headers directly in
  `src/platforms/`, and everything else under `src/` (including `src/fl/`
  and `src/third_party/`) is common code: it can move any platform's size.
- An unrecognized platform directory is treated as common code, so a new
  platform still gets measured instead of silently skipped.

    uv run ci/changed_targets.py --changed-file changed.txt --json
    uv run ci/changed_targets.py --changed-file changed.txt --affects esp32
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


COMMON = "common"

# Platform directories whose changes never affect a measured board.
IGNORED_PLATFORMS = frozenset({"wasm", "stub"})

# src/platforms/esp/<sub>/... -> family. Anything else under esp/ is both.
ESP_SUBDIRS = {
    "8266": "esp8266",
    "32": "esp32",
}

# src/platforms/arm/<sub>/... -> family.
ARM_SUBDIRS = {
    "teensy": "teensy",
    "rp": "rp",
    "nrf52": "nrf52",
    "nrf": "nrf52",
    "stm32": "stm32",
}

# Platform families that are directly a top-level platform directory.
DIRECT_PLATFORMS = frozenset({"avr", "esp8266", "esp32"})


def classify(path: str) -> str | None:
    """Return the platform family for one changed path, or None to ignore it.

    The literal `COMMON` means the change can affect every platform.
    """
    parts = path.strip().replace("\\", "/").split("/")
    if len(parts) < 2 or parts[0] != "src":
        # CI scripts, workflow files and docs only exercise the tooling.
        return COMMON
    if parts[1] != "platforms":
        return COMMON
    if len(parts) < 3:
        # A dispatcher header directly in src/platforms/.
        return COMMON
    family = parts[2]
    if family in IGNORED_PLATFORMS:
        return None
    if family == "shared":
        return COMMON
    sub = parts[3] if len(parts) > 3 else ""
    if family == "esp":
        return ESP_SUBDIRS.get(sub, "esp")
    if family == "arm":
        return ARM_SUBDIRS.get(sub, COMMON)
    if family in DIRECT_PLATFORMS:
        return family
    return COMMON


def affected(paths: list[str]) -> tuple[bool, frozenset[str]]:
    """Split changed paths into (touches_common, affected_platforms).

    `affected_platforms` never contains `COMMON`; `touches_common` carries
    that bit instead so callers can apply the "common code" rule directly.
    """
    platforms: set[str] = set()
    common = False
    for path in paths:
        family = classify(path)
        if family is None:
            continue
        if family == COMMON:
            common = True
        else:
            platforms.add(family)
    return common, frozenset(platforms)


def affects_platform(paths: list[str], platform: str) -> bool:
    """True when the change can move `platform`'s build (common code counts)."""
    common, platforms = affected(paths)
    return common or platform in platforms


def _read_changed(path: Path) -> list[str]:
    return [
        line.strip()
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--changed-file",
        type=Path,
        required=True,
        help="File with one changed path per line (git diff --name-only output).",
    )
    parser.add_argument(
        "--json", action="store_true", help="Print the classification as JSON."
    )
    parser.add_argument(
        "--affects",
        metavar="PLATFORM",
        help="Exit 0 if the change can affect PLATFORM (or common code), 1 otherwise.",
    )
    args = parser.parse_args()

    paths = _read_changed(args.changed_file)
    common, platforms = affected(paths)
    if args.affects:
        return 0 if (common or args.affects in platforms) else 1
    if args.json:
        sys.stdout.write(
            json.dumps(
                {"common": common, "platforms": sorted(platforms)}, sort_keys=True
            )
            + "\n"
        )
        return 0
    if common:
        print("common")
    for platform in sorted(platforms):
        print(platform)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

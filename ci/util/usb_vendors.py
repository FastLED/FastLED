"""Lazy, in-memory USB vendor (VID → name) resolver.

Ingests the compressed `ci/data/usb-vendors.tar.zst` table (mirrored from
fbuild's `fbuild_core::usb` tier-1 embedded archive — the same data
`fbuild port scan` resolves against) and decompresses it **once on first
use** into a module-level dict. This is the Python analogue of fbuild's
`OnceLock<HashMap<u16, String>>`: the tar is inflated + parsed exactly one
time per process, then every lookup is an O(1) dict hit.

Why this exists: FastLED's serial-port auto-detection previously guessed
the right COM port from a small hand-maintained VID:PID map plus a crude
"prefer CP2102/CH340/FTDI" heuristic, which mis-selected a co-attached
board (e.g. picked an ESP32's CP210x when the target was a Teensy). This
gives the same authoritative vendor identification fbuild uses, without
shelling out to `fbuild port scan`.

Table format (inside the tar, `usb-vendors.txt`): a single line of
`vid:vendor` pairs, comma-separated, with the vendor name percent-escaped
per RFC 3986 — e.g. `16c0:Van%20Ooijen...,1fc9:NXP%20Semiconductors,...`.

Note on the 16C0 collision: `0x16C0` ("Van Ooijen Technische
Informatica", the community V-USB registrant) is shared by PJRC Teensy AND
the legacy LPCXpresso-VCOM firmware, so the vendor name alone cannot tell
those two apart — that narrow case is still disambiguated by the curated
per-environment VID:PID map in `port_utils.py`. For every other board the
vendor name is unambiguous (NXP, Silicon Labs, Espressif, Arduino, FTDI…).
"""

from __future__ import annotations

import io
import tarfile
import urllib.parse
from functools import lru_cache
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Vendored copy of fbuild-core/data/usb-vendors.tar.zst. Refresh with
# `cp ../fbuild/crates/fbuild-core/data/usb-vendors.tar.zst ci/data/`.
_TABLE_PATH = Path(__file__).resolve().parents[1] / "data" / "usb-vendors.tar.zst"

# Guard against a corrupt/oversized archive blowing up memory on decompress.
_MAX_DECOMPRESSED_BYTES = 8 * 1024 * 1024


@lru_cache(maxsize=1)
def _vendor_map() -> dict[int, str]:
    """Decompress + parse the table exactly once; cache in memory.

    Returns an empty dict (never raises) if the table is missing or
    unreadable, so port detection degrades to the curated map / heuristics
    rather than crashing.
    """
    try:
        raw = _TABLE_PATH.read_bytes()
    except OSError:
        return {}
    try:
        import zstandard

        decompressed = zstandard.ZstdDecompressor().decompress(
            raw, max_output_size=_MAX_DECOMPRESSED_BYTES
        )
        with tarfile.open(fileobj=io.BytesIO(decompressed)) as tf:
            member = tf.extractfile("usb-vendors.txt")
            if member is None:
                return {}
            text = member.read().decode("utf-8", "replace")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:  # noqa: BLE001 — any decode failure → empty map fallback
        return {}

    out: dict[int, str] = {}
    for pair in text.split(","):
        vid_str, sep, name = pair.partition(":")
        if not sep:
            continue
        try:
            vid = int(vid_str, 16)
        except ValueError:
            continue
        out[vid] = urllib.parse.unquote(name)
    return out


def vendor_name(vid: int | None) -> str | None:
    """Return the USB vendor name for `vid` (lazy-loaded, cached), or None."""
    if vid is None:
        return None
    return _vendor_map().get(vid)


def vendor_count() -> int:
    """Number of vendors in the loaded table (0 if the table is unavailable)."""
    return len(_vendor_map())

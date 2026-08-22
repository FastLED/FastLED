# USB VID/PID registry — FastLED/boards is the source of truth

**Rule: every board/device USB VID:PID record lands in
[FastLED/boards](https://github.com/FastLED/boards) first.** FastLED and fbuild
are both *consumers*. Neither repo is allowed to grow a hand-maintained USB
identity catalogue, and a VID/PID must never be introduced into this repo as
the place it first exists.

If a board cannot be identified, the fix is a FastLED/boards change plus a
version cascade — not a new literal in `ci/`.

## Why the registry, and not a local table

A local table forks on the first board that behaves differently, and the fork
is invisible until hardware is attached. Three consumers had already drifted
before this rule was written: `ci/util/port_utils.py`, `ci/util/serial_probe.py`,
and fbuild's former generated Rust tables all carried overlapping,
independently-edited copies of the same identities. FastLED/boards ends that by
publishing one artifact that every consumer ingests.

## The publication pipeline

```
FastLED/boards data branches           source of truth
  (platformio / arduino / vendors / other)
              │
              │  site.yml regenerates on every push
              ▼
https://fastled.github.io/boards/usb-vids.proto.zstd
              │                    ▲
              │                    └── zstd-compressed protobuf; ~7.7 KB,
              │                        36 vendors / 1056 products as of
              │                        2026-08-22
              ▼
fbuild ingestion (build + runtime cache + fallback)
   crates/fbuild-core/src/usb/data.rs → USB_VIDS_PROTO_ZSTD_URL
   crates/fbuild-core/src/usb/profiles.rs → schema + SHA-256 verification
              │
              │  version bump cascaded via the `fbuild==X.Y.Z` pin
              ▼
FastLED (this repo) — `fbuild port scan`, `fbuild deploy`, autoresearch
```

The on-disk schema is a compact nested protobuf, so the vendor name is stored
once per VID rather than once per product:

```protobuf
message UsbVidDatabase { repeated Vendor vendors = 1; }
message Vendor  { uint32 vid = 1; string name = 2; repeated Product products = 3; }
message Product { uint32 pid = 1; string name = 2; }
```

A single artifact therefore yields **both** projections fbuild needs — VID →
vendor, and VID:PID → {vendor, product} — which is why there is no separate
per-VID blob and no reason for a consumer to keep its own map.

The compressed archive is also fbuild's **fallback** path: when the network
cache is cold or unreachable, fbuild reads the last-ingested
`usb-vids.proto.zstd` from its cache root rather than degrading to a
hardcoded table. Degrading to a local literal is not an available behavior by
design.

## What to do when an identity is missing

1. **Confirm it is actually missing.** Decode the published artifact and look
   the pair up. Do not infer absence from a failed port scan — a cold or stale
   cache looks identical to a missing record.
2. **Add it on the FastLED/boards data branch** (`vendors` or `other` for a
   bare chip or debug probe; `platformio` / `arduino` for a board). The
   `site.yml` workflow republishes the artifact on push.
3. **Let fbuild ingest it** and cut an fbuild release.
4. **Cascade the version into this repo** — see below. Until the pin moves,
   FastLED still resolves against the older artifact.

Never short-circuit steps 1–3 by adding the literal here "temporarily". The
LPC845-BRK and RP2350W entries in `ci/util/port_utils.py` are exactly what a
temporary exception looks like three months later.

## Cascading an fbuild version bump into FastLED

The registry reaches this repo only through the fbuild pin. When fbuild
publishes a release carrying new USB identities:

```bash
# 1. Confirm the release is actually on PyPI before moving the pin.
curl -fsSL https://pypi.org/pypi/fbuild/json | python -c "import sys,json;print(json.load(sys.stdin)['info']['version'])"

# 2. Move the pin in pyproject.toml: fbuild==<old>  →  fbuild==<new>
#    Add a "Pin history" entry saying what the bump carries.

# 3. Refresh the local lockfile and environment.
uv lock
uv sync

# 4. Verify the toolchain actually resolves to the new version.
uv run fbuild --version
```

**`uv.lock` is gitignored in this repo** (`.gitignore` line 52), so the pin in
`pyproject.toml` is the only committed half of the cascade. Do not skip the
relock anyway: without it your local environment keeps running the previous
wheel and any verification you do is against the old registry snapshot.

## Migration status (audited 2026-08-22)

Every VID:PID literal in this repo's `ci/` tree was checked against the live
`usb-vids.proto.zstd` (36 vendors / 1056 products). **14 of 15 resolve from the
registry**:

| VID:PID | Identity | Registry |
| --- | --- | --- |
| `2E8A:000A` | rp2040 / rpipico application CDC | resolved |
| `2E8A:F00A` | rpipicow application CDC | resolved |
| `2E8A:000F` | rp2350 / rpipico2 application CDC | resolved |
| `2E8A:F00F` | rp2350w / rpipico2w application CDC | resolved |
| `2E8A:0003` | RP2 ROM BOOTSEL | resolved |
| `16C0:0483` | LPCXpresso VCOM (LPC11U35) | resolved |
| `16C0:0486` | PJRC / Teensy USB-Serial (alt) | resolved |
| `1FC9:0132` | NXP LPC-Link2 CMSIS-DAP | resolved |
| `303A:1001` | Espressif native USB CDC | resolved |
| `10C4:EA60` | Silicon Labs CP2102 | resolved |
| `1A86:7523` | WCH CH340 | resolved |
| `1A86:55D4` | WCH CH343 | resolved |
| `0403:6001` | FTDI FT232R | resolved |
| `0403:6010` | FTDI FT2232 | resolved |
| `0403:6014` | FTDI FT232H | **GAP** — FastLED/boards#60 |

The FTDI vendor entry exists but enumerates only `6001` and `6010`; `6014` is
absent registry-wide. It is the sole blocker to deleting the local tables
outright rather than freezing them. Do not close that gap by keeping the
literal — track it in FastLED/boards#60.

Re-run the audit at any time:

```bash
uv run python ci/util/audit_usb_registry.py
```

It fetches the live artifact, decodes it, and exits non-zero while any
audited literal is still unresolvable. A cold or stale cache is
indistinguishable from a missing record, so always check the published
payload rather than a failed port scan. `AUDITED_LITERALS` in that script is
the checklist — it should shrink to empty as literals are retired.

## Existing local tables (legacy, do not extend)

| Location | Status |
| --- | --- |
| `ci/util/port_utils.py` → `ENVIRONMENT_TO_VCOM_VID_PIDS` | Legacy. Frozen — every pair is already published in FastLED/boards. Do not add entries; see FastLED#3836. |
| `ci/util/serial_probe.py` → `BOARD_FINGERPRINTS` | Legacy display/diagnostic hints only. Never a selection authority. Do not add entries. |
| `ci/tests/**` | Test fixtures may use concrete literals to exercise parsing and selection. They must not become runtime defaults. |

Tests are the one sanctioned home for concrete USB literals in this repo —
same carve-out fbuild grants its `#[cfg(test)]` modules.

## Related

- fbuild's mirror of this rule: `CLAUDE.md` → "USB VID/PID source of truth",
  and `docs/usb-vidpid-audit.md` for the per-module migration audit.
- `agents/docs/hardware-autoresearch.md` → board bring-up workflow.
- `agents/docs/build-system.md` → "Deployment (flash / upload) is fbuild's job".

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

```text
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

**Push it to FastLED/boards. Immediately. We own that repo — this is the
standard way to get support, not an escalation.** Do not file an issue and
wait, and do not add a local literal "for now".

The whole round trip is one commit and a few minutes:

```bash
# 1. Confirm it is actually missing (do not infer absence from a failed
#    port scan — a cold cache looks identical to a missing record).
uv run python ci/util/audit_usb_registry.py

# 2. Add the record on the `other` data branch of FastLED/boards.
git clone https://github.com/FastLED/boards.git .extern-repos/boards
cd .extern-repos/boards
git worktree add ../boards-other origin/other -b curate/<topic>
```

Curated records are flat JSON arrays — one file per topic, alongside the
existing `nxp_debug_probe_pids.json` / `teensy_pids.json`:

```json
[
  { "_comment": "Why these exist, when, and for which issue.",
    "_skip": "ignored by extract_other.py (only records with vid+pid+product are ingested)" },
  { "vid": "0403", "pid": "6014",
    "product": "FTDI FT232H (single-channel Hi-Speed USB to UART/FIFO)",
    "_source": "Where this identity came from and what ships it." }
]
```

```bash
# 3. Verify the extractor ingests it BEFORE pushing.
uv run --no-project python builders/extract_other.py --in ../boards-other --out /tmp/other.json

# 4. Push straight to the data branch, then rebuild the site.
git push origin curate/<topic>:other
gh workflow run "Build site" -R FastLED/boards

# 5. Re-run the audit once the rebuild completes (~2-4 min).
uv run python ci/util/audit_usb_registry.py
```

`site.yml` triggers on any push to a data branch, with a nightly backstop and
a `workflow_dispatch` override — dispatch it rather than waiting if the push
trigger does not fire.

**Do you also need an fbuild bump?** Usually no. fbuild fetches the published
catalogue at runtime, so a boards push reaches every consumer on the next cache
refresh without a release. Bump fbuild only when the *ingestion or resolution
logic* changes, not when data changes. That is the "push boards, or push boards
and fbuild" split.

Never add the literal here "temporarily". The LPC845-BRK and RP2350W entries in
`ci/util/port_utils.py` are exactly what a temporary exception looks like three
months later.

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

**`uv.lock` is gitignored in this repo**, so the pin in
`pyproject.toml` is the only committed half of the cascade. Do not skip the
relock anyway: without it your local environment keeps running the previous
wheel and any verification you do is against the old registry snapshot.

## Migration status (audited 2026-08-22)

Every VID:PID literal in this repo's `ci/` tree resolves from the published
registry — **15 of 15**:

| VID:PID | Identity |
| --- | --- |
| `2E8A:000A` | rp2040 / rpipico application CDC |
| `2E8A:F00A` | rpipicow application CDC |
| `2E8A:000F` | rp2350 / rpipico2 application CDC |
| `2E8A:F00F` | rp2350w / rpipico2w application CDC |
| `2E8A:0003` | RP2 ROM BOOTSEL |
| `16C0:0483` | LPCXpresso VCOM (LPC11U35) |
| `16C0:0486` | PJRC / Teensy USB-Serial (alt) |
| `1FC9:0132` | NXP LPC-Link2 CMSIS-DAP |
| `303A:1001` | Espressif native USB CDC |
| `10C4:EA60` | Silicon Labs CP2102 |
| `1A86:7523` | WCH CH340 |
| `1A86:55D4` | WCH CH343 |
| `0403:6001` | FTDI FT232R |
| `0403:6010` | FTDI FT2232 |
| `0403:6014` | FTDI FT232H |

The audit initially found 14/15. The one gap — FTDI FT232H `0403:6014`, absent
registry-wide — was closed the way this doc prescribes: one commit to the
`other` data branch of FastLED/boards, site rebuild, done (FastLED/boards#60).
That is the worked example. Total elapsed time was a few minutes, which is why
the procedure above says push rather than file.

Nothing local is now load-bearing: every literal in `ci/` is redundant with the
registry and can be deleted once its call site routes through fbuild.

Re-run the audit at any time:

```bash
uv run python ci/util/audit_usb_registry.py
```

It fetches the published catalogue and exits non-zero if any audited literal
stops resolving. `AUDITED_LITERALS` in that script is the checklist — it should
shrink to empty as literals are retired.

The script reads `usb-ids.json`, the plain-JSON projection of the catalogue,
and needs nothing outside the standard library. fbuild reads
`usb-vids.proto.zstd` instead because it wants the compact binary form; both
endpoints carry identical data, so there is no reason for a Python consumer to
take on a zstd dependency and a protobuf decoder to read it.

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

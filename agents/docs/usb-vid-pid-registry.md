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
before this rule was written: FastLED's now-retired CI tables and fbuild's
former generated Rust tables all carried overlapping,
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

**`uv.lock` is gitignored in this repo**, so the pin in
`pyproject.toml` is the only committed half of the cascade. Do not skip the
relock anyway: without it your local environment keeps running the previous
wheel and any verification you do is against the old registry snapshot.

## Migration status

The legacy runtime VID:PID tables were retired after all 15 identities resolved
from FastLED/boards. The catalogue fetch/decode utility remains for the exact
VID:PID query mode tracked in FastLED #3996.

Check an observed identity against the published catalogue directly:

```bash
uv run python ci/util/audit_usb_registry.py --lookup <VID:PID>
```

The lookup exits 0 and names the vendor/product when published, or exits 1 and
reports `MISSING` when the exact pair is absent.

## Runtime ownership

| Location | Status |
| --- | --- |
| `fbuild deploy` | Environment-aware port selection, including boards with multiple accepted probe identities. |
| `fbuild port scan` | FastLED/boards-backed vendor/product diagnostics. |
| `ci/tests/**` | Test fixtures may use concrete literals to exercise parsing and selection. They must not become runtime defaults. |

Tests are the one sanctioned home for concrete USB literals in this repo —
same carve-out fbuild grants its `#[cfg(test)]` modules.

## Related

- fbuild's mirror of this rule: `CLAUDE.md` → "USB VID/PID source of truth",
  and `docs/usb-vidpid-audit.md` for the per-module migration audit.
- `agents/docs/hardware-autoresearch.md` → board bring-up workflow.
- `agents/docs/build-system.md` → "Deployment (flash / upload) is fbuild's job".

# CI Utilities

Shared Python utilities for the FastLED CI/build system.

## USB VID/PID identities are not owned here

`port_utils.py` and `serial_probe.py` each carry a legacy VID:PID table. Both
are **frozen**. USB board identity is owned by
[FastLED/boards](https://github.com/FastLED/boards) and reaches this repo
through fbuild's ingestion of the published `usb-vids.proto.zstd` archive.

Do not add entries to either table. If a board cannot be identified, push the
record straight to FastLED/boards — we own it, so that is the normal fix, not
an escalation. One commit to the `other` data branch plus a site rebuild; it
takes minutes. fbuild picks the new data up on its next cache refresh, so a
version bump is usually unnecessary.

Check the migration status at any time:

```bash
uv run python ci/util/audit_usb_registry.py
```

All 15 literals currently resolve upstream, so this exits 0 and is safe to wire
into CI. Full procedure: `agents/docs/usb-vid-pid-registry.md`.

Full rule and cascade procedure: `agents/docs/usb-vid-pid-registry.md`.

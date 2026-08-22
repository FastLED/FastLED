# CI Utilities

Shared Python utilities for the FastLED CI/build system.

## USB VID/PID identities are not owned here

`port_utils.py` and `serial_probe.py` each carry a legacy VID:PID table. Both
are **frozen**. USB board identity is owned by
[FastLED/boards](https://github.com/FastLED/boards) and reaches this repo
through fbuild's ingestion of the published `usb-vids.proto.zstd` archive.

Do not add entries to either table. If a board cannot be identified, add the
record upstream, let fbuild ingest it, then cascade the `fbuild==X.Y.Z` pin in
`pyproject.toml` and relock locally (`uv lock && uv sync`; `uv.lock` itself is
gitignored here).

Check the migration status at any time:

```bash
uv run python ci/util/audit_usb_registry.py
```

Full rule and cascade procedure: `agents/docs/usb-vid-pid-registry.md`.

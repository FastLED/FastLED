# CI Utilities

Shared Python utilities for the FastLED CI/build system.

## USB VID/PID identities are not owned here

USB board identity is owned by
[FastLED/boards](https://github.com/FastLED/boards) and reaches this repo
through fbuild's ingestion of the published `usb-vids.proto.zstd` archive.

Use `fbuild port scan` for registry-backed USB diagnostics and let `fbuild
deploy` select a port for a known environment. If a board cannot be identified,
add the record upstream, let fbuild ingest it, then cascade the `fbuild==X.Y.Z`
pin in `pyproject.toml` and relock locally (`uv lock && uv sync`; `uv.lock`
itself is gitignored here).

`ci/util/audit_usb_registry.py --lookup <VID:PID>` retains the exact catalogue
query tracked in FastLED #3996. The script no longer has an audit mode or local
retirement checklist because the runtime tables have been removed.

Full rule and cascade procedure: `agents/docs/usb-vid-pid-registry.md`.

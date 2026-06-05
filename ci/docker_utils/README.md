# FastLED Docker Helpers

This directory hosts Docker helpers for FastLED's **emulator** and **test-runner** workflows. The earlier compilation-Docker system (the `niteris/fastled-compiler-*` image family on Docker Hub plus the local `fastled-platformio-*` images) was decommissioned in #2812 — fbuild does not self-poison the way PlatformIO did, so the per-platform compiler images were no longer earning their cost.

## What's here now

### Emulator Docker (kept)

- **`Dockerfile.avr8js`** — AVR8JS-based emulator image used by `avr8js_uno_test.yml` to run Arduino Uno firmware in JavaScript.
- **`avr8js/`** — Node packages + entrypoint scripts baked into the avr8js image.
- **`avr8js_docker.py`** — Python runner that invokes the avr8js image.
- **`qemu_esp32_docker.py`** — Qemu-based ESP32 emulator runner used by the `qemu_esp32*_test.yml` workflows.
- **`README.qemu.md`** — qemu-specific usage notes.

### Test-runner Docker (kept)

- **`container_db.py`** — SQLite-backed container lifecycle tracking. Used by `ci/runners/docker_runner.py` (the `bash test --docker` path) to keep containers alive between runs and clean up after dead processes.
- **`cleanup_orphans.py`** — standalone CLI for purging orphaned tracked containers (`uv run python -m ci.docker.cleanup_orphans`).
- **`test_container_db.py`** — unit tests for `container_db.py`.

### Shared utilities (kept, possibly pruned)

- **`build_image.py`** — image-name hashing. After #2812 only `generate_config_hash()` and `get_platformio_version()` remain (used by the qemu runner). The compile-Docker helpers (`extract_architecture`, `generate_docker_tag`) were removed.
- **`DockerManager.py`** — generic `docker run` / `docker stop` subprocess wrapper. Retained because `qemu_esp32_docker.py` calls `run_container_streaming()` on it. The compile-side orchestrator that also used to call in here is gone.
- **`build_platforms.py`** — board → platform-family mapping. Retained because `qemu_esp32_docker.py` calls `get_platform_for_board()` to derive `niteris/fastled-simulator-*` image names. The mirror compiler-image names that historically used this mapping are no longer published.

### What was deleted in #2812

| Removed | Why |
|---|---|
| `Dockerfile.base`, `Dockerfile.template` | Built the `niteris/fastled-compiler-base` and per-platform compiler images. |
| `build.sh`, `docker_build_utils.py`, `generate_platformio_ini.py` | In-container layered build orchestration. |
| `image_priority_tracker.py` | Smart-rebuild scheduler that drove `docker_compiler_smart_build.yml`. |
| `prune_old_images.py` | Pruned local `fastled-platformio-*` images (image family no longer exists). |
| `diagnose_sync.sh` | Diagnosed rsync sync between host and compile-Docker container. |
| `DOCKER_DESIGN.md`, `task.md`, `RSYNC_FIX.md` | Architecture / design notes for the retired system. |

The matching workflows (`.github/workflows/docker_compiler_*.yml`, `docker_merge_platform.yml`, `docker_build_compiler.yml`) and the compile-side Python wrapper (`ci/compiler/docker_manager.py`, `ci/build_docker_image_pio.py`) were removed in the same PR. The `--docker` and `--build` flags on `bash compile` are gone.

`bash test --docker` and `bash profile --docker` still work today against the published Docker Hub images, but will be audited and retired separately (#2812 Phase 6 — Docker Hub image deprecation).

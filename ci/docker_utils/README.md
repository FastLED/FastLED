# FastLED Docker Helpers

This directory hosts the **AVR8JS emulator** Docker helper only. All other Docker
paths for FastLED — per-platform compilation, per-platform ESP32 QEMU
simulation, and the host unit-tests image — have been retired. fbuild is the
default and only compile backend; `fbuild test-emu --emulator qemu` drives ESP32
QEMU natively.

## What's here now

- **`Dockerfile.avr8js`** — AVR8JS-based emulator image used by
  `.github/workflows/avr8js_uno_test.yml` to run Arduino Uno / ATtiny firmware
  in JavaScript.
- **`avr8js/`** — Node packages + entrypoint scripts baked into the avr8js
  image.
- **`avr8js_docker.py`** — Python runner (`DockerAVR8jsRunner`) that pulls +
  invokes the avr8js image. Used from CI and from the local
  `ci.runners.avr8js_runner.run_avr8js_tests` entrypoint (`bash test --run uno`).

## What was retired

The following used to live here and are now gone:

| Removed | Killed in | Why |
|---|---|---|
| `Dockerfile.base`, `Dockerfile.template` | #2812 | Built `niteris/fastled-compiler-base` and per-platform compiler images. |
| `build.sh`, `docker_build_utils.py`, `generate_platformio_ini.py` | #2812 | In-container layered build orchestration. |
| `image_priority_tracker.py` | #2812 | Smart-rebuild scheduler that drove `docker_compiler_smart_build.yml`. |
| `prune_old_images.py` | #2812 | Pruned local `fastled-platformio-*` images. |
| `diagnose_sync.sh` | #2812 | Diagnosed rsync sync between host and compile-Docker container. |
| `DOCKER_DESIGN.md`, `task.md`, `RSYNC_FIX.md` | #2812 | Architecture notes for the retired compile-Docker system. |
| `qemu_esp32_docker.py`, `qemu_test_integration.py`, `README.qemu.md` | this sweep | Docker-based ESP32 QEMU runner. CI now runs `fbuild test-emu --emulator qemu` directly. |
| `build_platforms.py` | this sweep | `DOCKER_PLATFORMS` board → platform-family mapping — only ever used to derive the retired `niteris/fastled-simulator-*` image names. |
| `build_image.py` | this sweep | Image-name hashing — only consumer was the retired QEMU Docker runner. |
| `DockerManager.py` | this sweep | Generic `docker run` / `docker stop` subprocess wrapper — only consumer was the retired QEMU Docker runner. |
| `container_db.py`, `cleanup_orphans.py`, `test_container_db.py` | this sweep | SQLite-backed container lifecycle tracking for `bash test --docker`. |
| `ci/runners/qemu_runner.py`, `ci/install-qemu.py` | this sweep | Local QEMU test entrypoints that pulled and executed the retired simulator images. |
| `ci/runners/docker_runner.py`, `docker/unit-tests/` | this sweep | Host `fastled-unit-tests` image + `bash test --docker` wrapper. |

The matching workflows (`.github/workflows/docker_compiler_*.yml`,
`docker_merge_platform.yml`, `docker_build_compiler.yml`) and the compile-side
Python wrapper (`ci/compiler/docker_manager.py`, `ci/build_docker_image_pio.py`)
were removed in #2812. `bash compile --docker`, `bash compile --build`, `bash
test --docker`, `bash test --qemu`, and `bash profile --docker` are all gone.
Callgrind now requires a Linux host (WSL2 on Windows) — the Windows path used to
be to run callgrind inside `fastled-unit-tests`, but that image is gone too.

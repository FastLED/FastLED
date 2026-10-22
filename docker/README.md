# FastLED Docker Images

Docker is used for the VS Code development container. fbuild owns board
compilation and native emulator execution, including AVR8JS.

## Available Images

### 1. Legacy AVR8JS Emulator (`ci/docker_utils/`)

Retained for manual use. The Uno AVR8JS workflow and `bash test --run uno`
use native fbuild and do not pull or build this image.

- `ci/docker_utils/Dockerfile.avr8js` — image definition.
- `ci/docker_utils/avr8js_docker.py` — Python runner (`DockerAVR8jsRunner`).

See `ci/docker_utils/README.md` for the runner API.

### 2. VS Code DevContainer (`.devcontainer/`)

Development container for VS Code with Python, fbuild, the SDL runtime, and
build tools. fbuild downloads and manages Espressif QEMU on first use; the
container does not supply a separate QEMU installation. Open the project in VS
Code and select "Reopen in Container" when prompted.

## When to Use Which Image

| Use Case | How |
|----------|-------|
| Cross-compile for Arduino / ESP32 / etc | `bash compile <board>` (native fbuild, no Docker) |
| Run C++ unit tests | `bash test --cpp` (native, no Docker) |
| Run AVR8JS emulator (Uno) | `bash test --run uno Test` (native fbuild, no Docker) |
| VS Code remote development | `.devcontainer/` |

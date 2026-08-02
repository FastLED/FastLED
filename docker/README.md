# FastLED Docker Images

Docker is used only for the AVR8JS emulator image and the VS Code development
container. fbuild owns board compilation and native emulator execution.

## Available Images

### 1. AVR8JS Emulator (`ci/docker_utils/`)

Runs Arduino AVR firmware in a JavaScript AVR simulator. Used by the `uno AVR8JS
Test` GitHub Actions workflow (`.github/workflows/avr8js_uno_test.yml`) to
validate Uno / ATtiny sketches without hardware.

- `ci/docker_utils/Dockerfile.avr8js` — image definition.
- `ci/docker_utils/avr8js_docker.py` — Python runner (`DockerAVR8jsRunner`).

See `ci/docker_utils/README.md` for the runner API.

### 2. VS Code DevContainer (`.devcontainer/`)

Development container for VS Code with Python, QEMU, and build tools
pre-installed. Open the project in VS Code and select "Reopen in Container" when
prompted.

## When to Use Which Image

| Use Case | How |
|----------|-------|
| Cross-compile for Arduino / ESP32 / etc | `bash compile <board>` (native fbuild, no Docker) |
| Run C++ unit tests | `bash test --cpp` (native, no Docker) |
| Run AVR8JS emulator (Uno / ATtiny) | `ci/docker_utils/` (CI-driven; local via `ci.docker_utils.avr8js_docker.DockerAVR8jsRunner`) |
| VS Code remote development | `.devcontainer/` |

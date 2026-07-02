# FastLED Docker Images

Docker is no longer used to build FastLED for any target — fbuild drives all
platform compiles and ESP32 QEMU emulation natively, and the host unit-test
Docker image was retired along with the platform-Docker sweep. Only two Docker
touchpoints remain in this repo:

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

## What was retired

| Family | Killed in | Replacement |
|---|---|---|
| `niteris/fastled-compiler-*` (per-platform PIO compile images) | #2812 | `bash compile <board>` — native fbuild |
| `niteris/fastled-simulator-*` (per-platform ESP32 QEMU images) | this sweep | `uv run fbuild test-emu --emulator qemu ...` — fbuild auto-downloads the Espressif QEMU binary |
| `docker/unit-tests/` + `fastled-unit-tests` (host unit tests image) | this sweep | `bash test --cpp` — native Meson |
| `bash test --docker`, `bash profile --docker` | this sweep | Native builds. Callgrind requires a Linux host (WSL2 works). |
| `bash compile --docker`, `bash compile --build` | #2812 | Removed with the compile images. |

## When to Use Which Image

| Use Case | How |
|----------|-------|
| Cross-compile for Arduino / ESP32 / etc | `bash compile <board>` (native fbuild, no Docker) |
| Run C++ unit tests | `bash test --cpp` (native, no Docker) |
| Run ESP32 QEMU emulator | `uv run fbuild test-emu --emulator qemu --environment <env> .build/pio/<env>` (native, no Docker) |
| Run AVR8JS emulator (Uno / ATtiny) | `ci/docker_utils/` (CI-driven; local via `ci.docker_utils.avr8js_docker.DockerAVR8jsRunner`) |
| VS Code remote development | `.devcontainer/` |

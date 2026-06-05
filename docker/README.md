# FastLED Docker Images

This directory contains Docker configurations for various FastLED development workflows.

## Available Images

### 1. Unit Tests (`docker/unit-tests/`)

Ubuntu-based image for running C++ unit tests with clang and cmake.

**Build:**
```bash
cd docker/unit-tests
./build.sh build
```

**Run tests:**
```bash
./build.sh run
```

**Interactive shell:**
```bash
./build.sh shell
```

**Direct Docker commands:**
```bash
# From project root
docker build -f docker/unit-tests/Dockerfile -t fastled-unit-tests .
docker run --rm -v "$(pwd):/fastled" fastled-unit-tests bash -c "uv run test.py --cpp"
```

### 2. Emulators (`ci/docker_utils/`)

Docker images for hardware emulation:

- `Dockerfile.avr8js` — AVR8JS simulator for Arduino Uno emulation (used by `uno AVR8JS Test` workflow).
- qemu helpers — ESP32 qemu runner for `qemu_esp32*_test.yml` workflows.

See `ci/docker_utils/README.md` for detail.

> **Note**: The PlatformIO cross-compilation Docker images (`niteris/fastled-compiler-*`) that used to live here were decommissioned in #2812 — fbuild is now the default compile backend and does not have the PlatformIO self-poisoning behavior the compiler images were designed to work around.

### 3. VS Code DevContainer (`.devcontainer/`)

Development container for VS Code with Python, QEMU, and build tools pre-installed.

Open the project in VS Code and select "Reopen in Container" when prompted.

## When to Use Which Image

| Use Case | Image |
|----------|-------|
| Run unit tests in clean environment | `docker/unit-tests/` |
| Run AVR8JS / ESP32 qemu emulator | `ci/docker_utils/` (driven by CI) |
| VS Code remote development | `.devcontainer/` |
| Cross-compile for Arduino/ESP32/etc | Native — `bash compile <board>` (no Docker; fbuild backend). |

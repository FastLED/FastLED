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

### 2. PlatformIO Compilation (`ci/docker_utils/`)

Production Docker images for cross-platform compilation via PlatformIO. Used by CI and `bash compile --docker`.

- `Dockerfile.base` - Base image with PlatformIO, UV, and dependencies
- `Dockerfile.template` - Platform-specific image with pre-cached toolchains
- `Dockerfile.avr8js` - AVR8JS simulator for Arduino Uno emulation

See `ci/docker_utils/README.md` for detailed documentation.

### 3. VS Code DevContainer (`.devcontainer/`)

Development container for VS Code with Python, QEMU, and build tools pre-installed.

Open the project in VS Code and select "Reopen in Container" when prompted.

## When to Use Which Image

| Use Case | Image |
|----------|-------|
| Run unit tests in clean environment | `docker/unit-tests/` |
| Cross-compile for Arduino/ESP32/etc | `ci/docker_utils/` (via `bash compile --docker`) |
| VS Code remote development | `.devcontainer/` |
| CI/CD workflows | `ci/docker_utils/` |

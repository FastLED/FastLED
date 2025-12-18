# FastLED PlatformIO Docker Build Systems

FastLED provides **two complementary Docker build systems**:

1. **Local Development Images** (this document) - For developers building locally
2. **CI/CD Published Images** (see [DOCKER_DESIGN.md](DOCKER_DESIGN.md)) - Pre-built images on Docker Hub

---

## Local Development Images

**Local development tool for cross-platform compilation with pre-cached dependencies.**

This Docker-based build system enables fast, reproducible compilation of FastLED sketches across multiple platforms without installing platform-specific toolchains on your host machine.

## Overview

The system uses **Option 1: Bake-In Dependencies** architecture:
- Separate Docker image per platform with pre-installed dependencies
- Hash-based image naming for automatic cache invalidation
- Lightning-fast runtime (zero dependency downloads)
- Stateless containers (use `--rm` flag)

## Image Naming Convention

```
fastled-platformio-{architecture}-{platform}-{hash}
```

Example: `fastled-platformio-esp32-esp32s3-a1b2c3d4`

The hash is automatically generated from:
- Platform configuration (e.g., `espressif32`)
- Framework (e.g., `arduino`)
- Board identifier (e.g., `esp32-s3-devkitc-1`)
- Platform packages/URLs
- PlatformIO version

**Cache invalidation is automatic**: When platform config changes (e.g., ESP32 SDK link update in `ci/boards.py`), a new hash is generated and a new image is built.

## Quick Start

### 1. Build a Docker Image

```bash
# Build image for Arduino Uno
uv run python ci/build_docker_image_pio.py --platform uno

# Build image for ESP32-S3
uv run python ci/build_docker_image_pio.py --platform esp32s3 --framework arduino

# Build with custom platformio.ini
uv run python ci/build_docker_image_pio.py --platformio-ini ./custom-config.ini

# Force rebuild without cache
uv run python ci/build_docker_image_pio.py --platform uno --no-cache
```

### 2. Run Compilation in Container

**Recommended:** Use the `bash compile` command with `--docker` flag for automatic setup:

```bash
# Simple compilation with automatic Docker detection
bash compile esp32dev Blink --docker

# Compile with output directory (automatically mounted as volume)
bash compile esp32dev Blink --docker -o ./build_output

# Artifacts appear immediately in ./build_output on host
```

**Advanced:** Manual Docker commands for custom workflows:

The Docker image contains a snapshot of FastLED from GitHub. To compile your local code, rsync your repo and run the same `compile` command:

```bash
# The pattern: bash compile <platform> <example> --docker
# Host command:      bash compile esp32dev Blink --docker
# Container command: bash compile esp32dev Blink (mirrors host, minus --docker flag)

# Compile with output directory
docker run --rm \
  -v $(pwd):/host:ro \
  -v ./build_output:/fastled/output:rw \
  fastled-platformio-esp32-esp32dev-abc123 \
  bash -c "rsync -a /host/ /fastled/ && bash compile esp32dev Blink"

# Compile without output directory (shorter example)
docker run --rm \
  -v $(pwd):/host:ro \
  fastled-platformio-avr-uno-abc123 \
  bash -c "rsync -a /host/ /fastled/ && bash compile uno Blink"

# Interactive shell for debugging
docker run --rm -it \
  -v $(pwd):/host:ro \
  fastled-platformio-avr-uno-abc123 \
  bash
# Inside container:
# rsync -a /host/ /fastled/
# bash compile uno Blink
```

## Volume Mounts

### Automatic Output Directory Mounting (with -o flag)

**NEW:** When using the `-o` or `--out` flag with `bash compile`, the output directory is automatically mounted as a volume:

```bash
# Automatically mounts ./build_output as /fastled/output in container
bash compile esp32dev Blink --docker -o ./build_output

# Artifacts appear directly in ./build_output on host
# - firmware.bin
# - firmware.elf
# - firmware.factory.bin (ESP32 merged binary)
```

**How it works:**
1. Output directory is validated (must be within project root or current directory)
2. Directory is created if it doesn't exist
3. Mounted as `/fastled/output:rw` in container
4. Build artifacts automatically copied after successful compilation
5. No need for post-compilation `docker cp`

**Security:** Output directory must be relative to current directory or a subdirectory. Absolute paths outside the project are rejected.

### Manual Pattern (Advanced)

For custom Docker usage, you can manually mount volumes:

- `-v $(pwd):/host:ro` - Mount your FastLED repo as read-only
  - Use `rsync -a /host/ /fastled/` inside container to update code
  - Read-only mount keeps host filesystem safe
  - Rsync handles efficient updates (only changed files)

- `-v ./build_output:/fastled/output:rw` - Output directory for compiled binaries

When `/fastled/output` is mounted, the entrypoint automatically copies build artifacts after successful compilation:
- `*.bin` - Binary files (ESP32, STM32, etc.)
- `*.hex` - Hex files (AVR platforms)
- `*.elf` - ELF files (debugging symbols)
- `*.factory.bin` - Merged binaries for ESP32 (includes bootloader + partitions + app)

**Key Insight:** `/fastled/` inside the container starts with a GitHub snapshot, then gets updated via rsync with your local code. The `compile` script works identically inside and outside the container.

## Build Modes

### Mode A: Generate from Board Configuration

Uses platform configuration from `ci/boards.py`:

```bash
uv run python ci/build_docker_image_pio.py --platform uno
uv run python ci/build_docker_image_pio.py --platform esp32s3 --framework arduino
```

**Advantages:**
- Consistent with existing CI/build infrastructure
- Automatic hash generation for cache invalidation
- Validated board configurations

### Mode B: Use Existing platformio.ini

Uses a custom `platformio.ini` file:

```bash
uv run python ci/build_docker_image_pio.py --platformio-ini ./custom-config.ini
```

**Use cases:**
- Testing custom platform configurations
- Advanced PlatformIO features
- One-off builds

**Note:** Hash-based naming is not available in Mode B (uses generic image name).

## Image Management

### List Images

```bash
# List all FastLED PlatformIO images
docker images fastled-platformio-*

# Check image details
docker image inspect fastled-platformio-avr-uno-abc123
```

### Cleanup Old Images

The `prune_old_images.py` script removes outdated images:

```bash
# Dry run (show what would be deleted)
uv run python ci/docker_utils/prune_old_images.py

# Actually delete old images (keeps newest for each platform)
uv run python ci/docker_utils/prune_old_images.py --force

# Remove images older than 30 days
uv run python ci/docker_utils/prune_old_images.py --days 30 --force

# Remove images for specific platform only
uv run python ci/docker_utils/prune_old_images.py --platform esp32s3 --force

# Remove ALL FastLED PlatformIO images (dangerous!)
uv run python ci/docker_utils/prune_old_images.py --all --force
```

**Default behavior** (without `--days` or `--all`):
- Keeps the newest image for each platform/architecture combination
- Deletes older images with different config hashes
- Safe for regular cleanup

### Manual Cleanup

```bash
# Remove specific image
docker rmi fastled-platformio-avr-uno-abc123

# Remove all FastLED images (use with caution!)
docker rmi $(docker images -q fastled-platformio-*)

# Remove unused Docker images (generic cleanup)
docker image prune -a --filter "until=720h"  # Remove images older than 30 days
```

## Architecture

### Build Time (Image Creation)

**Input:**
- Platform name (e.g., `uno`, `esp32s3`) → reads from `ci/boards.py`
- Optional framework override

**Process:**
1. Generate `platformio.ini` from board configuration
2. Build base image (if needed) with PlatformIO and Python dependencies
3. Build platform-specific image with toolchains and dependencies
4. Pre-compile Blink sketch to cache all dependencies
5. Clean build artifacts but preserve PlatformIO cache

**Output:**
- Docker image: `fastled-platformio-{architecture}-{platform}-{hash}`
- Pre-cached dependencies in `~/.platformio/packages` and `~/.platformio/platforms`

### Runtime (Compilation)

**Input:**
- Platform name + sketch name ONLY
- Source code volume-mounted from host

**Process:**
1. Mount source directories (read-only)
2. Optionally mount output directory (read-write)
3. Run PlatformIO compilation
4. Copy binaries to output directory (if mounted)

**Output:**
- Compiled binaries in container
- Optionally copied to host via output volume

**Key Insight:** Platform configuration is baked into the image at build time. Runtime only needs source code and compilation command.

## Merged Binary Support (ESP32)

For ESP32 platforms, the system automatically supports merged binary generation:

**What is a merged binary?**
- Single flashable `.factory.bin` file
- Combines bootloader + partition table + application
- Simplifies flashing process (one file instead of three)

**Generation:**
PlatformIO automatically creates merged binaries at:
```
.pio/build/<env>/firmware.factory.bin
```

**Access merged binaries:**
1. Use output directory mount:
   ```bash
   docker run --rm \
     -v ./src:/fastled/src:ro \
     -v ./examples:/fastled/examples:ro \
     -v ./build_output:/fastled/output:rw \
     fastled-platformio-esp32-esp32s3-abc123 \
     pio run
   ```

2. Check `./build_output/firmware.factory.bin` on host

## Supported Platforms

The system supports all platforms defined in `ci/boards.py`:

### AVR
- `uno`, `nano_every`, `leonardo`
- `attiny85`, `attiny88`, `attiny4313`
- `attiny1604`, `attiny1616` (megaAVR)

### ESP32
- `esp32dev`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`
- All ESP32 variants with custom SDK support

### ARM
- `sam3x8e_due` (SAM)
- `stm32f103c8`, `stm32f103cb`, `stm32f103tb`, `stm32f411ce`, `stm32h747xi` (STM32)
- `teensy30`, `teensy40`, `teensy41` (Teensy)
- `rp2040`, `rp2350` (RP2040)
- `nrf52840_dk`, `xiaoblesense` (nRF52)

### Other
- `uno_r4_wifi` (Renesas)
- `apollo3_red` (Apollo3)
- `native` (host compilation)

To see all available platforms:
```bash
uv run python -c "from ci.boards import ALL; print([b.board_name for b in ALL])"
```

## Benefits

### Developer Experience
- **Zero-friction compilation**: Just run and compile instantly
- **No toolchain installation**: All dependencies in Docker
- **Consistent builds**: Exact same environment as CI
- **Cross-platform**: Compile for any platform from any host OS

### Performance
- **Lightning-fast runtime**: Zero dependency downloads
- **Parallel compilation**: Run multiple platforms simultaneously
- **Efficient caching**: Docker layer sharing reduces storage

### Reproducibility
- **Frozen dependencies**: Exact versions in image
- **Automatic cache invalidation**: Hash-based naming
- **Idempotent builds**: Same input → same output

### Maintenance
- **Simple cleanup**: Automated pruning scripts
- **Stateless containers**: No persistent state to manage
- **Local development only**: Not designed for CI (CI uses native tools)

## Troubleshooting

### Docker Not Found

```bash
# Check if Docker is installed
docker --version

# Install Docker Desktop
# https://www.docker.com/products/docker-desktop
```

### Permission Errors

On Linux, you may need to add your user to the `docker` group:
```bash
sudo usermod -aG docker $USER
# Log out and back in for changes to take effect
```

### Build Failures

```bash
# Try rebuilding without cache
uv run python ci/build_docker_image_pio.py --platform uno --no-cache

# Check base image exists
docker images fastled-platformio:latest

# Rebuild base image if needed
# (base image is automatically built by build_docker_image_pio.py)
```

### Compilation Errors

```bash
# Run with interactive shell to debug
docker run --rm -it \
  -v ./src:/fastled/src:ro \
  -v ./examples:/fastled/examples:ro \
  fastled-platformio-avr-uno-abc123 \
  /bin/bash

# Inside container:
pio run --verbose
```

### Output Directory Not Working

Ensure directory is created and has write permissions:
```bash
mkdir -p ./build_output
chmod 777 ./build_output  # Or appropriate permissions
```

Check if output directory is mounted:
```bash
docker run --rm \
  -v ./build_output:/fastled/output:rw \
  fastled-platformio-avr-uno-abc123 \
  ls -la /fastled/output
```

## Advanced Usage

### Custom Image Name

```bash
uv run python ci/build_docker_image_pio.py \
  --platform uno \
  --image-name my-custom-uno-image
```

### Framework Override

```bash
# Some platforms support multiple frameworks
uv run python ci/build_docker_image_pio.py \
  --platform esp32s3 \
  --framework arduino
```

### Inspect PlatformIO Cache

```bash
# Run interactive shell
docker run --rm -it \
  fastled-platformio-avr-uno-abc123 \
  /bin/bash

# Inside container:
ls -la ~/.platformio/packages   # Pre-cached toolchains
ls -la ~/.platformio/platforms  # Pre-cached platforms
```

### Build Specific Sketch

```bash
# Ensure your sketch is in examples/ directory
# Then mount it and run:
docker run --rm \
  -v ./src:/fastled/src:ro \
  -v ./examples:/fastled/examples:ro \
  fastled-platformio-avr-uno-abc123 \
  pio run -e uno
```

## Technical Notes

### Why Option 1 (Bake-In Dependencies)?

We chose **Option 1** over **Option 2** (base image + runtime install) for:

1. **Developer Experience**: Zero-friction, instant compilation
2. **Simplicity**: No container state management
3. **Docker Philosophy**: Ephemeral, reproducible containers
4. **Parallelization**: Multiple platforms compile simultaneously
5. **Storage is cheap**: Disk space < developer time

### Why Hash-Based Naming?

Hash-based naming provides automatic cache invalidation:

**Scenario:** ESP32 SDK URL changes in `ci/boards.py`

**Old approach:**
- Image name stays same: `fastled-platformio-esp32-esp32s3`
- Old image has stale SDK
- Developer must manually rebuild: `--no-cache`
- Risk of using outdated dependencies

**Hash-based approach:**
- SDK change → new hash → new image name
- Old image: `fastled-platformio-esp32-esp32s3-abc123` (stale SDK)
- New image: `fastled-platformio-esp32-esp32s3-def456` (new SDK)
- Build script automatically builds new image
- Developer uses latest image automatically
- Old image can be pruned later

**Benefits:**
- Automatic cache invalidation
- No manual intervention needed
- Safe rollback (old image still available)
- Clear version tracking

### Local Development Only

This Docker system is designed for **local development**, not for CI:

**Why not CI?**
- CI servers already have native toolchains
- Docker overhead not needed in CI
- Native compilation faster than Docker in CI
- CI uses existing `ci/ci-compile.py` infrastructure

**Use cases:**
- Local cross-platform development
- Testing builds on different platforms
- Debugging platform-specific issues
- Development on machines without toolchains

## CI/CD Published Images (Docker Hub)

FastLED also provides **pre-built Docker images on Docker Hub** for CI/CD and production use.

### Quick Start with Published Images

```bash
# Pull and use pre-built images (no local build needed!)
bash compile uno Blink --docker
bash compile esp32s3 Blink --docker
bash compile teensy41 Blink --docker
```

These images are:
- **Multi-board** (AVR, Teensy, etc.): Each image caches toolchains for ALL boards in its platform family
- **Single-board** (ESP): Each ESP board has its own image to prevent build artifact accumulation
- **Multi-arch**: Built for both linux/amd64 and linux/arm64
- **Auto-updated**: Rebuilt daily at 2:00 AM UTC

### Available Published Images

| Platform | Docker Image | Cached Boards |
|----------|--------------|---------------|
| AVR | `niteris/fastled-compiler-avr:latest` | uno, leonardo, attiny85, attiny88, attiny4313, nano_every, attiny1604, attiny1616 |
| ESP32 (Original) | `niteris/fastled-compiler-esp-32dev:latest` | esp32dev |
| ESP32-S2 | `niteris/fastled-compiler-esp-32s2:latest` | esp32s2 |
| ESP32-S3 | `niteris/fastled-compiler-esp-32s3:latest` | esp32s3 |
| ESP8266 | `niteris/fastled-compiler-esp-8266:latest` | esp8266 |
| ESP32-C2 | `niteris/fastled-compiler-esp-32c2:latest` | esp32c2 |
| ESP32-C3 | `niteris/fastled-compiler-esp-32c3:latest` | esp32c3 |
| ESP32-C5 | `niteris/fastled-compiler-esp-32c5:latest` | esp32c5 |
| ESP32-C6 | `niteris/fastled-compiler-esp-32c6:latest` | esp32c6 |
| ESP32-H2 | `niteris/fastled-compiler-esp-32h2:latest` | esp32h2 |
| ESP32-P4 | `niteris/fastled-compiler-esp-32p4:latest` | esp32p4 |
| Teensy | `niteris/fastled-compiler-teensy:latest` | teensylc, teensy30, teensy31, teensy40, teensy41 |
| STM32 | `niteris/fastled-compiler-stm32:latest` | stm32f103c8, stm32f103cb, stm32f103tb, stm32f411ce, stm32h747xi |
| RP | `niteris/fastled-compiler-rp:latest` | rp2040, rp2350 |
| NRF52 | `niteris/fastled-compiler-nrf52:latest` | nrf52840_dk, adafruit_feather_nrf52840_sense, xiaoblesense |
| SAM | `niteris/fastled-compiler-sam:latest` | sam3x8e_due |

### Key Features

- **Flexible caching strategy**:
  - Grouped platforms (AVR, Teensy, etc.): Multi-board images with shared toolchains
  - Flat platforms (ESP): Single-board images to prevent build artifact accumulation
- **Instant compilation**: Cached toolchains mean zero download time during compilation
- **Multi-architecture**: Supports both x86 and ARM hosts (linux/amd64, linux/arm64)
- **Daily updates**: Images rebuild nightly with latest FastLED release

### Example Usage

```bash
# Any AVR board compiles instantly using the same image
bash compile uno Blink --docker
bash compile attiny85 Blink --docker
bash compile nano_every Blink --docker

# All ESP32 variants use the same image
bash compile esp32dev Blink --docker
bash compile esp32s3 Blink --docker
bash compile esp32c6 Blink --docker
```

### Implementation Details

Platform→boards mapping is defined in `ci/docker_utils/build_platforms.py`:

```python
DOCKER_PLATFORMS = {
    "avr": ["uno", "attiny85", "nano_every", ...],
    "esp": ["esp32dev", "esp32s3", "esp32c3", ...],
    # ... etc
}
```

During image build, `ci/docker_utils/build.sh`:
1. Receives platform name (e.g., "uno")
2. Looks up platform family ("avr")
3. Gets all boards for platform
4. Compiles each board to pre-cache toolchains

Result: One Docker image contains toolchains for entire platform family!

### Documentation

For complete details on the CI/CD Docker system:
- **[DOCKER_DESIGN.md](DOCKER_DESIGN.md)** - Architecture, naming conventions, build schedule
- **[build_platforms.py](build_platforms.py)** - Platform→boards mapping (source of truth)

---

## Comparison: Local vs Published Images

| Feature | Local Development | CI/CD Published |
|---------|------------------|-----------------|
| **Purpose** | Development/testing | Production/CI |
| **Location** | Built locally | Docker Hub |
| **Naming** | Hash-based (`fastled-platformio-uno-abc123`) | Platform-level (`niteris/fastled-compiler-avr:latest`) |
| **Container** | Board-specific | Platform-level (`fastled-compiler-avr`) |
| **Boards per image** | One board | Multiple boards (family) |
| **Build trigger** | Manual | Daily (2 AM UTC) |
| **FastLED source** | GitHub release | GitHub release |
| **Cache invalidation** | Automatic (hash) | Time-based (daily) |
| **Best for** | Local cross-platform dev | CI/CD, production builds |

---

## See Also

### Local Development
- `ci/build_docker_image_pio.py` - Build script for local images
- `ci/docker_utils/prune_old_images.py` - Cleanup script for local images

### CI/CD Published Images
- `ci/docker_utils/build_platforms.py` - Platform→boards mapping
- `ci/docker_utils/DOCKER_DESIGN.md` - Complete CI/CD system documentation
- `.github/workflows/build_docker_compiler_*.yml` - GitHub Actions workflows

### Shared
- `ci/docker_utils/Dockerfile.template` - Platform-specific Dockerfile (used by both systems)
- `ci/docker_utils/Dockerfile.base` - Base image Dockerfile (used by both systems)
- `ci/docker_utils/build.sh` - Multi-board build script (used by CI/CD)
- `ci/boards.py` - Platform configurations (used by both systems)
- `ci/AGENTS.md` - CI/build system documentation

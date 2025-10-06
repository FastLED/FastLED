# FastLED PlatformIO Docker Build System

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

```bash
# Basic compilation
docker run --rm \
  -v ./src:/fastled/src:ro \
  -v ./examples:/fastled/examples:ro \
  fastled-platformio-avr-uno-abc123 \
  pio run

# With output directory (saves compiled binaries to host)
docker run --rm \
  -v ./src:/fastled/src:ro \
  -v ./examples:/fastled/examples:ro \
  -v ./build_output:/fastled/output:rw \
  fastled-platformio-avr-uno-abc123 \
  pio run

# Interactive shell for debugging
docker run --rm -it \
  -v ./src:/fastled/src:ro \
  -v ./examples:/fastled/examples:ro \
  fastled-platformio-avr-uno-abc123 \
  /bin/bash
```

## Volume Mounts

### Required (Read-Only)
- `-v ./src:/fastled/src:ro` - FastLED source code
- `-v ./examples:/fastled/examples:ro` - Arduino sketches

### Optional (Read-Write)
- `-v ./build_output:/fastled/output:rw` - Output directory for compiled binaries

When `/fastled/output` is mounted, the container automatically copies build artifacts after compilation:
- `*.bin` - Binary files (ESP32, STM32, etc.)
- `*.hex` - Hex files (AVR platforms)
- `*.elf` - ELF files (debugging symbols)
- `*.factory.bin` - Merged binaries for ESP32 (includes bootloader + partitions + app)

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
uv run python ci/docker/prune_old_images.py

# Actually delete old images (keeps newest for each platform)
uv run python ci/docker/prune_old_images.py --force

# Remove images older than 30 days
uv run python ci/docker/prune_old_images.py --days 30 --force

# Remove images for specific platform only
uv run python ci/docker/prune_old_images.py --platform esp32s3 --force

# Remove ALL FastLED PlatformIO images (dangerous!)
uv run python ci/docker/prune_old_images.py --all --force
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
- `uno`, `nano_every`, `yun`
- `attiny85`, `attiny88`, `attiny4313`
- `ATtiny1604`, `ATtiny1616` (megaAVR)

### ESP32
- `esp32dev`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`
- All ESP32 variants with custom SDK support

### ARM
- `due`, `digix` (SAM)
- `bluepill`, `blackpill`, `giga_r1` (STM32)
- `teensy30`, `teensy40`, `teensy41` (Teensy)
- `rpipico`, `rpipico2` (RP2040)
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

## See Also

- `ci/build_docker_image_pio.py` - Build script source code
- `ci/docker/Dockerfile.template` - Platform-specific Dockerfile
- `ci/docker/Dockerfile.base` - Base image Dockerfile
- `ci/docker/prune_old_images.py` - Cleanup script
- `ci/boards.py` - Platform configurations
- `ci/AGENTS.md` - CI/build system documentation

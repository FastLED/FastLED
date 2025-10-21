# FastLED Docker Compiler Images - Design Document

## Overview

This document describes the architecture and naming conventions for FastLED's pre-built Docker compiler images. These images provide pre-cached PlatformIO toolchains for faster compilation across different embedded platforms.

## Architecture

### Base + Platform Pattern

```
niteris/fastled-compiler-base:latest
    ├── niteris/fastled-compiler-avr-uno:latest
    ├── niteris/fastled-compiler-esp:latest
    ├── niteris/fastled-compiler-teensy-teensy41:latest
    ├── niteris/fastled-compiler-stm32-bluepill:latest
    ├── niteris/fastled-compiler-rp:latest
    ├── niteris/fastled-compiler-nrf52-nrf52840_dk:latest
    └── niteris/fastled-compiler-sam-due:latest
```

### Base Image

**Image**: `niteris/fastled-compiler-base:latest`

**Contents**:
- Python 3.11 on Debian Bookworm
- UV package manager
- PlatformIO core
- Git, build-essential, and common build tools
- FastLED source (from latest release)

**Build Schedule**: Daily at 2:00 AM UTC

**Dockerfile**: `ci/docker/Dockerfile.base`

### Platform Images

**Pattern**: `niteris/fastled-compiler-<platform>-<representative-board>:latest`

**Contents**:
- Inherits from base image
- Pre-cached platform-specific toolchains for **ALL boards** in the platform family
- Pre-warmed compilation cache for multiple boards

**Build Strategy**: Each platform image is built with **multiple board compilations** to pre-cache all toolchains and dependencies for that platform family. For example:
- `avr-uno` image compiles: uno, attiny85, attiny88, nano_every, etc.
- `esp` image compiles: esp32dev, esp32s3, esp32c3, etc.
- `teensy-teensy41` image compiles: teensy30, teensy31, teensy40, teensy41, teensylc
- `sam-due` image compiles: due, digix

This ensures that any board in the platform family can compile instantly without downloading additional toolchains.

**Build Schedule**: Daily at 3:00 AM UTC (1 hour after base)

**Dockerfile**: `ci/docker/Dockerfile.template` (with `PLATFORM_NAME` build arg for initial setup, then multi-board warm-up)

## Platform Naming Strategy

### Platform Families

Platform images are organized by **toolchain family**, not individual boards. Boards that share the same PlatformIO platform and have small toolchain overhead are grouped together.

**Important**: Each platform image contains **pre-cached toolchains for ALL boards** in that family. The "representative board" in the image name is just the naming convention - the actual image supports all listed boards with zero additional downloads.

| Platform Family | Representative Board | Docker Image | Included Boards |
|----------------|---------------------|--------------|-----------------|
| **avr** | uno | `fastled-compiler-avr-uno` | uno, yun, attiny85, attiny88, attiny4313, nano_every, attiny1604, attiny1616 |
| **esp** | (varies by IDF) | `fastled-compiler-esp` | esp32dev, esp32s3, esp32c3, esp32c2, esp32c5, esp32c6, esp32h2, esp32p4, esp32s2, esp8266 |
| **teensy** | teensy41 | `fastled-compiler-teensy-teensy41` | teensy30, teensy31, teensy40, teensy41, teensylc |
| **stm32** | bluepill | `fastled-compiler-stm32-bluepill` | bluepill, blackpill, maple_mini, hy_tinystm103tb, giga_r1 |
| **rp2040** | rp2040 | `fastled-compiler-rp` | rp2040, rp2350 |
| **nrf52** | nrf52840_dk | `fastled-compiler-nrf52-nrf52840_dk` | nrf52840_dk, adafruit_feather_nrf52840_sense, xiaoblesense |
| **sam** | due | `fastled-compiler-sam-due` | due, digix |

### Platform-Board Naming Convention

```
niteris/fastled-compiler-<platform-family>-<board-name>:<tag>
```

**Examples**:
- `niteris/fastled-compiler-avr-uno:latest`
- `niteris/fastled-compiler-esp:latest`
- `niteris/fastled-compiler-teensy-teensy41:latest`

**Rationale**:
- `<platform-family>`: Groups boards by shared toolchain (e.g., `avr`, `esp`, `teensy`)
- `<board-name>`: Representative board for the platform (usually the most common or well-known)
- `<tag>`: Version identifier (currently `:latest`)

## Special Cases

### ESP32 Platform

**Complexity**: ESP32 boards support multiple IDF (Espressif IoT Development Framework) versions with different capabilities.

**Current Strategy**: Single `:latest` tag using current stable IDF from pioarduino

**Future Strategy** (when needed):
```
niteris/fastled-compiler-esp:latest     → Current stable (e.g., idf5.5)
niteris/fastled-compiler-esp:idf5.5     → Explicit IDF 5.5
niteris/fastled-compiler-esp:idf5.4     → Explicit IDF 5.4
niteris/fastled-compiler-esp:idf5.3     → Explicit IDF 5.3
niteris/fastled-compiler-esp:idf4.4     → Explicit IDF 4.4
```

**Platform Sources**:
- **IDF 5.1+**: Pioarduino fork (`pioarduino/platform-espressif32`) - faster updates to new IDF versions
- **IDF 4.4**: Official PlatformIO (`platformio/platform-espressif32`)

**Note**: Pioarduino is an implementation detail and doesn't need to appear in image names. Users care about IDF version, not the source repository.

### Teensy Platform

**Note**: Despite radical differences between Teensy 3.x (Cortex-M4, ~72MHz) and Teensy 4.x (Cortex-M7, 600MHz), the toolchains have small payload and can be aggregated into a single image.

**Image**: `niteris/fastled-compiler-teensy-teensy41:latest`
- Pre-caches ALL Teensy boards: teensylc, teensy30, teensy31, teensy40, teensy41
- Build process compiles example for each board to warm up all toolchains
- Uses representative board `teensy41` in the name (most capable current board)

### AVR Platform

**Note**: Combines classic AVR (`atmelavr`) and modern AVR (`atmelmegaavr`) platforms since they share similar toolchains.

**Image**: `niteris/fastled-compiler-avr-uno:latest`

**Pre-cached boards**:
- Classic AVR (`atmelavr`): uno, yun, attiny85, attiny88, attiny4313
- Modern AVR (`atmelmegaavr`): nano_every, attiny1604, attiny1616

**Build process**: Compiles example for each board listed above to ensure all AVR toolchains are cached in the image.

## Multi-Architecture Support

All images are built for **linux/amd64** and **linux/arm64** using Docker Buildx.

**Build Process**:
1. Build amd64 image on `ubuntu-24.04` runner
2. Build arm64 image on `ubuntu-24.04-arm` runner
3. Merge digests into single multi-arch manifest
4. Push manifest to Docker Hub

## Build Schedule

```
2:00 AM UTC  →  Base image builds
                 ↓
                 (10 minute delay for registry propagation)
                 ↓
2:10 AM UTC  →  Platform images build in parallel (each compiles multiple boards)
                 ├── avr-uno (builds: uno, attiny85, attiny88, nano_every, etc.)
                 ├── esp (builds: esp32dev, esp32s3, esp32c3, esp32c6, etc.)
                 ├── teensy-teensy41 (builds: teensy30, teensy31, teensy40, teensy41, teensylc)
                 ├── stm32-bluepill (builds: bluepill, blackpill, maple_mini, etc.)
                 ├── rp (builds: rp2040, rp2350)
                 ├── nrf52-nrf52840_dk (builds: nrf52840_dk, adafruit boards, xiaoblesense)
                 └── sam-due (builds: due, digix)
```

**Scheduling**:
- Base image: Daily at 2:00 AM UTC via cron
- Platform images: Triggered by base image workflow (10 min delay), can also run on daily cron at 3:00 AM UTC as fallback

**No push triggers**: Images only build on schedule, not on file changes

**Build Duration**: Platform images take longer to build since they compile multiple boards to pre-cache all toolchains. This is intentional - the cost is paid once during nightly builds so users get instant compilation.

**Workflow Trigger**: Base image workflow triggers platform builds using `gh workflow run` with a 10-minute delay parameter to allow base image to propagate through Docker Hub's CDN.

## Implementation Mapping

### GitHub Workflows

**Three workflow files:**

1. **`.github/workflows/build_docker_compiler_base.yml`**
   - Builds base image
   - Runs daily at 2:00 AM UTC
   - Triggers platform builds after completion

2. **`.github/workflows/build_docker_compiler_platforms.yml`**
   - Single unified workflow building ALL platform images in parallel
   - Contains jobs for: avr, esp, teensy, stm32, rp2040, nrf52, sam
   - Triggered by base workflow (with 10 min delay)
   - Can also run via manual dispatch or fallback cron

3. **`.github/workflows/template_build_docker_compiler.yml`**
   - Reusable template for building single-architecture images
   - Called by both base and platform workflows

### Template Workflow

**File**: `.github/workflows/template_build_docker_compiler.yml`

Reusable workflow for building platform images:
- Accepts platform name as input
- Handles multi-arch builds
- Manages digest uploads for manifest merging

### Python Board Mapping

**File**: `ci/docker/build_platforms.py`

The single source of truth for platform→boards relationships. This module defines which boards belong to which Docker platform family and is used by `build.sh` during Docker image builds.

**Structure**:
```python
DOCKER_PLATFORMS = {
    "avr": ["uno", "yun", "attiny85", "attiny88", "attiny4313",
            "nano_every", "ATtiny1604", "ATtiny1616"],
    "esp": ["esp32dev", "esp32s3", "esp32c3", "esp32c2", "esp32c5",
            "esp32c6", "esp32h2", "esp32p4", "esp32s2", "esp8266"],
    "teensy": ["teensylc", "teensy30", "teensy31", "teensy40", "teensy41"],
    "stm32": ["bluepill", "blackpill", "maple_mini",
              "hy_tinystm103tb", "giga_r1"],
    "rp2040": ["rp2040", "rp2350"],
    "nrf52": ["nrf52840_dk", "adafruit_feather_nrf52840_sense",
              "xiaoblesense"],
    "sam": ["due", "digix"],
}

# Reverse mapping automatically generated
BOARD_TO_PLATFORM = {
    board: platform
    for platform, boards in DOCKER_PLATFORMS.items()
    for board in boards
}
```

**How It Works**:

During Docker build (`ci/docker/build.sh`), the "compile" stage:
1. Receives `PLATFORM_NAME` from Dockerfile (e.g., "uno")
2. Looks up platform family: `BOARD_TO_PLATFORM["uno"]` → `"avr"`
3. Gets all boards for platform: `DOCKER_PLATFORMS["avr"]` → `["uno", "attiny85", ...]`
4. Loops through and compiles each board: `bash compile uno Blink`, `bash compile attiny85 Blink`, etc.
5. Result: All toolchains for the platform family are pre-cached in the image

**Helper Functions**:
- `get_platform_for_board(board_name)` - Returns platform family for a board
- `get_boards_for_platform(platform)` - Returns all boards in a platform
- `get_docker_image_name(platform, board)` - Generates Docker image name

**Adding New Boards**:

Simply add the board name to the appropriate platform list in `DOCKER_PLATFORMS`:

```python
"avr": [
    "uno",
    "attiny85",
    "new_avr_board",  # <-- Add here
],
```

The board will automatically be included in the next Docker build, and its toolchains will be pre-cached.

**Adding New Platforms**:

1. Add new platform entry to `DOCKER_PLATFORMS`
2. Add corresponding workflow jobs in `.github/workflows/build_docker_compiler_platforms.yml`
3. Update this documentation

## Adding New Platforms

To add a new platform image:

1. **Edit `.github/workflows/build_docker_compiler_platforms.yml`**:
   - Add new platform to credentials job outputs
   - Add new `build-<platform>-amd64` job
   - Add new `build-<platform>-arm` job
   - Add new `merge-<platform>` job
   - Follow the existing pattern for AVR, ESP, etc.

2. **Example addition** (for Apollo3 platform):
   ```yaml
   # In credentials job, add:
   registry_apollo3: ${{ steps.credentials.outputs.registry_apollo3 }}

   # In credentials steps, add:
   echo "registry_apollo3=$(echo 'niteris/fastled-compiler-apollo3-apollo3_red' | base64 -w0 | base64 -w0)" >> $GITHUB_OUTPUT

   # Then add the three jobs: build-apollo3-amd64, build-apollo3-arm, merge-apollo3
   ```

3. **Update this document** with new platform mapping

4. **Test locally**:
   ```bash
   cd ci/docker
   docker build -f Dockerfile.template \
     --build-arg PLATFORM_NAME=<board> \
     -t niteris/fastled-compiler-<platform>-<board>:latest .
   ```

## Local Testing

### Build Base Image
```bash
cd ci/docker
docker build -f Dockerfile.base \
  -t niteris/fastled-compiler-base:latest .
```

### Build Platform Image (Multi-Board)
```bash
cd ci/docker
docker build -f Dockerfile.template \
  --build-arg PLATFORM_NAME=uno \
  -t niteris/fastled-compiler-avr-uno:latest .
```

**What happens**:
- `build.sh` receives `PLATFORM_NAME=uno`
- Looks up platform family in `build_platforms.py`: uno → "avr"
- Gets all AVR boards: ["uno", "yun", "attiny85", "attiny88", "nano_every", "ATtiny1604", "ATtiny1616"]
- Compiles each board to pre-cache all AVR toolchains
- Image contains cached toolchains for ALL AVR boards

### Test Compilation
```bash
cd ../..  # Back to project root
bash compile uno Blink --docker       # Uses pre-cached AVR toolchain
bash compile attiny85 Blink --docker  # Also uses same avr-uno image (instant!)
bash compile nano_every Blink --docker  # Also instant - toolchain already cached
```

All boards in the platform family compile instantly with zero downloads.

## Future Enhancements

### IDF Version Tags for ESP

When multiple IDF versions are needed:

1. Create multiple workflows with different tags:
   - `build_docker_compiler_esp_idf5.3.yml`
   - `build_docker_compiler_esp_idf5.4.yml`
   - `build_docker_compiler_esp_idf5.5.yml`

2. Each workflow builds with different `PLATFORM_NAME` and IDF configuration

3. Update base Dockerfile or use build args to select IDF version

### Automatic Platform Detection

Enhance `ci/boards.py` to automatically map boards to Docker images:

```python
def get_docker_image_for_board(board_name: str) -> str:
    """Return the Docker image name for a given board."""
    for platform, boards in DOCKER_PLATFORM_MAP.items():
        if board_name in boards:
            return f"niteris/fastled-compiler-{platform}:latest"
    return "niteris/fastled-compiler-base:latest"  # fallback
```

### Build Matrix Optimization

Instead of separate workflows, use a single workflow with matrix strategy:

```yaml
strategy:
  matrix:
    include:
      - platform: avr-uno
        board: uno
      - platform: esp
        board: esp32dev
      # ... etc
```

This would reduce workflow duplication but make scheduling offsets harder.

## References

- **ci/docker/Dockerfile.base**: Base image with PlatformIO and dependencies
- **ci/docker/Dockerfile.template**: Platform-specific image template
- **ci/docker/build.sh**: Docker build script with layered caching and multi-board compilation
- **ci/docker/build_platforms.py**: Platform→boards mapping (source of truth for Docker families)
- **ci/boards.py**: Individual board configurations (source of truth for PlatformIO settings)
- **build.py**: Local testing script for building multiple platforms

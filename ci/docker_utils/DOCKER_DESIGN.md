# FastLED Docker Compiler Images - Design Document

## Overview

This document describes the architecture and naming conventions for FastLED's pre-built Docker compiler images. These images provide pre-cached PlatformIO toolchains for faster compilation across different embedded platforms.

## Architecture

### Base + Platform Pattern

```
niteris/fastled-compiler-base:latest
    ├── niteris/fastled-compiler-avr:latest
    ├── ESP32 Platforms (flat structure - one image per board):
    │   ├── niteris/fastled-compiler-esp-32dev:latest
    │   ├── niteris/fastled-compiler-esp-32s2:latest
    │   ├── niteris/fastled-compiler-esp-32s3:latest
    │   ├── niteris/fastled-compiler-esp-8266:latest
    │   ├── niteris/fastled-compiler-esp-32c2:latest
    │   ├── niteris/fastled-compiler-esp-32c3:latest
    │   ├── niteris/fastled-compiler-esp-32c5:latest
    │   ├── niteris/fastled-compiler-esp-32c6:latest
    │   ├── niteris/fastled-compiler-esp-32h2:latest
    │   └── niteris/fastled-compiler-esp-32p4:latest
    ├── niteris/fastled-compiler-teensy:latest
    ├── STM32 Platforms (flat structure - one image per board):
    │   ├── niteris/fastled-compiler-stm32-f103c8:latest
    │   ├── niteris/fastled-compiler-stm32-f411ce:latest
    │   ├── niteris/fastled-compiler-stm32-f103cb:latest
    │   ├── niteris/fastled-compiler-stm32-f103tb:latest
    │   └── niteris/fastled-compiler-stm32-h747xi:latest
    ├── niteris/fastled-compiler-rp:latest
    ├── niteris/fastled-compiler-nrf52:latest
    └── SAM Platforms (flat structure - one image per board):
        └── niteris/fastled-compiler-sam-3x8e:latest
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

**Dockerfile**: `ci/docker_utils/Dockerfile.base`

### Platform Images

**Pattern**: `niteris/fastled-compiler-<platform>:latest`

**Contents**:
- Inherits from base image
- Pre-cached platform-specific toolchains for **ALL boards** in the platform family
- Pre-warmed compilation cache for multiple boards

**Build Strategy**: Varies by platform architecture:

**Grouped Platforms** (AVR, Teensy, RP, NRF52):
- Each image compiles **multiple boards** to pre-cache all toolchains and dependencies
- Examples:
  - `avr` image compiles: uno, attiny85, attiny88, nano_every, etc.
  - `teensy` image compiles: teensy30, teensy31, teensy40, teensy41, teensylc
  - `nrf52` image compiles: nrf52840_dk, adafruit boards, xiaoblesense

**Flat Platforms** (ESP, STM32, SAM):
- Each image compiles **a single board** to prevent build artifact accumulation
- Examples:
  - `esp-32s3` image compiles: esp32s3 only
  - `esp-8266` image compiles: esp8266 only
  - `stm32-f103c8` image compiles: stm32f103c8 only
  - `sam-3x8e` image compiles: sam3x8e_due only

This ensures instant compilation without downloading additional toolchains, while preventing Docker image bloat for platforms prone to build artifact accumulation.

**Build Schedule**: Daily at 3:00 AM UTC (1 hour after base)

**Dockerfile**: `ci/docker_utils/Dockerfile.template` (with `PLATFORM_NAME` build arg for initial setup, then multi-board warm-up)

## Platform Naming Strategy

### Platform Families

Platform images use two strategies:

**Grouped Platforms**: Boards that share the same PlatformIO platform and have small toolchain overhead are grouped together.

**Flat Platforms**: ESP, STM32, and SAM boards use individual images (one per board) to prevent build artifact accumulation issues.

| Platform Family | Docker Image | Included Boards | Strategy |
|----------------|--------------|-----------------|----------|
| **avr** | `fastled-compiler-avr` | uno, leonardo, attiny85, attiny88, attiny4313, nano_every, attiny1604, attiny1616 | Grouped |
| **esp-32dev** | `fastled-compiler-esp-32dev` | esp32dev | Flat |
| **esp-32s2** | `fastled-compiler-esp-32s2` | esp32s2 | Flat |
| **esp-32s3** | `fastled-compiler-esp-32s3` | esp32s3 | Flat |
| **esp-8266** | `fastled-compiler-esp-8266` | esp8266 | Flat |
| **esp-32c2** | `fastled-compiler-esp-32c2` | esp32c2 | Flat |
| **esp-32c3** | `fastled-compiler-esp-32c3` | esp32c3 | Flat |
| **esp-32c5** | `fastled-compiler-esp-32c5` | esp32c5 | Flat |
| **esp-32c6** | `fastled-compiler-esp-32c6` | esp32c6 | Flat |
| **esp-32h2** | `fastled-compiler-esp-32h2` | esp32h2 | Flat |
| **esp-32p4** | `fastled-compiler-esp-32p4` | esp32p4 | Flat |
| **teensy** | `fastled-compiler-teensy` | teensy30, teensy31, teensy40, teensy41, teensylc | Grouped |
| **stm32-f103c8** | `fastled-compiler-stm32-f103c8` | stm32f103c8 | Flat |
| **stm32-f411ce** | `fastled-compiler-stm32-f411ce` | stm32f411ce | Flat |
| **stm32-f103cb** | `fastled-compiler-stm32-f103cb` | stm32f103cb | Flat |
| **stm32-f103tb** | `fastled-compiler-stm32-f103tb` | stm32f103tb | Flat |
| **stm32-h747xi** | `fastled-compiler-stm32-h747xi` | stm32h747xi | Flat |
| **rp2040** | `fastled-compiler-rp` | rp2040, rp2350 | Grouped |
| **nrf52** | `fastled-compiler-nrf52` | nrf52840_dk, adafruit_feather_nrf52840_sense, xiaoblesense | Grouped |
| **sam-3x8e** | `fastled-compiler-sam-3x8e` | sam3x8e_due | Flat |

### Platform Naming Convention

**Grouped Platforms**:
```
niteris/fastled-compiler-<platform-family>:<tag>
```

**Examples**:
- `niteris/fastled-compiler-avr:latest`
- `niteris/fastled-compiler-teensy:latest`
- `niteris/fastled-compiler-nrf52:latest`

**Flat Platforms (ESP, STM32, SAM)**:
```
niteris/fastled-compiler-<platform>-<board>:<tag>
```

**Examples**:
- `niteris/fastled-compiler-esp-32s3:latest`
- `niteris/fastled-compiler-esp-8266:latest`
- `niteris/fastled-compiler-stm32-f103c8:latest`
- `niteris/fastled-compiler-stm32-h747xi:latest`
- `niteris/fastled-compiler-sam-3x8e:latest`

**Rationale**:
- Grouped platforms: `<platform-family>` groups boards by shared toolchain (e.g., `avr`, `teensy`)
- Flat platforms: `<platform>-<board>` provides one image per board to prevent build artifact accumulation
- `<tag>`: Version identifier (currently `:latest`)
- ESP, STM32, and SAM boards use flat structure due to large toolchain sizes and build artifact issues

## Special Cases

### ESP32 Platform

**Flat Structure**: ESP32 family uses individual images (one per board) to prevent build artifact accumulation:

**Xtensa Architecture Boards** (4 images):
- `niteris/fastled-compiler-esp-32dev:latest` - ESP32 (original)
- `niteris/fastled-compiler-esp-32s2:latest` - ESP32-S2
- `niteris/fastled-compiler-esp-32s3:latest` - ESP32-S3
- `niteris/fastled-compiler-esp-8266:latest` - ESP8266

**RISC-V Architecture Boards** (6 images):
- `niteris/fastled-compiler-esp-32c2:latest` - ESP32-C2
- `niteris/fastled-compiler-esp-32c3:latest` - ESP32-C3
- `niteris/fastled-compiler-esp-32c5:latest` - ESP32-C5
- `niteris/fastled-compiler-esp-32c6:latest` - ESP32-C6
- `niteris/fastled-compiler-esp-32h2:latest` - ESP32-H2
- `niteris/fastled-compiler-esp-32p4:latest` - ESP32-P4

**Rationale**:
- ESP32 toolchains are significantly larger than other platforms
- Build artifacts accumulate rapidly in grouped images, causing build failures
- Flat structure (one image per board) prevents artifact accumulation
- Users only download the specific toolchain they need
- Each board may use different IDF versions optimized for that chip

**IDF Version Strategy**: Each image uses `:latest` tag with the optimal IDF version for that board (typically from pioarduino). Future strategy (when needed):
```
niteris/fastled-compiler-esp-32s3:latest    → Current stable (e.g., idf5.4)
niteris/fastled-compiler-esp-32s3:idf5.4    → Explicit IDF 5.4
niteris/fastled-compiler-esp-32dev:latest   → Current stable (e.g., idf5.3)
niteris/fastled-compiler-esp-32dev:idf5.3   → Explicit IDF 5.3
```

**Platform Sources**:
- **IDF 5.1+**: Pioarduino fork (`pioarduino/platform-espressif32`) - faster updates to new IDF versions
- **IDF 4.4**: Official PlatformIO (`platformio/platform-espressif32`)

**Note**: Pioarduino is an implementation detail and doesn't need to appear in image names. Users care about IDF version, not the source repository.

### Teensy Platform

**Note**: Despite radical differences between Teensy 3.x (Cortex-M4, ~72MHz) and Teensy 4.x (Cortex-M7, 600MHz), the toolchains have small payload and can be aggregated into a single image.

**Image**: `niteris/fastled-compiler-teensy:latest`
- Pre-caches ALL Teensy boards: teensylc, teensy30, teensy31, teensy40, teensy41
- Build process compiles example for each board to warm up all toolchains

### AVR Platform

**Note**: Combines classic AVR (`atmelavr`) and modern AVR (`atmelmegaavr`) platforms since they share similar toolchains.

**Image**: `niteris/fastled-compiler-avr:latest`

**Pre-cached boards**:
- Classic AVR (`atmelavr`): uno, leonardo, attiny85, attiny88, attiny4313
- Modern AVR (`atmelmegaavr`): nano_every, attiny1604, attiny1616

**Build process**: Compiles example for each board listed above to ensure all AVR toolchains are cached in the image.

### STM32 Platform

**Flat Structure**: STM32 family uses individual images (one per board) to prevent build artifact accumulation:

**STM32 Board Images** (5 images):
- `niteris/fastled-compiler-stm32-f103c8:latest` - STM32F103C8 Blue Pill
- `niteris/fastled-compiler-stm32-f411ce:latest` - STM32F411CE Black Pill
- `niteris/fastled-compiler-stm32-f103cb:latest` - STM32F103CB Maple Mini
- `niteris/fastled-compiler-stm32-f103tb:latest` - STM32F103TB Tiny STM
- `niteris/fastled-compiler-stm32-h747xi:latest` - STM32H747XI Arduino Giga R1

**Rationale**:
- STM32 toolchains and build artifacts can accumulate in grouped images
- Flat structure (one image per board) prevents artifact accumulation
- Users only download the specific toolchain they need
- Each board may use different STM32 HAL versions and configurations

### SAM Platform

**Flat Structure**: SAM family uses individual images (one per board) to prevent build artifact accumulation:

**SAM Board Images** (1 image):
- `niteris/fastled-compiler-sam-3x8e:latest` - Atmel SAM3X8E Due

**Rationale**:
- SAM toolchains and build artifacts can accumulate in grouped images
- Flat structure maintains consistency with other flat platforms (ESP, STM32)
- Provides flexibility for future SAM board additions without affecting existing images

## Multi-Architecture Support

All images are built for **linux/amd64** and **linux/arm64** using Docker Buildx.

**Build Process** (Independent per Platform):
1. Build amd64 image on `ubuntu-24.04` runner
2. Build arm64 image on `ubuntu-24.04-arm` runner
3. Once both architectures complete for a platform, merge digests into single multi-arch manifest
4. Push manifest to Docker Hub immediately (without waiting for other platforms)

**Key Feature**: Each platform merges and uploads independently. If one platform build fails (e.g., ESP32-C6), other platforms (AVR, Teensy, etc.) still merge and upload successfully.

## Build Schedule

```
2:00 AM UTC  →  Base image builds
                 ↓
                 (10 minute delay for registry propagation)
                 ↓
2:10 AM UTC  →  Platform images build in parallel
                 ├── avr (grouped: uno, leonardo, attiny85, attiny88, nano_every, etc.)
                 ├── ESP32 boards (flat - 10 individual images):
                 │   ├── esp-32dev (builds: esp32dev only)
                 │   ├── esp-32s2 (builds: esp32s2 only)
                 │   ├── esp-32s3 (builds: esp32s3 only)
                 │   ├── esp-8266 (builds: esp8266 only)
                 │   ├── esp-32c2 (builds: esp32c2 only)
                 │   ├── esp-32c3 (builds: esp32c3 only)
                 │   ├── esp-32c5 (builds: esp32c5 only)
                 │   ├── esp-32c6 (builds: esp32c6 only)
                 │   ├── esp-32h2 (builds: esp32h2 only)
                 │   └── esp-32p4 (builds: esp32p4 only)
                 ├── teensy (grouped: teensy30, teensy31, teensy40, teensy41, teensylc)
                 ├── STM32 boards (flat - 5 individual images):
                 │   ├── stm32-f103c8 (builds: stm32f103c8 only)
                 │   ├── stm32-f411ce (builds: stm32f411ce only)
                 │   ├── stm32-f103cb (builds: stm32f103cb only)
                 │   ├── stm32-f103tb (builds: stm32f103tb only)
                 │   └── stm32-h747xi (builds: stm32h747xi only)
                 ├── rp (grouped: rp2040, rp2350)
                 ├── nrf52 (grouped: nrf52840_dk, adafruit boards, xiaoblesense)
                 └── SAM boards (flat - 1 individual image):
                     └── sam-3x8e (builds: sam3x8e_due only)
```

**Scheduling**:
- Base image: Daily at 2:00 AM UTC via cron
- Platform images: Triggered by base image workflow (10 min delay), can also run on daily cron at 3:00 AM UTC as fallback

**No push triggers**: Images only build on schedule, not on file changes

**Build Duration**: Platform images take longer to build since they compile multiple boards to pre-cache all toolchains. This is intentional - the cost is paid once during nightly builds so users get instant compilation.

**Workflow Trigger**: Base image workflow triggers platform builds using `gh workflow run` with a 10-minute delay parameter to allow base image to propagate through Docker Hub's CDN.

## Implementation Mapping

### GitHub Workflows

**Four workflow files:**

1. **`.github/workflows/docker_compiler_base.yml`**
   - Builds base image
   - Runs daily at 2:00 AM UTC
   - Triggers platform builds after completion

2. **`.github/workflows/docker_compiler_template.yml`**
   - Main workflow orchestrating all platform builds
   - Contains 20 independent build jobs (one per platform)
   - Each build job produces amd64 + arm64 images
   - Contains 20 independent merge jobs (one per platform)
   - Each merge job depends only on its corresponding build job
   - Triggered by base workflow (with 10 min delay)
   - Can also run via manual dispatch or fallback cron

3. **`.github/workflows/docker_build_compiler.yml`**
   - Reusable template for building single-architecture images
   - Called by platform build jobs (40 times total: 20 platforms × 2 architectures)

4. **`.github/workflows/docker_merge_platform.yml`**
   - Reusable template for merging multi-arch manifests
   - Called by platform merge jobs (20 times total: one per platform)
   - Downloads digests, creates manifest, pushes to Docker Hub

### Reusable Workflows

**Build Workflow** (`.github/workflows/docker_build_compiler.yml`):
- Builds a single-architecture Docker image
- Accepts platform name, dockerfile, architecture as inputs
- Pushes image digest for later manifest merging
- Uploads digest as artifact

**Merge Workflow** (`.github/workflows/docker_merge_platform.yml`):
- Merges amd64 and arm64 digests into multi-arch manifest
- Accepts platform name and registry as inputs
- Downloads digest artifacts from build jobs
- Creates and pushes manifest to Docker Hub

### Python Board Mapping

**File**: `ci/docker_utils/build_platforms.py`

The single source of truth for platform→boards relationships. This module defines which boards belong to which Docker platform family and is used by `build.sh` during Docker image builds.

**Structure**:
```python
DOCKER_PLATFORMS = {
    # Grouped platforms (multi-board images)
    "avr": ["uno", "leonardo", "attiny85", "attiny88", "attiny4313",
            "nano_every", "attiny1604", "attiny1616"],
    "teensy": ["teensylc", "teensy30", "teensy31", "teensy40", "teensy41"],
    "rp": ["rp2040", "rp2350"],
    "nrf52": ["nrf52840_dk", "adafruit_feather_nrf52840_sense",
              "xiaoblesense"],

    # Flat platforms (single-board images) - ESP boards
    "esp-32dev": ["esp32dev"],
    "esp-32s2": ["esp32s2"],
    "esp-32s3": ["esp32s3"],
    "esp-8266": ["esp8266"],
    "esp-32c2": ["esp32c2"],
    "esp-32c3": ["esp32c3"],
    "esp-32c5": ["esp32c5"],
    "esp-32c6": ["esp32c6"],
    "esp-32h2": ["esp32h2"],
    "esp-32p4": ["esp32p4"],

    # Flat platforms (single-board images) - STM32 boards
    "stm32-f103c8": ["stm32f103c8"],
    "stm32-f411ce": ["stm32f411ce"],
    "stm32-f103cb": ["stm32f103cb"],
    "stm32-f103tb": ["stm32f103tb"],
    "stm32-h747xi": ["stm32h747xi"],

    # Flat platforms (single-board images) - SAM boards
    "sam-3x8e": ["sam3x8e_due"],
}

# Reverse mapping automatically generated
BOARD_TO_PLATFORM = {
    board: platform
    for platform, boards in DOCKER_PLATFORMS.items()
    for board in boards
}
```

**How It Works**:

During Docker build (`ci/docker_utils/build.sh`), the "compile" stage:

**Grouped Platforms** (e.g., AVR, Teensy):
1. Receives `PLATFORM_NAME` from Dockerfile (e.g., "uno")
2. Looks up platform family: `BOARD_TO_PLATFORM["uno"]` → `"avr"`
3. Gets all boards for platform: `DOCKER_PLATFORMS["avr"]` → `["uno", "leonardo", ...]`
4. Loops through and compiles each board: `bash compile uno Blink`, `bash compile attiny85 Blink`, etc.
5. Result: All toolchains for the platform family are pre-cached in the image

**Flat Platforms** (e.g., ESP):
1. Receives `PLATFORM_NAME` from Dockerfile (e.g., "esp32s3")
2. Looks up platform: `BOARD_TO_PLATFORM["esp32s3"]` → `"esp-32s3"`
3. Gets boards for platform: `DOCKER_PLATFORMS["esp-32s3"]` → `["esp32s3"]` (single board)
4. Compiles the single board: `bash compile esp32s3 Blink`
5. Result: Only the specific board's toolchain is cached, preventing artifact accumulation

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

1. **Add platform to `ci/docker_utils/build_platforms.py`**:
   ```python
   DOCKER_PLATFORMS = {
       # ... existing platforms ...
       "apollo3": ["apollo3_sparkfun"],  # Add new platform
   }
   ```

2. **Edit `.github/workflows/docker_compiler_template.yml`**:

   a. Add registry output to `credentials` job:
   ```yaml
   outputs:
     registry_apollo3: ${{ steps.credentials.outputs.registry_apollo3 }}
   ```

   b. Add registry generation to credentials steps:
   ```yaml
   echo "registry_apollo3=$(echo -n 'niteris/fastled-compiler-apollo3' | base64 -w0 | base64 -w0)" >> $GITHUB_OUTPUT
   ```

   c. Add build job:
   ```yaml
   build-apollo3:
     name: 🔨 fastled-compiler-apollo3 [${{ matrix.arch.platform }}]
     if: github.event.inputs.skip_platforms != 'true'
     needs: [credentials, wait-for-base]
     strategy:
       fail-fast: false
       matrix:
         arch:
           - runs_on: ubuntu-24.04
             platform: linux/amd64
           - runs_on: ubuntu-24.04-arm
             platform: linux/arm64
     uses: ./.github/workflows/docker_build_compiler.yml
     with:
       runs_on: ${{ matrix.arch.runs_on }}
       platform: ${{ matrix.arch.platform }}
       dockerfile: Dockerfile.template
       group: apollo3
       platforms: apollo3_sparkfun
       tag: latest
     secrets:
       env_vars: |
         docker_username=${{ needs.credentials.outputs.docker_username }}
         docker_password=${{ needs.credentials.outputs.docker_password }}
         docker_registry_image=${{ needs.credentials.outputs.registry_apollo3 }}
   ```

   d. Add merge job:
   ```yaml
   merge-apollo3:
     name: 📦 merge fastled-compiler-apollo3
     if: github.event.inputs.skip_platforms != 'true'
     needs: build-apollo3
     uses: ./.github/workflows/docker_merge_platform.yml
     with:
       group_name: apollo3
       registry: niteris/fastled-compiler-apollo3
       tag: latest
     secrets:
       docker_password: ${{ secrets.DOCKER_PASSWORD }}
   ```

3. **Update this document** with new platform mapping

4. **Test locally**:
   ```bash
   cd ci/docker
   docker build -f Dockerfile.template \
     --build-arg PLATFORM_NAME=<board> \
     -t niteris/fastled-compiler-<platform>:latest .
   ```

**Key Benefit**: The new platform will merge and upload independently, without being blocked by other platform builds.

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
  -t niteris/fastled-compiler-avr:latest .
```

**What happens**:
- `build.sh` receives `PLATFORM_NAME=uno`
- Looks up platform family in `build_platforms.py`: uno → "avr"
- Gets all AVR boards: ["uno", "leonardo", "attiny85", "attiny88", "nano_every", "attiny1604", "attiny1616"]
- Compiles each board to pre-cache all AVR toolchains
- Image contains cached toolchains for ALL AVR boards

### Test Compilation
```bash
cd ../..  # Back to project root
bash compile uno Blink --docker       # Uses pre-cached AVR toolchain
bash compile attiny85 Blink --docker  # Also uses same avr image (instant!)
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

- **ci/docker_utils/Dockerfile.base**: Base image with PlatformIO and dependencies
- **ci/docker_utils/Dockerfile.template**: Platform-specific image template
- **ci/docker_utils/build.sh**: Docker build script with layered caching and multi-board compilation
- **ci/docker_utils/build_platforms.py**: Platform→boards mapping (source of truth for Docker families)
- **ci/boards.py**: Individual board configurations (source of truth for PlatformIO settings)
- **build.py**: Local testing script for building multiple platforms

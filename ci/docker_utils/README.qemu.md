# Docker-based QEMU ESP32 Testing

This directory contains Docker-based QEMU ESP32 emulation tools for FastLED testing.

## Overview

The Docker QEMU solution provides:
- Consistent testing environment across different platforms
- No need for local QEMU installation
- Isolated execution environment
- Support for multiple ESP32 variants (ESP32, ESP32-C3, ESP32-S2, ESP32-S3)

## Components

### Core Files

- `DockerManager.py` - Docker container management utilities
- `qemu_esp32_docker.py` - Main Docker QEMU runner script
- `qemu_test_integration.py` - Integration with existing test framework

### Docker Images

The QEMU runner uses the official **`espressif/idf:latest`** Docker image from Docker Hub, which includes:
- Complete ESP-IDF toolchain
- QEMU binaries for ESP32, ESP32-C3, ESP32-S3
- All necessary development tools

No custom Dockerfiles needed - everything runs using the official Espressif image.

## Usage

### Quick Start

1. **Run QEMU tests via test.py (recommended):**
   ```bash
   uv run test.py --qemu esp32c3 BlinkParallel
   ```

2. **Run a firmware test with Docker directly:**
   ```bash
   uv run ci/docker_utils/qemu_esp32_docker.py path/to/firmware.bin
   ```

3. **Run with specific timeout and interrupt pattern:**
   ```bash
   uv run ci/docker_utils/qemu_esp32_docker.py firmware.bin \
     --timeout 60 \
     --interrupt-regex "Test passed"
   ```

### Integration with Test Framework

The Docker QEMU runner integrates with the existing test framework:

```bash
# Check available runners
uv run ci/docker_utils/qemu_test_integration.py check

# Run test with automatic runner selection
uv run ci/docker_utils/qemu_test_integration.py test --firmware path/to/firmware.bin

# Force Docker runner
uv run ci/docker_utils/qemu_test_integration.py test --firmware firmware.bin --docker
```

### Docker Image Used

The QEMU runner automatically pulls and uses:
- **`espressif/idf:latest`** (~11.6GB) - Official Espressif ESP-IDF Docker image
  - Includes complete ESP-IDF toolchain
  - QEMU binaries for all ESP32 variants (ESP32, ESP32-C3, ESP32-S3)
  - Development and debugging tools
  - Automatically pulled on first use

## Features

### Automatic Fallback

The integration module automatically selects the best available runner:
1. Native QEMU (if installed)
2. Docker QEMU (if Docker available)
3. Error if neither available

### Platform Support

Works on:
- Linux (native Docker)
- macOS (Docker Desktop)
- Windows (Docker Desktop, WSL2)

### Firmware Formats

Supports:
- Direct `firmware.bin` files
- PlatformIO build directories
- ESP-IDF build directories

## Environment Variables

- `FIRMWARE_DIR` - Directory containing firmware files
- `FIRMWARE_FILE` - Specific firmware file to run
- `MACHINE` - ESP32 variant (esp32, esp32c3, esp32s2, esp32s3)
- `FLASH_SIZE` - Flash size in MB (default: 4)
- `TIMEOUT` - Execution timeout in seconds (default: 30)

## Troubleshooting

### Docker Not Found

Install Docker:
- **Windows/Mac**: Install Docker Desktop
- **Linux**: `sudo apt-get install docker.io` or equivalent

### Permission Denied

Add user to docker group:
```bash
sudo usermod -aG docker $USER
# Log out and back in
```

### Image Pull Failed

Check internet connection and Docker Hub access. The runner will automatically try alternative images if the primary fails.

### QEMU Timeout

Increase timeout value:
```bash
uv run ci/docker_utils/qemu_esp32_docker.py firmware.bin --timeout 120
```

## CI/CD Integration

### GitHub Actions

```yaml
- name: Run QEMU Test in Docker
  run: |
    uv run ci/docker_utils/qemu_esp32_docker.py \
      .pio/build/esp32dev/firmware.bin \
      --timeout 60 \
      --interrupt-regex "Setup complete"
```

### Local CI

```bash
# Install dependencies
uv pip install -r requirements.txt

# Run tests
uv run ci/docker_utils/qemu_test_integration.py test \
  --firmware .pio/build/esp32dev/firmware.bin
```

## Development

### Adding New ESP32 Variants

1. Add machine type mapping to `qemu_esp32_docker.py` (lines 319-331)
2. Add QEMU binary path if different architecture (RISC-V vs Xtensa)
3. Update test.py machine type detection (lines 182-190)
4. Test with sample firmware

### Debugging

Run interactively with extended timeout:
```bash
uv run ci/docker_utils/qemu_esp32_docker.py firmware.bin --interactive --timeout 300
```

Check QEMU output logs:
```bash
# Output is saved to qemu_output.log by default
cat qemu_output.log
```

## Performance

- Docker adds ~2-5 seconds overhead vs native QEMU
- First run pulls image (one-time ~100MB download)
- Subsequent runs use cached image
- Consider using lightweight image for CI/CD
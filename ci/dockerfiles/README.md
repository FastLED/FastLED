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
- `Dockerfile.qemu-esp32` - Full ESP-IDF + QEMU environment
- `Dockerfile.qemu-esp32-lite` - Lightweight QEMU-only environment
- `docker-compose.yml` - Docker Compose configuration

## Usage

### Quick Start

1. **Run a firmware test with Docker:**
   ```bash
   uv run ci/docker/qemu_esp32_docker.py path/to/firmware.bin
   ```

2. **Run with specific timeout and interrupt pattern:**
   ```bash
   uv run ci/docker/qemu_esp32_docker.py firmware.bin \
     --timeout 60 \
     --interrupt-regex "Test passed"
   ```

3. **Use docker-compose:**
   ```bash
   cd ci/docker
   FIRMWARE_DIR=../../.pio/build/esp32dev docker-compose up qemu-esp32
   ```

### Integration with Test Framework

The Docker QEMU runner integrates with the existing test framework:

```bash
# Check available runners
uv run ci/docker/qemu_test_integration.py check

# Run test with automatic runner selection
uv run ci/docker/qemu_test_integration.py test --firmware path/to/firmware.bin

# Force Docker runner
uv run ci/docker/qemu_test_integration.py test --firmware firmware.bin --docker
```

### Building Docker Images

```bash
# Build lightweight image
docker build -f Dockerfile.qemu-esp32-lite -t fastled/qemu-esp32:lite .

# Build full ESP-IDF image
docker build -f Dockerfile.qemu-esp32 -t fastled/qemu-esp32:full .

# Or use docker-compose
docker-compose build
```

## Docker Images

### Pre-built Images

The runner can use pre-built images:
- `espressif/qemu:esp-develop-8.2.0-20240122` (Official Espressif image)
- `mluis/qemu-esp32:latest` (Community image)

### Custom Images

Two Dockerfiles are provided:

1. **Dockerfile.qemu-esp32-lite** (Recommended)
   - Minimal Ubuntu base
   - QEMU binaries only
   - ~200MB image size
   - Fast startup

2. **Dockerfile.qemu-esp32**
   - Full ESP-IDF environment
   - Development tools included
   - ~2GB image size
   - Complete toolchain

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
uv run ci/docker/qemu_esp32_docker.py firmware.bin --timeout 120
```

## CI/CD Integration

### GitHub Actions

```yaml
- name: Run QEMU Test in Docker
  run: |
    uv run ci/docker/qemu_esp32_docker.py \
      .pio/build/esp32dev/firmware.bin \
      --timeout 60 \
      --interrupt-regex "Setup complete"
```

### Local CI

```bash
# Install dependencies
uv pip install -r requirements.txt

# Run tests
uv run ci/docker/qemu_test_integration.py test \
  --firmware .pio/build/esp32dev/firmware.bin
```

## Development

### Adding New ESP32 Variants

1. Update Dockerfiles with new ROM files
2. Add machine type to runner scripts
3. Test with sample firmware

### Debugging

Run interactively:
```bash
uv run ci/docker/qemu_esp32_docker.py firmware.bin --interactive
```

Enable verbose output:
```bash
docker-compose up qemu-esp32-interactive
```

## Performance

- Docker adds ~2-5 seconds overhead vs native QEMU
- First run pulls image (one-time ~100MB download)
- Subsequent runs use cached image
- Consider using lightweight image for CI/CD
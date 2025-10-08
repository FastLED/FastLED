# FastLED Cloud Development Environment

This devcontainer configuration enables running FastLED development in the cloud using GitHub Codespaces or Gitpod.

## Quick Start

### GitHub Codespaces

1. Navigate to the [FastLED repository](https://github.com/FastLED/FastLED)
2. Click **Code** → **Codespaces** → **Create codespace on main**
3. Wait 2-3 minutes for the container to build
4. Start developing! All dependencies are pre-installed

### Gitpod

Click this button or visit `https://gitpod.io/#https://github.com/FastLED/FastLED`:

[![Open in Gitpod](https://gitpod.io/button/open-in-gitpod.svg)](https://gitpod.io/#https://github.com/FastLED/FastLED)

## What's Included

- **Python 3.11** with uv package manager
- **PlatformIO** for embedded development
- **C++ toolchain** (clang, gcc, cmake, ninja)
- **QEMU** for ESP32 emulation
- **VS Code extensions**:
  - PlatformIO IDE
  - Python + Pylance
  - C/C++ tools
  - CMake tools

## Available Commands

All commands from the main repo work in the cloud environment:

```bash
# Run unit tests
uv run test.py

# Run C++ tests only
uv run test.py --cpp

# Run QEMU emulation tests
uv run test.py --qemu esp32s3

# Compile for specific platforms
uv run ci/ci-compile.py uno --examples Blink
uv run ci/ci-compile.py esp32s3 --examples Blink

# Lint code
bash lint

# Compile WASM
uv run ci/wasm_compile.py examples/Blink --just-compile
```

## Hardware Limitations

Cloud environments cannot access physical USB devices, so:

- ✅ **Full compilation support** for all 50+ platforms
- ✅ **Unit tests** run perfectly
- ✅ **QEMU emulation** for ESP32 testing
- ✅ **WASM compilation** and testing
- ❌ **Physical device flashing** requires local setup

For FastLED development, QEMU emulation provides excellent hardware validation without physical devices.

## Performance Notes

- **Build speed**: Cloud instances use 2-4 cores (free tier)
- **Caching**: PlatformIO cache is mounted for faster rebuilds
- **First build**: May take 5-10 minutes for initial dependency downloads
- **Subsequent builds**: Much faster due to caching

## Cost

### GitHub Codespaces
- **Free tier**: 120 core-hours/month (60 hours on 2-core)
- **Paid tier**: $0.18/hour for 4-core after free tier

### Gitpod
- **Free tier**: 50 hours/month
- **Paid tier**: $0.36/hour for standard workspace

## Troubleshooting

### Container fails to build
- Check that Python 3.11 feature is properly specified
- Ensure Dockerfile has correct base image

### PlatformIO not working
- Run `pio --version` to verify installation
- Try reinstalling: `pip install --upgrade platformio`

### Tests fail
- First build takes longer - wait for complete dependency installation
- Check internet connection for package downloads
- Review test output for specific errors

## Local Development

To use this devcontainer locally with VS Code:

1. Install Docker Desktop
2. Install "Dev Containers" VS Code extension
3. Open FastLED folder in VS Code
4. Press `F1` → "Dev Containers: Reopen in Container"

## Contributing

This devcontainer configuration is designed for cloud-based FastLED development. If you encounter issues or have improvements, please open an issue or PR on the FastLED repository.

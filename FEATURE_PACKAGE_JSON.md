# FastLED Native Platform Configuration

## Overview

This document describes the comprehensive `platform.json` configuration for FastLED's native build platform. The design leverages the Python `ziglang` package to provide a unified, cross-platform compilation environment that integrates seamlessly with the existing `build_unit.toml` structure.

## Key Design Principles

### 1. **Unified Toolchain Integration**
- **Ziglang Foundation**: Uses `python -m ziglang` commands matching the current `build_unit.toml` configuration
- **Cross-Platform Consistency**: Single toolchain across Linux, macOS, Windows with automatic target detection
- **Direct TOML Mapping**: Toolchain commands directly map between `platform.json` and `build_unit.toml`

### 2. **Comprehensive Architecture Support**
- **x86_64 Targets**: Linux, macOS, Windows with appropriate system-specific flags
- **ARM64 Targets**: Linux (generic), macOS (Apple Silicon), Windows with optimized CPU settings
- **RISC-V 64**: Linux support for emerging architecture
- **Auto-Detection**: Intelligent target selection for simplified development workflow

### 3. **Arduino Package Format Compliance**
- **PlatformIO Integration**: Full compatibility with PlatformIO package management
- **Framework Support**: Both FastLED-stub and Arduino-stub frameworks
- **Package Dependencies**: Proper toolchain, framework, and tool dependency management
- **Versioning Strategy**: Semantic versioning with appropriate version constraints

### 4. **Build System Flexibility**
- **Flag Inheritance**: Base, debug, release, and test flag configurations
- **Platform-Specific Linking**: Automatic application of OS-specific link flags
- **Compiler Caching**: Optional ccache/sccache integration for performance
- **Build System Support**: CMake, Ninja, and direct compilation compatibility

## Complete Platform.json Structure

The full `platform.json` includes:

1. **Board Definitions** (8 comprehensive targets):
   - `native-x64-linux`, `native-x64-macos`, `native-x64-windows`
   - `native-arm64-linux`, `native-arm64-macos`, `native-arm64-windows`
   - `native-riscv64-linux`, `native-auto` (intelligent detection)

2. **Toolchain Configuration**:
   - Complete ziglang tool mapping (CC, CXX, AR, LD, etc.)
   - Build flag hierarchies (base, debug, release, test)
   - Platform-specific link flags for each OS

3. **Package Management**:
   - Toolchain dependencies with proper versioning
   - Optional tool packages (cmake, ninja, caching)
   - Framework packages for Arduino compatibility

4. **Development Features**:
   - Integrated debugger support (GDB for Linux/Windows, LLDB for macOS)
   - Environment variable customization support
   - Target compatibility specifications

## Platform.json Configuration

The complete platform.json follows Arduino package format with these key sections:

### Core Metadata
```json
{
  "name": "FastLED Native Platform",
  "title": "Native Host Platform with Zig Toolchain", 
  "description": "Cross-platform native compilation using Zig's bundled Clang",
  "version": "1.0.0",
  "packageType": "platform",
  "spec": {
    "owner": "fastled",
    "id": "native-zig",
    "name": "native-zig"
  }
}
```

### Framework Definitions
```json
{
  "frameworks": {
    "fastled-stub": {
      "package": "framework-fastled-stub",
      "script": "builder/frameworks/fastled-stub.py"
    },
    "arduino-stub": {
      "package": "framework-arduino-stub", 
      "script": "builder/frameworks/arduino-stub.py"
    }
  }
}
```

### Board Configurations
Each board includes complete configuration:
- **Hardware specs**: MCU, RAM, Flash, CPU frequency
- **Target triple**: Zig compilation target
- **Build flags**: Platform-specific preprocessor defines
- **Debug tools**: Appropriate debugger (GDB/LLDB) configuration
- **Framework support**: Compatible frameworks list

Example board definition:
```json
{
  "native-x64-linux": {
    "name": "Native x86_64 Linux",
    "mcu": "x86_64",
    "fcpu": "3000000000L",
    "ram": 17179869184,
    "flash": 1099511627776,
    "vendor": "Generic",
    "product": "Native x86_64",
    "frameworks": ["fastled-stub", "arduino-stub"],
    "build": {
      "target": "x86_64-linux-gnu",
      "toolchain": "ziglang", 
      "cpu": "x86_64",
      "mcu": "native",
      "f_cpu": "3000000000L",
      "extra_flags": [
        "-DNATIVE_PLATFORM=1",
        "-DSTUB_PLATFORM=1", 
        "-DFASTLED_NATIVE_X64=1",
        "-DFASTLED_NATIVE_LINUX=1"
      ]
    },
    "upload": { "protocol": "native" },
    "debug": {
      "tools": {
        "gdb": {
          "server": {
            "executable": "gdb",
            "arguments": ["$PROG_PATH"]
          }
        }
      }
    }
  }
}
```

### Toolchain Mapping
Direct integration with ziglang commands:
```json
{
  "toolchain-ziglang": {
    "CC": "python -m ziglang cc",
    "CXX": "python -m ziglang c++",
    "AR": "python -m ziglang ar",
    "LD": "python -m ziglang c++",
    "OBJCOPY": "python -m ziglang objcopy",
    "NM": "python -m ziglang nm",
    "STRIP": "python -m ziglang strip",
    "RANLIB": "python -m ziglang ranlib"
  }
}
```

### Build Flag Hierarchies
```json
{
  "build_flags": {
    "base": [
      "-std=gnu++17",
      "-fpermissive", 
      "-Wall",
      "-Wextra",
      "-Wno-deprecated-register",
      "-Wno-backslash-newline-escape",
      "-fno-exceptions",
      "-fno-rtti",
      "-DSTUB_PLATFORM=1",
      "-DNATIVE_PLATFORM=1"
    ],
    "debug": [
      "-g3",
      "-Og",
      "-DDEBUG=1",
      "-DFASTLED_DEBUG=1"
    ],
    "release": [
      "-O3",
      "-DNDEBUG=1",
      "-flto"
    ],
    "test": [
      "-DFASTLED_UNIT_TEST=1",
      "-DFASTLED_FORCE_NAMESPACE=1",
      "-DFASTLED_TESTING=1",
      "-DFASTLED_USE_PROGMEM=0",
      "-DARDUINO=10808",
      "-DFASTLED_USE_STUB_ARDUINO=1",
      "-DSKETCH_HAS_LOTS_OF_MEMORY=1",
      "-DFASTLED_STUB_IMPL=1",
      "-DFASTLED_USE_JSON_UI=1",
      "-DFASTLED_NO_AUTO_NAMESPACE=1",
      "-DFASTLED_NO_PINMAP=1",
      "-DHAS_HARDWARE_PIN_SUPPORT=1",
      "-DFASTLED_DEBUG_LEVEL=1",
      "-DFASTLED_NO_ATEXIT=1",
      "-DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS=1"
    ]
  }
}
```

### Platform-Specific Linking
```json
{
  "link_flags": {
    "base": [],
    "linux": [
      "-pthread",
      "-lm",
      "-ldl"
    ],
    "macos": [
      "-pthread",
      "-lm"
    ],
    "windows": [
      "-mconsole",
      "-nodefaultlibs", 
      "-unwindlib=libunwind",
      "-nostdlib++",
      "-lc++",
      "-lkernel32",
      "-luser32",
      "-lgdi32",
      "-ladvapi32"
    ]
  }
}
```

## Integration with build_unit.toml

The `platform.json` is designed to seamlessly integrate with the existing `build_unit.toml` configuration:

### Toolchain Mapping

The `[tools]` section in `build_unit.toml` directly maps to the ziglang toolchain defined in `platform.json`:

- `cpp_compiler = ["python", "-m", "ziglang", "c++"]` → `CXX: "python -m ziglang c++"`
- `linker = ["python", "-m", "ziglang", "c++"]` → `LD: "python -m ziglang c++"`
- `archiver = ["python", "-m", "ziglang", "ar"]` → `AR: "python -m ziglang ar"`

### Build Flag Integration

The platform flags complement the TOML configuration:

- **Base flags**: Defined in both `platform.json` and `build_unit.toml` for consistency
- **Platform-specific flags**: Windows-specific flags from TOML are automatically applied
- **Test flags**: Unit test defines from TOML are included in the platform test configuration

### Target Selection

The platform supports automatic target detection or explicit target selection:

```bash
# Auto-detect target
platformio run -e native-auto

# Explicit target selection
platformio run -e native-x64-linux
platformio run -e native-arm64-macos
platformio run -e native-x64-windows
```

## Integration Benefits

### Seamless TOML Integration
The design ensures perfect compatibility with existing `build_unit.toml`:
- Toolchain commands match exactly: `["python", "-m", "ziglang", "c++"]`
- Build flags complement rather than duplicate TOML configuration
- Platform-specific flags automatically applied based on detection

### Enhanced Development Workflow
- **Single Command Compilation**: `platformio run -e native-auto`
- **Cross-Compilation Support**: Build for different targets from any host
- **Testing Integration**: Full unit test support with proper namespace isolation
- **Debug Builds**: Integrated debugging with platform-appropriate tools

### Future-Proof Architecture
- **Extensible Design**: Easy addition of new architectures and targets
- **WebAssembly Ready**: Structure supports future WASM integration
- **Container Builds**: Foundation for Docker/Podman integration
- **Static Analysis**: Framework for clang-tidy and analyzer integration

## Usage Examples

### Basic Compilation

```bash
# Compile for current platform (auto-detect)
platformio run -e native-auto

# Compile for specific platform
platformio run -e native-x64-linux
```

### Testing

```bash
# Run unit tests
platformio test -e native-auto

# Run tests with specific build flags
platformio test -e native-x64-linux --test-port=native
```

### Cross-Compilation

```bash
# Compile for ARM64 Linux from x86_64
platformio run -e native-arm64-linux

# Compile for Windows from Linux
platformio run -e native-x64-windows
```

### Debug Builds

```bash
# Debug build with symbols
platformio run -e native-auto --build-type=debug

# Release build with optimization
platformio run -e native-auto --build-type=release
```

## Advanced Configuration

### Custom Board Definition

```json
{
  "native-custom": {
    "name": "Custom Native Target",
    "mcu": "custom",
    "build": {
      "target": "x86_64-linux-musl", 
      "extra_flags": [
        "-DCUSTOM_DEFINE=1",
        "-static"
      ]
    }
  }
}
```

### Environment Variables

The platform respects environment variables for customization:

- `ZIG_TARGET`: Override target triple (e.g., `x86_64-linux-musl`)
- `ZIG_CPU`: Override CPU type (e.g., `x86_64_v3`)
- `FASTLED_NATIVE_FLAGS`: Additional compiler flags

### Build System Integration

The platform integrates with multiple build systems:

- **PlatformIO**: Primary integration with comprehensive board support
- **CMake**: Compatible with existing CMake configurations
- **Direct Compilation**: Can be used directly with ziglang commands

## Complete Board Definitions

The platform.json includes 8 comprehensive board definitions:

1. **native-x64-linux**: x86_64 Linux with GNU target
2. **native-x64-macos**: x86_64 macOS with Apple-specific configuration
3. **native-x64-windows**: x86_64 Windows with MinGW compatibility
4. **native-arm64-linux**: ARM64 Linux with Cortex-A76 optimization
5. **native-arm64-macos**: ARM64 macOS with Apple M1 optimization
6. **native-arm64-windows**: ARM64 Windows with appropriate runtime
7. **native-riscv64-linux**: RISC-V 64-bit Linux support
8. **native-auto**: Automatic target detection and selection

## Tool Dependencies

The platform includes comprehensive tool support:

```json
{
  "tools": {
    "python-ziglang": {
      "type": "compiler",
      "version": "^0.13.0",
      "description": "Zig toolchain with bundled Clang/LLVM",
      "install": { "pip": "ziglang" }
    },
    "cmake": {
      "type": "build-system",
      "version": "^3.21.0",
      "optional": true
    },
    "ninja": {
      "type": "build-system", 
      "version": "^1.11.0",
      "optional": true
    },
    "ccache": {
      "type": "compiler-cache",
      "version": "^4.0.0",
      "optional": true
    },
    "sccache": {
      "type": "compiler-cache",
      "version": "^0.7.0",
      "optional": true
    }
  }
}
```

## Target Compatibility

Platform compatibility specifications ensure proper deployment:

```json
{
  "target_compatibility": {
    "x86_64-linux-gnu": {
      "glibc": ">=2.31",
      "kernel": ">=5.4.0"
    },
    "x86_64-macos-none": {
      "macos": ">=11.0"
    },
    "x86_64-windows-gnu": {
      "windows": ">=10"
    },
    "aarch64-linux-gnu": {
      "glibc": ">=2.31", 
      "kernel": ">=5.4.0"
    },
    "aarch64-macos-none": {
      "macos": ">=11.0"
    },
    "aarch64-windows-gnu": {
      "windows": ">=10"
    },
    "riscv64-linux-gnu": {
      "glibc": ">=2.31",
      "kernel": ">=5.10.0"
    }
  }
}
```

## Troubleshooting

### Common Issues

1. **Ziglang not found**: Ensure `ziglang` Python package is installed
2. **Target not supported**: Check `zig targets` for available targets
3. **Linking errors**: Verify platform-specific link flags in configuration

### Debug Information

```bash
# Check available targets
zig targets

# Verify toolchain installation
python -m ziglang version

# Test compilation
python -m ziglang c++ --version
```

## Implementation Status

This design is **ready for implementation** and provides:
- ✅ **Complete board definitions** for all major native platforms
- ✅ **Full toolchain integration** with existing ziglang setup  
- ✅ **Comprehensive build configurations** for all use cases
- ✅ **Arduino package format compliance** for PlatformIO compatibility
- ✅ **Debug and development tool integration** for professional development
- ✅ **Future-proof extensible architecture** for additional platforms

The configuration can be deployed alongside the existing build system for gradual validation and testing before full migration.

## Future Enhancements

- **WebAssembly Support**: Integration with FastLED WASM compilation
- **Static Analysis**: Integration with clang-tidy and static analyzers
- **Performance Profiling**: Built-in profiling support for optimization
- **Container Builds**: Docker/Podman integration for reproducible builds
- **Package Registry**: Publication to Arduino package index for distribution

## Conclusion

This platform.json design provides a comprehensive, production-ready configuration for FastLED's native build platform. It seamlessly integrates with the existing `build_unit.toml` while providing the standardized Arduino package format required for broader ecosystem compatibility. The design is thoroughly tested conceptually and ready for implementation with proper validation and testing procedures.

# Design Document: IWYU Installation for Windows with uv Integration

## Overview

This document outlines a design for automated Include What You Use (IWYU) installation on Windows systems using the uv package manager, integrated with the existing FastLED project infrastructure.

## Background

Include What You Use (IWYU) is a Clang-based tool that analyzes C++ `#include` directives to suggest optimizations. While FastLED already has comprehensive IWYU integration for Linux and macOS, Windows installation remains challenging due to:

- **Complex build requirements** - IWYU must be built against specific Clang versions
- **Toolchain dependencies** - Requires LLVM/Clang development libraries
- **Binary availability** - Limited pre-built Windows binaries
- **Path configuration** - Complex setup for header discovery

## Current State Analysis

### Existing FastLED IWYU Integration

FastLED already provides sophisticated IWYU support:

```cmake
# tests/cmake/StaticAnalysis.cmake
message(WARNING "🚫 include-what-you-use requested but NOT FOUND")
message(WARNING "   No IWYU analysis will be performed")
message(WARNING "   Install it with:")
message(WARNING "     Ubuntu/Debian: sudo apt install iwyu")
message(WARNING "     macOS: brew install include-what-you-use")
message(WARNING "     Windows: See STATIC_ANALYSIS.md for installation guide")
message(WARNING "     Or build from source: https://include-what-you-use.org/")
```

```python
# ci/ci-iwyu.py
if not check_iwyu_available():
    print("Error: include-what-you-use not found in PATH")
    print("Install it with:")
    print("  Ubuntu/Debian: sudo apt install iwyu")
    print("  macOS: brew install include-what-you-use")
    print("  Or build from source: https://include-what-you-use.org/")
    return 1
```

### Windows Installation Challenges

Current Windows options require significant technical expertise:

1. **Build from source** - Requires LLVM build environment
2. **MSYS2** - Requires separate package manager (`pacman -S mingw-w64-clang-x86_64-include-what-you-use`)
3. **Community binaries** - Limited availability and trust concerns

## Proposed Solution Architecture

### Core Design Principles

1. **uv-native approach** - Leverage uv's cross-platform capabilities
2. **Seamless integration** - Work with existing FastLED IWYU infrastructure  
3. **Automatic dependency management** - Handle IWYU and toolchain requirements
4. **Progressive enhancement** - Graceful fallback when IWYU unavailable

### Implementation Strategy

#### Phase 1: uv Tool Integration

Create a Python package that provides IWYU for Windows through uv's tool system:

```python
# New file: ci/install_iwyu.py
"""
IWYU installation manager for Windows using uv
"""
import subprocess
import sys
from pathlib import Path

def install_iwyu_windows():
    """Install IWYU on Windows via uv tool system"""
    
    # Option 1: Custom Python package that bundles IWYU
    try:
        subprocess.run([
            "uv", "tool", "install", "iwyu-windows-binary"
        ], check=True)
        return True
    except subprocess.CalledProcessError:
        pass
    
    # Option 2: Use existing Python IWYU wrapper
    try:
        subprocess.run([
            "uv", "tool", "install", "include-what-you-use"
        ], check=True)
        return True
    except subprocess.CalledProcessError:
        pass
        
    # Option 3: Download and install pre-built binary
    return install_iwyu_binary()

def install_iwyu_binary():
    """Download and install pre-built IWYU binary"""
    # Implementation for downloading from GitHub releases
    # Similar to how uv itself is distributed
    pass
```

#### Phase 2: pyproject.toml Integration

Add IWYU as an optional development dependency:

```toml
# Addition to pyproject.toml
[project.optional-dependencies]
dev-iwyu = [
    "iwyu-windows-binary>=0.24.0; sys_platform == 'win32'",
    # Alternative: custom wrapper package
]

[tool.uv.sources]
iwyu-windows-binary = { url = "https://github.com/fastled/iwyu-windows/releases/latest/download/iwyu-windows-{python_version}-win_amd64.whl", marker = "sys_platform == 'win32'" }
```

#### Phase 3: Enhanced Installation Scripts

Update existing scripts to use uv for Windows IWYU installation:

```python
# Enhancement to ci/ci-iwyu.py
def install_iwyu_if_missing():
    """Attempt to install IWYU if not available"""
    if check_iwyu_available():
        return True
        
    if not sys.platform.startswith('win'):
        return False  # Use system package manager on other platforms
        
    print("🔄 IWYU not found. Attempting automatic installation via uv...")
    
    try:
        # Try uv tool installation first
        result = subprocess.run([
            "uv", "tool", "install", "iwyu-wrapper"
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            print("✅ IWYU installed successfully via uv tool")
            return check_iwyu_available()
            
    except Exception as e:
        print(f"⚠️ Automatic installation failed: {e}")
        
    # Provide manual installation guidance
    print_windows_installation_guide()
    return False

def print_windows_installation_guide():
    """Enhanced Windows installation instructions"""
    print("\n📋 Windows IWYU Installation Options:")
    print("1. ✅ RECOMMENDED: Use uv tool system")
    print("   uv tool install iwyu-wrapper")
    print("2. 📦 Use MSYS2 package manager:")
    print("   pacman -S mingw-w64-clang-x86_64-include-what-you-use")
    print("3. 🔨 Build from source (advanced):")
    print("   See: https://include-what-you-use.org/")
    print("4. 📥 Download pre-built binary:")
    print("   See: https://github.com/Agrael1/BuildIWYU")
```

#### Phase 4: MCP Server Integration

Enhance the MCP server to handle IWYU installation:

```python
# Enhancement to mcp_server.py
@mcp_server.tool()
def setup_iwyu(
    auto_install: bool = True,
    method: str = "auto"  # "auto", "uv", "msys2", "manual"
) -> dict:
    """Setup IWYU for static analysis on Windows"""
    
    if check_iwyu_available():
        return {
            "status": "success", 
            "message": "IWYU already available",
            "version": get_iwyu_version()
        }
    
    if not auto_install:
        return {
            "status": "info",
            "message": "IWYU not found. Use auto_install=True to install automatically"
        }
    
    if method == "auto" or method == "uv":
        success = install_iwyu_via_uv()
        if success:
            return {
                "status": "success",
                "message": "IWYU installed successfully via uv",
                "method": "uv_tool"
            }
    
    # Fallback methods...
    return provide_installation_guidance()

def install_iwyu_via_uv() -> bool:
    """Install IWYU using uv tool system"""
    try:
        # Method 1: Pre-built wheel
        subprocess.run([
            "uv", "tool", "install", 
            "--from", "https://github.com/fastled/iwyu-windows/releases/latest/download/iwyu-wrapper.whl"
        ], check=True)
        return True
    except subprocess.CalledProcessError:
        pass
    
    # Method 2: PyPI package (if available)
    try:
        subprocess.run(["uv", "tool", "install", "iwyu-wrapper"], check=True)
        return True
    except subprocess.CalledProcessError:
        pass
        
    return False
```

## Implementation Details

### Custom IWYU Windows Package

Create a Python package that bundles IWYU for Windows:

```python
# Structure: iwyu-windows-binary/
# ├── pyproject.toml
# ├── src/
# │   └── iwyu_windows/
# │       ├── __init__.py
# │       ├── bin/
# │       │   ├── include-what-you-use.exe
# │       │   ├── fix_includes.py
# │       │   └── iwyu_tool.py
# │       └── mappings/
# │           ├── stl.imp
# │           └── boost.imp
# └── scripts/
#     └── include-what-you-use

# pyproject.toml for iwyu-windows-binary
[project]
name = "iwyu-windows-binary"
version = "0.24.0"
description = "Include What You Use (IWYU) binary distribution for Windows"
dependencies = []

[project.scripts]
include-what-you-use = "iwyu_windows:main"
fix_includes = "iwyu_windows:fix_includes_main"
iwyu_tool = "iwyu_windows:iwyu_tool_main"

[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"
```

### Enhanced Error Handling and User Experience

```python
# Enhanced user experience in ci/ci-iwyu.py
def main() -> int:
    args = parse_args()
    
    # Enhanced availability check with installation option
    if not check_iwyu_available():
        if sys.platform.startswith('win') and args.auto_install:
            print("🔄 IWYU not found on Windows. Attempting automatic installation...")
            if install_iwyu_via_uv():
                print("✅ IWYU installation successful!")
            else:
                print("❌ Automatic installation failed.")
                return suggest_manual_installation()
        else:
            return handle_iwyu_not_found()
    
    # Continue with existing logic...
```

### Integration with Existing Build System

Update CMake to auto-detect uv-installed IWYU:

```cmake
# Enhancement to tests/cmake/StaticAnalysis.cmake
function(find_iwyu_windows)
    # Check if uv tool provides IWYU
    execute_process(
        COMMAND uv tool list
        OUTPUT_VARIABLE UV_TOOLS_OUTPUT
        ERROR_QUIET
    )
    
    if(UV_TOOLS_OUTPUT MATCHES "iwyu-wrapper")
        execute_process(
            COMMAND uv tool run iwyu-wrapper -- --version
            OUTPUT_VARIABLE IWYU_VERSION_OUTPUT
            ERROR_QUIET
        )
        
        if(IWYU_VERSION_OUTPUT MATCHES "include-what-you-use")
            set(IWYU_EXE "uv tool run iwyu-wrapper --" PARENT_SCOPE)
            message(STATUS "Found IWYU via uv tool: ${IWYU_EXE}")
            return()
        endif()
    endif()
    
    # Fallback to standard detection
    find_program(IWYU_EXE NAMES include-what-you-use)
endfunction()
```

## Distribution Strategy

### Option 1: Standalone Binary Package

Create a self-contained Python wheel with IWYU binary:

```
iwyu-windows-binary-0.24.0-py3-none-win_amd64.whl
├── Pre-built include-what-you-use.exe (from LLVM)
├── Required DLLs (Clang libraries)
├── Python wrapper scripts
└── Standard library mappings
```

**Pros:**
- ✅ Simple installation: `uv tool install iwyu-windows-binary`
- ✅ No external dependencies
- ✅ Version-controlled and reproducible

**Cons:**
- ❌ Large package size (~50-100MB)
- ❌ Requires building/maintaining binaries
- ❌ Platform-specific (Windows only)

### Option 2: Installer Package

Create a Python package that downloads and installs IWYU:

```python
# iwyu-installer package
def install():
    """Download and install IWYU from official sources"""
    # Download from GitHub releases or LLVM
    # Extract to appropriate location
    # Update PATH or create wrapper scripts
```

**Pros:**
- ✅ Smaller package size
- ✅ Always gets latest IWYU version
- ✅ Can adapt to different Windows environments

**Cons:**
- ❌ Requires internet connection during installation
- ❌ More complex error handling
- ❌ Dependency on external download sources

### Option 3: Hybrid Approach (Recommended)

Combine both strategies with automatic fallback:

1. **Try PyPI package** - `uv tool install iwyu-windows`
2. **Download binary** - If package unavailable, download from GitHub releases
3. **Guide to alternatives** - MSYS2, manual installation, building from source

## Testing Strategy

### Automated Testing

```python
# New file: ci/test_iwyu_windows.py
"""Test IWYU Windows installation methods"""

import pytest
import subprocess
import sys

@pytest.mark.skipif(not sys.platform.startswith('win'), reason="Windows only")
def test_iwyu_uv_installation():
    """Test IWYU installation via uv"""
    # Clean environment
    subprocess.run(["uv", "tool", "uninstall", "iwyu-wrapper"], 
                  capture_output=True)
    
    # Install
    result = subprocess.run([
        "uv", "tool", "install", "iwyu-wrapper"
    ], capture_output=True, text=True)
    
    assert result.returncode == 0
    
    # Verify availability
    result = subprocess.run([
        "uv", "tool", "run", "iwyu-wrapper", "--", "--version"
    ], capture_output=True, text=True)
    
    assert "include-what-you-use" in result.stdout

@pytest.mark.skipif(not sys.platform.startswith('win'), reason="Windows only")  
def test_iwyu_analysis_integration():
    """Test IWYU analysis works with FastLED build system"""
    # Run IWYU on simple C++ test
    result = subprocess.run([
        "uv", "run", "ci/ci-iwyu.py", "--verbose"
    ], capture_output=True, text=True)
    
    # Should not fail due to IWYU unavailability
    assert "include-what-you-use not found" not in result.stderr
```

### Manual Testing Checklist

- [ ] Installation via `uv tool install iwyu-wrapper`
- [ ] Integration with `bash test --check`
- [ ] CMake detection and configuration
- [ ] FastLED C++ code analysis
- [ ] Error handling for missing dependencies
- [ ] Performance comparison with Linux/macOS

## Documentation Updates

### Update STATIC_ANALYSIS.md

```markdown
### Windows Installation (New Section)

#### Recommended: uv Tool Installation

The easiest way to install IWYU on Windows is using uv's tool system:

```bash
uv tool install iwyu-wrapper
```

This automatically downloads and configures IWYU with all required dependencies.

#### Verify Installation

```bash
uv tool run iwyu-wrapper -- --version
```

#### Integration with FastLED

Once installed, IWYU works seamlessly with FastLED's analysis tools:

```bash
# Automatic detection and usage
bash test --check
uv run ci/ci-iwyu.py
```

#### Alternative Methods

If uv installation fails, try these alternatives:

1. **MSYS2 Package Manager:**
   ```bash
   pacman -S mingw-w64-clang-x86_64-include-what-you-use
   ```

2. **Pre-built Binaries:**
   Download from: https://github.com/Agrael1/BuildIWYU

3. **Build from Source:**
   See: https://include-what-you-use.org/
```

### Update .cursorrules

```markdown
## 🔧 IWYU (Include What You Use) - Windows Support

### AUTOMATIC INSTALLATION (NEW - Windows)
**🚨 FOR WINDOWS USERS:** IWYU can now be automatically installed via uv:

```bash
# One-command installation
uv tool install iwyu-wrapper

# Verify installation  
uv tool run iwyu-wrapper -- --version

# Use with FastLED
bash test --check
uv run ci/ci-iwyu.py
```

### BACKGROUND AGENTS - Windows IWYU Setup
**Use MCP server `setup_iwyu` tool:**
```bash
uv run mcp_server.py
# Then use setup_iwyu tool with auto_install: true
```
```

## Migration Plan

### Phase 1: Package Development (Week 1-2)
- [ ] Create `iwyu-windows-binary` package
- [ ] Build and test IWYU Windows binaries
- [ ] Set up GitHub releases for distribution
- [ ] Initial PyPI package publication

### Phase 2: Integration (Week 3)  
- [ ] Update `ci/ci-iwyu.py` with Windows installation logic
- [ ] Enhance MCP server with `setup_iwyu` tool
- [ ] Update CMake detection for uv-installed IWYU
- [ ] Add Windows-specific error handling

### Phase 3: Testing and Documentation (Week 4)
- [ ] Comprehensive Windows testing
- [ ] Update documentation (STATIC_ANALYSIS.md, .cursorrules)
- [ ] Create installation troubleshooting guide
- [ ] Performance benchmarking

### Phase 4: Release and Monitoring (Week 5)
- [ ] Announce Windows IWYU support
- [ ] Monitor installation success rates
- [ ] Gather user feedback
- [ ] Iterate based on real-world usage

## Success Metrics

### Technical Metrics
- **Installation success rate**: >95% on Windows 10/11
- **Analysis performance**: Within 10% of Linux performance
- **Integration reliability**: Zero build system conflicts

### User Experience Metrics  
- **Installation time**: <2 minutes including dependencies
- **Documentation clarity**: <5 support requests per month
- **Error recovery**: Automatic fallback success rate >80%

## Risk Assessment and Mitigation

### High Risk: Binary Distribution
**Risk**: Legal/licensing issues with distributing LLVM/Clang binaries  
**Mitigation**: Use official LLVM releases, clear attribution, legal review

### Medium Risk: Dependency Management
**Risk**: Version conflicts between IWYU and system Clang  
**Mitigation**: Isolated installation, version pinning, compatibility testing

### Low Risk: Performance Impact
**Risk**: Slower analysis compared to native installation  
**Mitigation**: Benchmarking, optimization, alternative implementation paths

## Future Enhancements

### Advanced Features
- **Automatic updates**: `uv tool upgrade iwyu-wrapper`
- **Version management**: Multiple IWYU versions for different Clang versions
- **IDE integration**: VS Code extension, CLion plugin support
- **CI/CD optimization**: Cached installations, parallel analysis

### Cross-Platform Consistency
- **Unified interface**: Same commands work on all platforms
- **Shared configuration**: Common mapping files and settings
- **Performance parity**: Windows performance matching Linux/macOS

This design provides a comprehensive approach to bringing IWYU to Windows users through uv's powerful package management capabilities, while maintaining compatibility with FastLED's existing sophisticated static analysis infrastructure. 

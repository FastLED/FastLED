# New PlatformIO Testing System Design

## Overview

This document describes the design for a new testing system for FastLED that uses PlatformIO's `pio run` command directly with symlink directives and runtime source directory configuration.

## System Architecture

### Core Components

1. **Test Script**: `ci/tests/test_new_platformio.py` - Main test implementation
2. **Root Script**: `uno2.py` - Convenience script for full UNO build testing
3. **PlatformIO Configuration**: Dynamic `platformio.ini` generation with symlinks
4. **Example Source Management**: Runtime configuration of source directories

### Key Features

- **Direct PlatformIO Integration**: Uses `pio run` instead of `pio ci`
- **Symlink Library Dependencies**: FastLED library linked via `symlink://` directive
- **Runtime Source Directory**: Examples configured as source directory at runtime
- **Automatic Cache Management**: PlatformIO `.pio` cache handled automatically
- **Project Root Resolution**: Automatic detection of FastLED project root

## Technical Implementation

### 1. Project Root Resolution

```python
def resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(__file__).parent.resolve()
    while current != current.parent:
        if (current / "src" / "FastLED.h").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find FastLED project root")
```

### 2. PlatformIO Configuration Generation

The system generates a dynamic `platformio.ini` file with:

```ini
[platformio]
src_dir = {example_path}  ; Runtime configured to examples/Blink/

[env:uno]
platform = atmelavr
board = uno
framework = arduino

# FastLED library dependency (symlinked for efficiency)
lib_deps = symlink://{fastled_src_path}

# LDF Configuration
lib_ldf_mode = deep+
lib_compat_mode = off

# Build optimization
build_cache_enable = true
```

### 3. Build Directory Management

- **Base Directory**: `.build/test_platformio/uno/`
- **Cache Directory**: `.build/test_platformio/uno/.pio/`
- **Source Symlink**: Points to selected example directory
- **Library Symlink**: Points to FastLED `src/` directory

### 4. Example Source Configuration

The system supports flexible example selection:

```python
def configure_example_source(project_dir: Path, example_name: str) -> Path:
    """Configure the source directory to point to specified example."""
    project_root = resolve_project_root()
    example_path = project_root / "examples" / example_name
    
    if not example_path.exists():
        raise FileNotFoundError(f"Example not found: {example_path}")
    
    return example_path
```

### 5. PlatformIO Command Execution

```python
def run_pio_build(project_dir: Path, verbose: bool = False) -> Tuple[bool, str]:
    """Execute pio run command against the configured project."""
    cmd = ["pio", "run", "--project-dir", str(project_dir)]
    
    if verbose:
        cmd.append("--verbose")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0, result.stdout + result.stderr
```

## Test Implementation: `ci/tests/test_new_platformio.py`

### Core Test Class

```python
class TestNewPlatformIO(unittest.TestCase):
    """Test new PlatformIO testing system."""
    
    def setUp(self) -> None:
        """Set up test environment."""
        self.project_root = resolve_project_root()
        self.test_dir = Path(".build/test_platformio")
        self.board_name = "uno"
        
    def test_uno_blink_example(self) -> None:
        """Test building Blink example for UNO target."""
        # Resolve project root
        project_root = resolve_project_root()
        
        # Set up build directory
        project_dir = self.test_dir / self.board_name
        project_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure example source
        example_path = configure_example_source(project_dir, "Blink")
        
        # Generate platformio.ini
        success, msg = generate_platformio_ini(
            project_dir, self.board_name, project_root, example_path
        )
        self.assertTrue(success, f"Failed to generate platformio.ini: {msg}")
        
        # Run pio build
        success, output = run_pio_build(project_dir)
        self.assertTrue(success, f"Build failed: {output}")
        
    def test_multiple_examples(self) -> None:
        """Test building multiple examples in sequence."""
        examples = ["Blink", "DemoReel100", "ColorPalette"]
        
        for example_name in examples:
            with self.subTest(example=example_name):
                # Configure and build each example
                self._build_example(example_name)
                
    def _build_example(self, example_name: str) -> None:
        """Helper to build a single example."""
        project_dir = self.test_dir / self.board_name
        project_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure example source
        example_path = configure_example_source(project_dir, example_name)
        
        # Update platformio.ini for new example
        success, msg = generate_platformio_ini(
            project_dir, self.board_name, self.project_root, example_path
        )
        self.assertTrue(success, f"Failed to configure {example_name}: {msg}")
        
        # Run build
        success, output = run_pio_build(project_dir)
        self.assertTrue(success, f"Failed to build {example_name}: {output}")
```

### Utility Functions

```python
def generate_platformio_ini(
    project_dir: Path, 
    board_name: str, 
    project_root: Path, 
    example_path: Path
) -> Tuple[bool, str]:
    """Generate platformio.ini with symlink configuration."""
    
    fastled_src_path = project_root / "src"
    
    content = f"""[platformio]
src_dir = {example_path}

[env:{board_name}]
platform = atmelavr
board = {board_name}
framework = arduino

# FastLED library dependency (symlinked for efficiency)
lib_deps = symlink://{fastled_src_path}

# LDF Configuration
lib_ldf_mode = deep+
lib_compat_mode = off

# Build optimization
build_cache_enable = true
"""
    
    try:
        platformio_ini = project_dir / "platformio.ini"
        platformio_ini.write_text(content)
        return True, "platformio.ini generated successfully"
    except Exception as e:
        return False, f"Failed to write platformio.ini: {e}"
```

## Advantages of This System

### 1. **Direct PlatformIO Integration**
- Uses `pio run` which is more efficient than `pio ci`
- Leverages PlatformIO's native caching mechanisms
- Better integration with PlatformIO's project structure

### 2. **Symlink Efficiency**
- FastLED library linked via symlink for immediate reflection of changes
- No copying of library files required
- Faster builds due to symlink efficiency

### 3. **Runtime Source Configuration**
- Examples can be switched without regenerating entire project
- Flexible testing of different examples
- Easy integration with CI/CD systems

### 4. **Automatic Cache Management**
- PlatformIO `.pio` cache handled automatically
- Incremental builds benefit from cached artifacts
- Faster subsequent builds

### 5. **Clean Project Structure**
- Isolated build directories
- No interference with existing build systems
- Easy cleanup and maintenance

## Integration with Existing Systems

### Compatibility
- Works alongside existing `ci-compile.py` system
- Uses separate build directories to avoid conflicts
- Can be integrated into current CI workflows

### Migration Path
- Gradual adoption possible
- Existing tests remain functional
- Can be used for specific testing scenarios

## Testing Strategy

### Unit Tests
- Test project root resolution
- Test platformio.ini generation
- Test symlink configuration
- Test build execution

### Integration Tests
- Test with various examples
- Test incremental builds
- Test cache behavior
- Test error handling

### Performance Tests
- Compare build times with current system
- Measure cache effectiveness
- Benchmark different example sizes

## Future Enhancements

### Multi-Board Support
- Extend to support multiple board targets
- Dynamic board configuration
- Parallel board builds

### Enhanced Caching
- Shared cache across examples
- Intelligent cache invalidation
- Cross-project cache sharing

### CI Integration
- GitHub Actions integration
- Automated testing workflows
- Performance monitoring

## Conclusion

This new testing system provides a more efficient and flexible approach to testing FastLED with PlatformIO. By leveraging symlinks, runtime configuration, and native PlatformIO features, it offers improved build performance and better integration with the PlatformIO ecosystem.

The system is designed to complement existing testing infrastructure while providing a modern, efficient alternative for specific testing scenarios.

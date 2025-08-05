# FEATURE: Efficient Multi-Platform Build System Using pio run

## Current Problem

FastLED currently uses `pio ci` for multi-platform compilation testing, which is extremely inefficient:

- **Each `pio ci` call creates a new project from scratch**
- **Downloads dependencies repeatedly** for each example/board combination
- **Runs Library Dependency Finder (LDF) every time** even when dependencies haven't changed
- **No shared artifacts** between builds
- **No incremental compilation benefits**

Example current workflow:
```bash
pio ci example1.ino --board uno --lib src/
pio ci example2.ino --board uno --lib src/  # Downloads deps again, runs LDF again
pio ci example1.ino --board esp32dev --lib src/  # Downloads deps again, runs LDF again
```

This results in:
- **20-30 seconds per example** instead of 2-5 seconds for incremental builds
- **Gigabytes of redundant downloads** across all board/example combinations
- **CPU cycles wasted** on repetitive dependency resolution

## Proposed Solution: pio run with --project-init

### Architecture Overview

Replace `pio ci` with a two-phase approach:

1. **Initialization Phase**: Use `pio project init` + `pio run` with full LDF for dependency establishment
2. **Incremental Phase**: Use `pio run --disable-auto-clean` with LDF disabled for all subsequent builds

### Phase 1: Project Initialization (First Run Only)

For each board, create a persistent project structure:

```bash
# Create project structure for board
pio project init --board uno --project-dir .build/projects/uno

# Configure platformio.ini with FastLED dependencies
cat >> .build/projects/uno/platformio.ini << EOF
lib_deps = symlink://../../src
lib_ldf_mode = chain  # Full dependency scanning
lib_compat_mode = soft
EOF

# First build to establish dependencies and download packages
pio run --project-dir .build/projects/uno
```

This phase:
- ✅ **Downloads all platform packages** (toolchains, frameworks)
- ✅ **Resolves all library dependencies** with full LDF scanning
- ✅ **Compiles framework libraries** (Arduino core, etc.)
- ✅ **Establishes build cache** and dependency graph

### Phase 2: Incremental Compilation (All Subsequent Builds)

For each example, use the established project with optimizations:

```bash
# Copy example to project src/ directory
cp examples/Blink/Blink.ino .build/projects/uno/src/main.ino

# Build with optimizations disabled
pio run --project-dir .build/projects/uno \
        --disable-auto-clean \
        --project-option="lib_ldf_mode=off"
```

This phase:
- ✅ **Skips dependency resolution** (LDF disabled)
- ✅ **Reuses compiled framework libraries**
- ✅ **Preserves build artifacts** (no auto-clean)
- ✅ **Only compiles changed source files**

### Manual Artifact Cleanup Strategy

Since `--disable-auto-clean` prevents automatic cleanup:

```bash
# Clean artifacts manually when needed
pio run --target clean --project-dir .build/projects/uno

# Or clean specific object files
rm -rf .build/projects/uno/.pio/build/uno/src/
```

Cleanup triggers:
- **platformio.ini changes** (board configuration updates)
- **Source directory structure changes** (new/removed header directories)
- **Dependency updates** (FastLED version changes)
- **Build flag changes** (optimization levels, defines)

## Implementation Plan

### Directory Structure

```
.build/
├── projects/           # Persistent project directories
│   ├── uno/           # Arduino UNO project
│   │   ├── platformio.ini
│   │   ├── src/       # Example source files (copied here)
│   │   └── .pio/      # Build artifacts (preserved)
│   ├── esp32dev/      # ESP32 project
│   └── teensy31/      # Teensy 3.1 project
└── cache/             # Shared package cache
    └── packages/      # Downloaded toolchains/frameworks
```

### Algorithm Flow

```python
def compile_multi_platform(boards: List[str], examples: List[str]):
    for board in boards:
        project_dir = f".build/projects/{board}"
        
        # Phase 1: Initialize if needed
        if not project_exists(project_dir) or deps_changed():
            initialize_project(board, project_dir)
            build_initial(project_dir)  # Full LDF, download deps
        
        # Phase 2: Incremental builds
        for example in examples:
            copy_example_to_project(example, project_dir)
            build_incremental(project_dir)  # LDF off, no auto-clean
            
        # Optional: Clean artifacts for next board
        if clean_between_boards:
            clean_project_artifacts(project_dir)
```

### Configuration Changes

**platformio.ini template for each board:**
```ini
[env:{board}]
platform = {platform}
board = {board}
framework = arduino

# FastLED as symlinked dependency
lib_deps = symlink://../../src

# LDF Configuration (phase-dependent)
lib_ldf_mode = chain    # Phase 1: full scanning
lib_ldf_mode = off      # Phase 2: disabled

# Build optimizations
lib_compat_mode = soft
build_cache_dir = ../../cache
```

### API Design

```python
class EfficientBuilder:
    def __init__(self, base_dir: str = ".build"):
        self.base_dir = Path(base_dir)
        self.projects_dir = self.base_dir / "projects"
        self.cache_dir = self.base_dir / "cache"
    
    def initialize_board(self, board: Board) -> bool:
        """Phase 1: Create project and establish dependencies"""
        project_dir = self.projects_dir / board.name
        
        # Create project structure
        run_cmd(["pio", "project", "init", 
                 "--board", board.real_name,
                 "--project-dir", str(project_dir)])
        
        # Configure platformio.ini
        self.configure_platformio_ini(project_dir, board)
        
        # Initial build with full LDF
        return self.run_pio_build(project_dir, full_ldf=True)
    
    def build_example(self, board: Board, example: Path) -> bool:
        """Phase 2: Incremental build with optimizations"""
        project_dir = self.projects_dir / board.name
        
        # Copy example to project
        self.copy_example_source(example, project_dir / "src")
        
        # Build with optimizations
        return self.run_pio_build(project_dir, 
                                  disable_auto_clean=True,
                                  disable_ldf=True)
    
    def run_pio_build(self, project_dir: Path, 
                      disable_auto_clean: bool = False,
                      disable_ldf: bool = False,
                      full_ldf: bool = False) -> bool:
        """Execute pio run with specified optimizations"""
        cmd = ["pio", "run", "--project-dir", str(project_dir)]
        
        if disable_auto_clean:
            cmd.append("--disable-auto-clean")
        
        if disable_ldf:
            cmd.extend(["--project-option", "lib_ldf_mode=off"])
        elif full_ldf:
            cmd.extend(["--project-option", "lib_ldf_mode=chain"])
        
        return run_cmd(cmd).returncode == 0
```

## Performance Benefits

### Expected Improvements

**First Build (Initialization):**
- **Current**: 45-60 seconds per board (pio ci overhead)
- **New**: 30-45 seconds per board (pio run optimization)
- **Improvement**: 25-33% faster initialization

**Subsequent Builds (Incremental):**
- **Current**: 20-30 seconds per example (full pio ci)
- **New**: 2-5 seconds per example (incremental pio run)
- **Improvement**: 85-90% faster incremental builds

**Overall Multi-Platform Testing:**
- **Current**: 50 examples × 10 boards × 25 seconds = ~3.5 hours
- **New**: (10 boards × 45 seconds) + (500 builds × 3 seconds) = ~33 minutes
- **Improvement**: 84% reduction in total build time

### Resource Efficiency

**Network Traffic:**
- **Eliminate redundant downloads** of platform packages
- **Share dependency cache** across all projects
- **Reuse compiled libraries** between examples

**Storage Efficiency:**
- **Persistent build artifacts** prevent recompilation
- **Shared package cache** reduces disk usage
- **Incremental object files** only for changed sources

**CPU Utilization:**
- **Skip dependency resolution** for incremental builds
- **Reuse compiled framework** libraries
- **Parallel builds** possible with pio run -j flag

## Implementation Strategy

### Phase 1: Core Infrastructure
1. **Modify ci-compile.py** to support both pio ci (current) and pio run (new) modes
2. **Add project initialization** logic for board setup
3. **Implement artifact management** and cleanup strategies
4. **Add configuration templates** for platformio.ini generation

### Phase 2: Optimization Features
1. **Add LDF mode switching** (chain → off after initialization)
2. **Implement smart cleanup** triggers (detect when cleanup needed)
3. **Add shared cache management** for packages and dependencies
4. **Parallel project initialization** for multiple boards

### Phase 3: Advanced Features
1. **Dependency change detection** (when to re-initialize projects)
2. **Cross-example artifact sharing** (reuse compiled FastLED between examples)
3. **Build cache persistence** across CI runs
4. **Integration with existing symbol analysis** and size tracking

### Compatibility and Migration

**Backward Compatibility:**
- Keep existing `pio ci` mode as fallback option
- Add `--use-pio-run` flag to enable new system
- Gradual migration with A/B testing

**Risk Mitigation:**
- **Validate builds match** between pio ci and pio run outputs
- **Test on all supported platforms** before full migration
- **Monitor build times** and failure rates during transition

## Configuration Options

### Build Modes

```bash
# Current system (backward compatibility)
./compile --use-pio-ci uno --examples Blink

# New efficient system
./compile --use-pio-run uno --examples Blink

# Hybrid mode (new system with fallback)
./compile --efficient --fallback-pio-ci uno --examples Blink
```

### Cleanup Control

```bash
# Manual cleanup trigger
./compile --clean-projects uno esp32dev

# Automatic cleanup detection
./compile --auto-clean-when-needed uno --examples Blink

# Preserve artifacts across runs
./compile --preserve-artifacts uno --examples Blink
```

### Performance Tuning

```bash
# Parallel project initialization
./compile --parallel-init uno esp32dev teensy31

# Shared dependency caching
./compile --shared-cache --cache-dir /tmp/pio-cache

# Advanced LDF control
./compile --ldf-mode chain --disable-ldf-after-init
```

## Integration Points

### Existing Systems Integration

**Symbol Analysis:**
- Works with pio run build outputs
- Reuses existing ELF analysis tools
- Maintains compatibility with size tracking

**Build Info Generation:**
- Continues to generate build_info.json
- Uses pio run project directories
- Maintains board configuration data

**WASM Compilation:**
- Benefits from faster incremental builds
- Reuses established project structure
- Maintains existing docker integration

### CI/CD Integration

**GitHub Actions:**
- Faster PR validation builds
- Reduced CI runtime costs
- Better cache utilization

**Local Development:**
- Rapid iteration on examples
- Faster multi-platform testing
- Improved developer experience

## Success Metrics

### Performance Benchmarks

**Build Time Reduction:**
- Target: 80%+ reduction in multi-platform build time
- Measure: Before/after comparison on standard example set
- Monitor: Build time per example and total test suite time

**Resource Utilization:**
- Target: 60%+ reduction in network downloads
- Target: 50%+ reduction in CPU utilization
- Measure: Package download frequency and compilation cycles

### Quality Assurance

**Build Compatibility:**
- Ensure pio run outputs match pio ci outputs exactly
- Validate all supported board/example combinations
- Test edge cases and error conditions

**System Reliability:**
- Monitor build failure rates before/after migration
- Test cleanup and recovery mechanisms
- Validate cross-platform consistency

## Future Enhancements

### Advanced Optimizations

**Cross-Example Sharing:**
- Share compiled FastLED objects between examples
- Implement smart dependency tracking
- Advanced incremental compilation strategies

**Distributed Builds:**
- Multi-machine project distribution
- Shared remote cache systems
- Cloud-based compilation acceleration

**AI-Driven Optimization:**
- Predict when cleanup is needed
- Optimize build order for maximum cache reuse
- Dynamic LDF mode selection based on change patterns

This design provides a path to dramatically improve FastLED's multi-platform build performance while maintaining full compatibility and providing a smooth migration path.

## Integration Planning

This section details how the efficient pio run system will integrate into the existing FastLED codebase, leveraging current infrastructure while providing backward compatibility.

### Existing Infrastructure Analysis

FastLED has sophisticated build infrastructure that the new system must integrate with:

**Current Entry Points:**
- `./compile` - Bash wrapper script (entry point)
- `ci/ci-compile.py` - Main compilation orchestrator using pio ci
- `ci/util/concurrent_run.py` - Concurrent execution infrastructure
- `ci/compiler/compile_for_board.py` - Board-specific compilation logic

**Key Integration Points:**
- **Concurrent execution system** via `concurrent_run()` with ThreadPoolExecutor
- **Board configuration** via `ci/util/boards.py` and Board dataclass
- **Symbol analysis integration** with existing tools
- **Build info generation** for platform analysis
- **CLI argument parsing** and interactive board selection

### Integration Architecture

#### Phase 1: Extend Current System (Backward Compatible)

**Add pio run support alongside existing pio ci:**

```python
# ci/ci-compile.py - Enhanced main compilation function
def compile_with_pio_run(
    board: Board,
    example_paths: list[Path], 
    build_dir: str | None,
    defines: list[str],
    verbose: bool,
    use_persistent_projects: bool = True,
) -> tuple[bool, str]:
    """New efficient compilation using pio run with persistent projects"""
    
    if not use_persistent_projects:
        # Fallback to existing pio ci system
        return compile_with_pio_ci(board, example_paths, build_dir, defines, verbose)
    
    # Phase 1: Initialize persistent project if needed
    project_dir = ensure_project_initialized(board, build_dir, defines)
    
    # Phase 2: Incremental compilation for each example
    for example_path in example_paths:
        success, msg = compile_example_incremental(
            board, example_path, project_dir, defines, verbose
        )
        if not success:
            return False, msg
    
    return True, ""

# New CLI flag for enabling pio run system
def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(...)
    # ... existing arguments ...
    
    # New build system options
    parser.add_argument(
        "--use-pio-run", 
        action="store_true",
        help="Use efficient pio run system instead of pio ci"
    )
    parser.add_argument(
        "--clean-projects", 
        action="store_true",
        help="Clean persistent project directories before building"
    )
    parser.add_argument(
        "--disable-incremental", 
        action="store_true", 
        help="Disable incremental compilation optimizations"
    )
```

#### Phase 2: Integration with Concurrent Execution

**Extend `concurrent_run` infrastructure for pio run:**

```python
# ci/util/concurrent_run.py - Enhanced for pio run support
@dataclass
class ConcurrentRunArgs:
    # ... existing fields ...
    use_pio_run: bool = False              # Enable new system
    clean_projects: bool = False           # Clean before build
    disable_incremental: bool = False      # Force full rebuilds
    project_cache_dir: str | None = None   # Shared project cache

def concurrent_run(args: ConcurrentRunArgs) -> int:
    # ... existing initialization logic ...
    
    # NEW: Project initialization phase for pio run
    if args.use_pio_run:
        init_success = initialize_persistent_projects(args)
        if not init_success:
            return 1
    
    # Modified compilation logic to support both systems
    with ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board: Dict[Future[Any], Board] = {}
        
        for board in projects:
            # Choose compilation function based on system
            compile_func = (
                compile_examples_pio_run if args.use_pio_run 
                else compile_examples  # existing function
            )
            
            future = executor.submit(
                compile_func,
                board,
                examples + extra_examples.get(board, []),
                args.build_dir,
                args.verbose,
                libs=args.libs,
                incremental=not args.disable_incremental,
            )
            future_to_board[future] = board
        
        # ... existing completion handling ...

def initialize_persistent_projects(args: ConcurrentRunArgs) -> bool:
    """Initialize persistent project directories for all boards"""
    
    # Use parallel initialization for efficiency
    parallel_workers = min(len(args.projects), 4)  # Limit to prevent overload
    
    with ThreadPoolExecutor(max_workers=parallel_workers) as executor:
        future_to_board: Dict[Future[Any], Board] = {}
        
        for board in args.projects:
            future = executor.submit(
                initialize_single_project,
                board,
                args.build_dir,
                args.defines,
                args.clean_projects,
            )
            future_to_board[future] = board
        
        # Check all initialization results
        for future in as_completed(future_to_board):
            board = future_to_board[future]
            try:
                success, msg = future.result()
                if not success:
                    locked_print(f"Failed to initialize project for {board.board_name}: {msg}")
                    return False
                locked_print(f"Initialized project for {board.board_name}")
            except Exception as e:
                locked_print(f"Exception initializing {board.board_name}: {e}")
                return False
    
    return True
```

#### Phase 3: New Compilation Functions

**Add pio run specific compilation logic:**

```python
# ci/compiler/compile_for_board.py - Enhanced for pio run
def compile_examples_pio_run(
    board: Board,
    examples: list[Path],
    build_dir: str | None,
    verbose_on_failure: bool,
    libs: list[str] | None,
    incremental: bool = True,
) -> tuple[bool, str]:
    """Compile examples using efficient pio run system"""
    
    project_dir = get_project_directory(board, build_dir)
    
    for example in examples:
        locked_print(f"\n*** Building {example} for {board.board_name} (pio run) ***")
        
        # Copy example to project src directory
        success, msg = prepare_example_source(example, project_dir)
        if not success:
            return False, msg
        
        # Run incremental build
        success, msg = run_pio_build_incremental(
            project_dir, 
            board, 
            incremental=incremental,
            verbose=verbose_on_failure
        )
        if not success:
            return False, msg
    
    return True, ""

def initialize_single_project(
    board: Board,
    build_dir: str | None,
    defines: list[str],
    clean_first: bool,
) -> tuple[bool, str]:
    """Initialize a persistent project directory for a board"""
    
    project_dir = get_project_directory(board, build_dir)
    
    # Clean if requested
    if clean_first and project_dir.exists():
        shutil.rmtree(project_dir)
    
    # Skip if already initialized and valid
    if is_project_initialized(project_dir, board):
        return True, f"Project already initialized for {board.board_name}"
    
    # Create project structure
    cmd = [
        "pio", "project", "init",
        "--board", board.get_real_board_name(),
        "--project-dir", str(project_dir),
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return False, f"Failed to init project: {result.stderr}"
    
    # Configure platformio.ini
    success, msg = configure_project_platformio_ini(project_dir, board, defines)
    if not success:
        return False, msg
    
    # Initial build to establish dependencies
    success, msg = run_initial_project_build(project_dir, board)
    if not success:
        return False, msg
    
    return True, f"Successfully initialized project for {board.board_name}"

def run_pio_build_incremental(
    project_dir: Path,
    board: Board,
    incremental: bool,
    verbose: bool,
) -> tuple[bool, str]:
    """Run pio build with incremental optimizations"""
    
    cmd = ["pio", "run", "--project-dir", str(project_dir)]
    
    # Add incremental optimizations
    if incremental:
        cmd.append("--disable-auto-clean")
        cmd.extend(["--project-option", "lib_ldf_mode=off"])
    
    if verbose:
        cmd.append("--verbose")
    
    # Execute build
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode == 0:
        return True, "Build successful"
    else:
        return False, f"Build failed: {result.stderr}"
```

#### Phase 4: CLI Integration

**Enhanced command line interface:**

```python
# ci/ci-compile.py - Updated main function
def main() -> int:
    args = parse_args()
    
    # ... existing board and example resolution ...
    
    # Choose build system based on arguments
    if args.use_pio_run:
        locked_print("Using efficient pio run build system")
        
        # Create enhanced concurrent run args
        run_args = ConcurrentRunArgs(
            projects=boards,
            examples=example_paths,
            skip_init=args.skip_init,
            defines=defines,
            # ... existing args ...
            use_pio_run=True,
            clean_projects=args.clean_projects,
            disable_incremental=args.disable_incremental,
            project_cache_dir=args.build_dir,
        )
        
        return concurrent_run(args=run_args)
    else:
        locked_print("Using traditional pio ci build system")
        # ... existing pio ci logic ...
```

### File Structure Integration

**New files to be created:**

```
ci/
├── compiler/
│   ├── pio_run_compiler.py          # New: pio run compilation logic
│   └── project_manager.py           # New: persistent project management
├── util/
│   ├── project_cache.py             # New: project caching utilities
│   └── incremental_build.py         # New: incremental build optimization
└── templates/
    └── platformio_ini.template      # New: platformio.ini template for projects
```

**Modified files:**

```
ci/
├── ci-compile.py                    # Enhanced: Add pio run support
├── util/
│   └── concurrent_run.py            # Enhanced: Support pio run workflow
└── compiler/
    └── compile_for_board.py         # Enhanced: Add pio run compilation functions
```

### Configuration Integration

**Enhanced platformio.ini template:**

```ini
# ci/templates/platformio_ini.template
[env:{board_name}]
platform = {platform}
board = {board_name}
framework = {framework}

# FastLED dependency (symlinked for efficiency)
lib_deps = symlink://{fastled_src_path}

# LDF Configuration (dynamic based on phase)
lib_ldf_mode = {ldf_mode}  # 'chain' for init, 'off' for incremental
lib_compat_mode = soft

# Build optimization
build_cache_dir = {cache_dir}
{build_flags}
{custom_options}
```

### Testing and Validation Integration

**Extend existing test infrastructure:**

```python
# ci/tests/test_pio_run_integration.py - New comprehensive test
class TestPioRunIntegration(unittest.TestCase):
    
    def test_concurrent_pio_run_vs_pio_ci(self):
        """Validate pio run produces identical outputs to pio ci"""
        
    def test_incremental_build_performance(self):
        """Measure performance improvements of incremental builds"""
        
    def test_project_initialization_concurrent(self):
        """Test parallel project initialization"""
        
    def test_cleanup_and_recovery(self):
        """Test cleanup mechanisms and error recovery"""
```

### Migration Strategy

**Gradual rollout plan:**

1. **Phase 1**: Add `--use-pio-run` flag (default: false)
   - Parallel testing with existing system
   - Validate output compatibility

2. **Phase 2**: Enable for CI testing
   - Use pio run in continuous integration
   - Monitor performance and reliability

3. **Phase 3**: Default to pio run
   - Change default to `--use-pio-run=true`
   - Keep pio ci as fallback option

4. **Phase 4**: Full migration
   - Remove pio ci code paths
   - Optimize for pio run only

### Integration Testing Plan

**Compatibility validation:**

```bash
# Test both systems produce identical results
./compile uno --examples Blink --use-pio-ci > results_ci.log
./compile uno --examples Blink --use-pio-run > results_run.log

# Compare binary outputs
diff results_ci.log results_run.log

# Performance comparison
time ./compile uno esp32dev --examples Blink,DemoReel100 --use-pio-ci
time ./compile uno esp32dev --examples Blink,DemoReel100 --use-pio-run
```

**Integration smoke tests:**

```bash
# Test concurrent execution
./compile uno esp32dev teensy31 --examples Blink --use-pio-run

# Test cleanup and recovery
./compile uno --examples Blink --use-pio-run --clean-projects

# Test symbol analysis integration  
./compile uno --examples Blink --use-pio-run --symbols
```

### Backward Compatibility Guarantees

**API Compatibility:**
- All existing CLI arguments continue to work
- `concurrent_run` function signature preserved
- Board configuration format unchanged
- Symbol analysis integration maintained

**Behavioral Compatibility:**
- Build outputs identical between systems
- Error handling and reporting consistent
- Verbose logging format preserved
- Exit codes and return values unchanged

This integration plan provides a systematic approach to introducing the efficient pio run system while maintaining full compatibility with existing FastLED infrastructure and workflows.

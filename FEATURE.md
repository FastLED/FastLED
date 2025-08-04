# FastLED Library Caching System

## Problem Statement

The FastLED example compilation system currently rebuilds the entire FastLED static library (`libfastled.a`) on every test run, even when no source files have changed. This causes:

1. **Slow rebuild times**: All FastLED source files are recompiled as a single unit every time (~3-4 seconds)
2. **Linking cache misses**: Since the library file changes (timestamps/metadata), the linking cache always misses
3. **Poor developer experience**: Repeated `bash test --examples` runs are unnecessarily slow

## Current Behavior

```bash
# First run
[LIBRARY] Compiling FastLED sources (allsrc build)...
[LIBRARY] Creating static library: .build\fastled\libfastled.a
[LINKING] SUCCESS (REBUILT): Blink.exe
[LINKING] Cache: 0 hits, 1 misses (0.0% hit rate)

# Second run (same example)
[LIBRARY] Compiling FastLED sources (allsrc build)...  # ← Should be cached
[LIBRARY] Creating static library: .build\fastled\libfastled.a
[LINKING] SUCCESS (REBUILT): Blink.exe                 # ← Should be cached
[LINKING] Cache: 0 hits, 1 misses (0.0% hit rate)
```

## Desired Behavior

```bash
# First run
[LIBRARY] Compiling FastLED sources (allsrc build)...
[LIBRARY] Creating static library: libfastled-release.a
[LINKING] SUCCESS (REBUILT): Blink.exe
[LINKING] Cache: 0 hits, 1 misses (0.0% hit rate)

# Second run (same example)
[LIBRARY] CACHED: Using existing FastLED library: libfastled-release.a
[LINKING] SUCCESS (CACHED): Blink.exe
[LINKING] Cache: 1 hits, 0 misses (100.0% hit rate)
```

## Proposed Solution: Simple Library Caching

### Core Architecture

The FastLED library caching system will use **stable naming** with **content validation** for the allsrc build system. Since FastLED compiles as a single compilation unit, we only need to cache the final library file.

**Key Insight**: We don't need to track source file dependencies! The library file itself contains all the information we need:
- **Fast path**: Same timestamp = same file (sub-millisecond check)
- **Medium path**: Different timestamp but same hash = source changes didn't affect output (50ms check)
- **Slow path**: Different hash = rebuild needed (3-4s compilation)

#### Cache Naming Strategy

```python
import hashlib
import json
from pathlib import Path
from typing import List
from ci.compiler.clang_compiler import CompilerOptions, BuildFlags

def get_build_configuration_hash(
    build_flags: BuildFlags,
    compiler_options: CompilerOptions
) -> str:
    """Generate comprehensive configuration hash for validation (not filename)."""
    
    # Create comprehensive build signature from ALL configuration elements
    config_elements = []
    
    # All compiler flags matter (not just optimization/debug)
    config_elements.append(f"compiler_flags:{sorted(build_flags.compiler_flags)}")
    
    # All preprocessor defines matter
    config_elements.append(f"defines:{sorted(build_flags.defines)}")
    
    # All linker flags matter
    config_elements.append(f"link_flags:{sorted(build_flags.link_flags)}")
    
    # Compiler path and version matter
    config_elements.append(f"compiler:{compiler_options.compiler}")
    config_elements.append(f"compiler_args:{sorted(compiler_options.compiler_args)}")
    
    # Language standard and other critical settings
    config_elements.append(f"std_version:{compiler_options.std_version}")
    config_elements.append(f"include_path:{compiler_options.include_path}")
    config_elements.append(f"defines_opt:{sorted(compiler_options.defines or [])}")
    
    # Create stable hash from all elements
    combined_config = "|".join(config_elements)
    return hashlib.sha256(combined_config.encode('utf-8')).hexdigest()

def get_build_configuration_name(
    build_flags: BuildFlags,
    compiler_options: CompilerOptions
) -> str:
    """Generate stable cache filename based on semantic build type."""
    
    # Determine build type from compiler flags for stable naming
    is_debug = any(flag in ["-g", "-ggdb", "-g3"] for flag in build_flags.compiler_flags)
    is_optimized = any(flag.startswith("-O") and flag != "-O0" for flag in build_flags.compiler_flags)
    is_test = any("test" in define.lower() or "TEST" in define for define in build_flags.defines)
    is_stub = any("STUB" in define for define in build_flags.defines)
    
    # Generate semantic name for cache file (gets overwritten, no accumulation)
    if is_test and is_debug:
        return "test-debug"
    elif is_test and is_optimized:
        return "test-release"
    elif is_test:
        return "test-default"
    elif is_stub and is_debug:
        return "stub-debug"
    elif is_stub and is_optimized:
        return "stub-release"
    elif is_debug and not is_optimized:
        return "debug"
    elif is_optimized and not is_debug:
        return "release"
    elif is_optimized and is_debug:
        return "release-debug"
    else:
        return "default"

def calculate_file_hash(file_path: Path) -> str:
    """Calculate SHA256 hash of a file for content validation."""
    if not file_path.exists():
        return "no_file"
    
    hash_sha256 = hashlib.sha256()
    try:
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()[:16]  # Short hash for efficiency
    except (OSError, IOError):
        return "error"
```

#### Cache Storage Structure

```
.build/
├── library_cache/
│   ├── libfastled-debug.a          # Debug build library (stable name)
│   ├── libfastled-debug.json       # Debug metadata + comprehensive config_hash
│   ├── libfastled-release.a        # Release build library (stable name)
│   ├── libfastled-release.json     # Release metadata + comprehensive config_hash
│   ├── libfastled-test-debug.a     # Test debug build (stable name)
│   └── libfastled-test-debug.json  # Test debug metadata + comprehensive config_hash
└── link_cache/                     # Existing linking cache
    └── ...
```

#### Simplified Metadata Format

```json
{
    "cache_name": "libfastled-debug",
    "cache_version": "v2",
    "created_at": "2024-01-15T10:30:45Z",
    "library_hash": "f8e7d6c5b4a39281",
    "library_timestamp": 1705315845,
    "config_hash": "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6",
    "library_size": 2048576,
    "build_time": 3.45,
    "fastled_version": "main-commit-abc123"
}
```

### Implementation Details

#### 1. Simple Cache Check Function

```python
import json
import shutil
from pathlib import Path
from typing import Optional
from ci.compiler.clang_compiler import CompilerOptions, BuildFlags

def check_library_cache(
    build_flags: BuildFlags,
    compiler_options: CompilerOptions,
    cache_dir: Path
) -> Optional[Path]:
    """Check if cached library exists and is valid using fast timestamp + hash fallback."""
    
    try:
        # Get stable cache name based on semantic build type
        config_name = get_build_configuration_name(build_flags, compiler_options)
        
        library_file = cache_dir / f"libfastled-{config_name}.a"
        metadata_file = cache_dir / f"libfastled-{config_name}.json"
        
        # Quick existence check
        if not metadata_file.exists() or not library_file.exists():
            return None
        
        # Load metadata
        try:
            with open(metadata_file, 'r', encoding='utf-8') as f:
                metadata = json.load(f)
        except (json.JSONDecodeError, OSError, IOError):
            # Corrupted metadata - remove both files
            metadata_file.unlink(missing_ok=True)
            library_file.unlink(missing_ok=True)
            return None
        
        # Check if build configuration changed (ANY flags, defines, compiler settings)
        cached_config_hash = metadata.get("config_hash", "")
        current_config_hash = get_build_configuration_hash(build_flags, compiler_options)
        
        if cached_config_hash != current_config_hash:
            return None  # Build configuration changed
        
        # Fast path: Check timestamp first
        current_timestamp = int(library_file.stat().st_mtime)
        cached_timestamp = metadata.get("library_timestamp", 0)
        
        if current_timestamp == cached_timestamp:
            return library_file  # Same timestamp = same file (fast!)
        
        # Medium path: Timestamp differs, check hash
        current_hash = calculate_file_hash(library_file)
        cached_hash = metadata.get("library_hash", "")
        
        if current_hash == cached_hash:
            # Same content, update timestamp in metadata for next time
            metadata["library_timestamp"] = current_timestamp
            with open(metadata_file, 'w', encoding='utf-8') as f:
                json.dump(metadata, f, indent=2, sort_keys=True)
        return library_file
        
        # Hash differs = content changed = need rebuild
        return None
        
    except Exception as e:
        print(f"Warning: Cache check failed: {e}")
        return None

def store_library_cache(
    library_file: Path,
    build_flags: BuildFlags,
    compiler_options: CompilerOptions,
    cache_dir: Path,
    build_time: float
) -> bool:
    """Store library in cache with stable naming."""
    
    try:
        config_name = get_build_configuration_name(build_flags, compiler_options)
        cache_dir.mkdir(parents=True, exist_ok=True)
        
        cached_library = cache_dir / f"libfastled-{config_name}.a"
        metadata_file = cache_dir / f"libfastled-{config_name}.json"
        
        # Copy library file
        shutil.copy2(library_file, cached_library)
        
        # Calculate hash and get timestamp of the cached library
        library_hash = calculate_file_hash(cached_library)
        library_timestamp = int(cached_library.stat().st_mtime)
        
        # Calculate comprehensive config hash for validation
        config_hash = get_build_configuration_hash(build_flags, compiler_options)
        
        # Create metadata
        metadata = {
            "cache_name": f"libfastled-{config_name}",
            "cache_version": "v2",
            "created_at": datetime.now().isoformat(),
            "library_hash": library_hash,
            "library_timestamp": library_timestamp,
            "config_hash": config_hash,  # Comprehensive configuration hash for validation
            "library_size": cached_library.stat().st_size,
            "build_time": build_time,
            "fastled_version": get_fastled_version()
        }
        
        # Write metadata
        with open(metadata_file, 'w', encoding='utf-8') as f:
            json.dump(metadata, f, indent=2, sort_keys=True)
            
            return True
            
        except Exception as e:
            print(f"Warning: Failed to store library cache: {e}")
            return False
            
def get_fastled_version() -> str:
    """Get FastLED version/commit for cache validation."""
    try:
        import subprocess
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return f"commit-{result.stdout.strip()}"
    except Exception:
        pass
    return "unknown"
```

#### 2. Integrated Library Creation Function

```python
import time
import shutil
from pathlib import Path
from typing import Callable
from datetime import datetime
from ci.compiler.clang_compiler import Compiler


def create_fastled_library_with_cache(
    compiler: Compiler, 
    fastled_build_dir: Path, 
    log_timing: Callable[[str], None]
) -> Path:
    """Create libfastled.a static library with simple caching."""
    
    # Check cache first
    cache_dir = Path(".build/library_cache")
    cached_library = check_library_cache(
        compiler.build_flags,
        compiler.settings,
        cache_dir
    )
    
    config_name = get_build_configuration_name(compiler.build_flags, compiler.settings)
    lib_file = fastled_build_dir / f"libfastled-{config_name}.a"
    
    if cached_library:
        try:
            # Copy cached library to build directory
            shutil.copy2(cached_library, lib_file)
            log_timing(f"[LIBRARY] CACHED: Using cached FastLED library: {cached_library.name}")
            return lib_file
        except (OSError, IOError) as e:
            log_timing(f"[LIBRARY] Warning: Failed to copy cached library, rebuilding: {e}")
    
    # Cache miss - build library
    build_start = time.time()
    log_timing(f"[LIBRARY] Compiling FastLED sources (allsrc build)...")
    
    # Use existing create_fastled_library function for actual compilation
    lib_file = create_fastled_library_no_cache(compiler, fastled_build_dir, log_timing)
    
    build_time = time.time() - build_start
    
    # Store in cache for future builds
    cache_success = store_library_cache(
        lib_file, compiler.build_flags, 
        compiler.settings, cache_dir, build_time
    )
    
    if cache_success:
        log_timing(f"[LIBRARY] SUCCESS: FastLED library created and cached: {lib_file.name}")
    else:
        log_timing(f"[LIBRARY] SUCCESS: FastLED library created: {lib_file.name}")

    return lib_file
```

### Cache Management

#### Simple Cleanup

```python
import shutil
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict

def cleanup_library_cache(cache_dir: Path = None, max_age_days: int = 7) -> Dict[str, int]:
    """Clean up old library cache entries."""
    if cache_dir is None:
        cache_dir = Path(".build/library_cache")
    
    if not cache_dir.exists():
        return {"removed_entries": 0, "freed_bytes": 0}
    
    removed_count = 0
    freed_bytes = 0
    cutoff_date = datetime.now() - timedelta(days=max_age_days)
    
    # Find all library and metadata files
    for file_path in cache_dir.glob("libfastled-*.a"):
        try:
            # Check file age
            file_mtime = datetime.fromtimestamp(file_path.stat().st_mtime)
            if file_mtime < cutoff_date:
                # Remove library and corresponding metadata
                metadata_file = file_path.with_suffix('.json')
                
                size = file_path.stat().st_size
                file_path.unlink(missing_ok=True)
                metadata_file.unlink(missing_ok=True)
                
                removed_count += 1
                freed_bytes += size
                
            except Exception as e:
            print(f"Warning: Failed to remove cache file {file_path}: {e}")
    
    if removed_count > 0:
        print(f"[CACHE] Cleanup: Removed {removed_count} entries, freed {freed_bytes // 1024 // 1024}MB")
    
    return {"removed_entries": removed_count, "freed_bytes": freed_bytes}

def clear_library_cache(cache_dir: Path = None) -> bool:
    """Clear all library cache entries."""
    if cache_dir is None:
        cache_dir = Path(".build/library_cache")
    
    try:
        if cache_dir.exists():
            shutil.rmtree(cache_dir)
        return True
    except Exception as e:
        print(f"Warning: Failed to clear library cache: {e}")
        return False

def get_library_cache_stats(cache_dir: Path = None) -> Dict[str, int]:
    """Get simple library cache statistics."""
    if cache_dir is None:
        cache_dir = Path(".build/library_cache")
    
    if not cache_dir.exists():
        return {"total_entries": 0, "total_size_mb": 0}
    
    total_entries = 0
    total_size = 0
    
    for library_file in cache_dir.glob("libfastled-*.a"):
                if library_file.exists():
                    total_entries += 1
                    total_size += library_file.stat().st_size
    
    return {
        "total_entries": total_entries,
        "total_size_mb": total_size // 1024 // 1024
    }
```

### 3. Sketch Compilation Cache

```python
def check_sketch_cache(
    sketch_file: Path,
    build_flags: BuildFlags,
    compiler_options: CompilerOptions,
    cache_dir: Path
) -> Optional[Path]:
    """Check if cached sketch object exists and is valid."""
    
    try:
        # Generate cache name based on sketch + semantic build config
        sketch_name = sketch_file.stem
        config_name = get_build_configuration_name(build_flags, compiler_options)
        
        cached_obj = cache_dir / f"{sketch_name}-{config_name}.o"
        metadata_file = cache_dir / f"{sketch_name}-{config_name}.o.json"
        
        # Quick existence check
        if not cached_obj.exists() or not metadata_file.exists():
            return None
        
        # Load metadata
        with open(metadata_file, 'r', encoding='utf-8') as f:
            metadata = json.load(f)
        
        # Check if build configuration changed (comprehensive validation)
        cached_config_hash = metadata.get("config_hash", "")
        current_config_hash = get_build_configuration_hash(build_flags, compiler_options)
        
        if cached_config_hash != current_config_hash:
            return None  # Build configuration changed
        
        # Fast path: Check sketch timestamp
        current_timestamp = int(sketch_file.stat().st_mtime)
        cached_timestamp = metadata.get("sketch_timestamp", 0)
        
        if current_timestamp == cached_timestamp:
            return cached_obj  # Same timestamp = same sketch
        
        # Medium path: Check sketch hash
        current_hash = calculate_file_hash(sketch_file)
        cached_hash = metadata.get("sketch_hash", "")
        
        if current_hash == cached_hash:
            # Update timestamp in metadata
            metadata["sketch_timestamp"] = current_timestamp
            with open(metadata_file, 'w', encoding='utf-8') as f:
                json.dump(metadata, f, indent=2, sort_keys=True)
            return cached_obj
        
        return None  # Sketch changed
        
    except Exception:
        return None

def store_sketch_cache(
    sketch_file: Path,
    obj_file: Path,
    build_flags: BuildFlags,
    compiler_options: CompilerOptions,
    cache_dir: Path,
    build_time: float
) -> None:
    """Store sketch compilation metadata."""
    
    sketch_name = sketch_file.stem
    config_name = get_build_configuration_name(build_flags, compiler_options)
    config_hash = get_build_configuration_hash(build_flags, compiler_options)
    metadata_file = cache_dir / f"{sketch_name}-{config_name}.o.json"
    
    metadata = {
        "sketch_name": sketch_name,
        "sketch_hash": calculate_file_hash(sketch_file),
        "sketch_timestamp": int(sketch_file.stat().st_mtime),
        "obj_file": obj_file.name,
        "config_hash": config_hash,  # Comprehensive configuration hash for validation
        "compiled_at": datetime.now().isoformat(),
        "compile_time": build_time
    }
    
    with open(metadata_file, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=2, sort_keys=True)
```

### 4. Three-Stage Linking Cache

```python
import json
import time
from pathlib import Path
from typing import List
from datetime import datetime

def check_link_cache(
    exe_file: Path,
    sketch_obj: Path,
    library_file: Path,
    link_flags: List[str]
) -> bool:
    """Check if cached executable is valid using sketch + library hashes + link flags."""
    
    metadata_file = exe_file.with_suffix('.exe.json')
    
    # Quick existence check
    if not exe_file.exists() or not metadata_file.exists():
        return False
    
    try:
        # Load link metadata
        with open(metadata_file, 'r', encoding='utf-8') as f:
            metadata = json.load(f)
        
        # Check if sketch object changed
        current_sketch_hash = calculate_file_hash(sketch_obj)
        cached_sketch_hash = metadata.get("sketch_hash", "")
        
        if current_sketch_hash != cached_sketch_hash:
            return False  # Sketch changed, re-link needed
        
        # Check if library changed
        current_lib_hash = calculate_file_hash(library_file)
        cached_lib_hash = metadata.get("library_hash", "")
        
        if current_lib_hash != cached_lib_hash:
            return False  # Library changed, re-link needed
        
        # Check if link flags changed
        cached_link_flags = metadata.get("link_flags", [])
        if sorted(link_flags) != sorted(cached_link_flags):
            return False  # Link flags changed, re-link needed
        
        return True  # Cache hit!
        
    except Exception:
        return False  # Error = re-link

def store_link_cache(
    exe_file: Path,
    sketch_obj: Path,
    library_file: Path,
    link_flags: List[str],
    build_time: float
) -> None:
    """Store linking metadata alongside executable."""
    
    metadata_file = exe_file.with_suffix('.exe.json')
    
    metadata = {
        "exe_name": exe_file.name,
        "sketch_hash": calculate_file_hash(sketch_obj),
        "sketch_name": sketch_obj.name,
        "library_hash": calculate_file_hash(library_file),
        "library_name": library_file.name,
        "link_flags": sorted(link_flags),
        "linked_at": datetime.now().isoformat(),
        "link_time": build_time
    }
    
    with open(metadata_file, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=2, sort_keys=True)
```

#### Three-Stage Cache Structure

```
.build/
├── library_cache/
│   ├── libfastled-debug.a      # Cached FastLED library (stable semantic name)
│   └── libfastled-debug.json   # Library metadata + comprehensive config_hash
├── sketch_cache/
│   ├── Blink-debug.o           # Cached sketch object (stable semantic name)
│   ├── Blink-debug.o.json      # Sketch metadata + comprehensive config_hash
│   ├── DemoReel100-debug.o     # Another sketch object (same config type)
│   └── DemoReel100-debug.o.json # Its metadata + comprehensive config_hash
└── examples/
    ├── Blink.exe               # Final executable
    ├── Blink.exe.json          # Link metadata (both inputs + flags)
    ├── DemoReel100.exe         # Another executable
    └── DemoReel100.exe.json    # Its metadata (both inputs + flags)
```

#### Updated Link Metadata Format

```json
{
    "exe_name": "Blink.exe",
    "sketch_hash": "b2c3d4e5f6g7h8i9",
    "sketch_name": "Blink-debug.o",
    "library_hash": "f8e7d6c5b4a39281",
    "library_name": "libfastled-debug.a",
    "link_flags": ["-L.", "-lfastled", "-static"],
    "linked_at": "2024-01-15T10:30:45Z",
    "link_time": 1.23
}
```

## Performance Benefits

### Expected Improvements

| Scenario | Before | After | Improvement |
|----------|--------|-------|-------------|
| **Library build (timestamp hit)** | 3-4s (always) | 3-4s (first) → 0.01s (cached) | **99.7% faster** |
| **Library build (hash hit)** | 3-4s (always) | 3-4s (first) → 0.05s (cached) | **98% faster** |
| **Sketch compile (timestamp hit)** | 0.5-1s (always) | 0.5-1s (first) → 0.01s (cached) | **99% faster** |
| **Sketch compile (hash hit)** | 0.5-1s (always) | 0.5-1s (first) → 0.02s (cached) | **98% faster** |
| **Linking (cache hit)** | 1-2s (always rebuilds) | 1-2s (first) → 0.01s (cached) | **99.5% faster** |
| **Total rebuild (full cache hit)** | 5-7s | 5-7s (first) → 0.05s (cached) | **99.3% faster** |

### Cache Hit Scenarios

**Library Cache:**
- **Same build configuration, no source changes**: 100% hit rate (timestamp check - fastest!)
- **Same build configuration, source changes but same output**: 100% hit rate (hash check - fast!)
- **Different compiler flags**: Cache miss (new config name)
- **Source changes affecting output**: Cache miss (hash differs)

**Sketch Cache:**
- **Same sketch file, no changes**: 100% hit rate (timestamp check - fastest!)
- **Same sketch content but different timestamp**: 100% hit rate (hash check - fast!)
- **Different build configuration**: Cache miss (new config name)
- **Sketch content changed**: Cache miss (hash differs)

**Linking Cache:**
- **Same sketch hash + same library hash + same link flags**: 100% hit rate (sub-millisecond check)
- **Either rebuilt but identical output**: 100% hit rate (same hashes)
- **Different link flags**: Cache miss (re-link needed)
- **Either input changed**: Cache miss (hash differs)

## Integration Points

### 1. Replace Existing Caching Systems

**Files to modify:**
- `ci/compiler/test_example_compilation.py` - Replace `create_fastled_library()` and linking functions

**Library caching integration:**

```python
# Replace existing create_fastled_library() with:
def create_fastled_library(
    compiler: Compiler, fastled_build_dir: Path, log_timing: Callable[[str], None]
) -> Path:
    """Create libfastled.a static library with simple caching."""
    return create_fastled_library_with_cache(compiler, fastled_build_dir, log_timing)
```

**Complete three-stage build integration:**

```python
import time
from pathlib import Path
from typing import List, Callable

def build_example_with_cache(
    compiler: Compiler,
    sketch_file: Path,
    output_exe: Path,
    link_flags: List[str],
    log_timing: Callable[[str], None]
) -> bool:
    """Complete three-stage build with caching: library + sketch + linking."""
    
    # Stage 1: Build FastLED library (with caching)
    library_cache_dir = Path(".build/library_cache")
    library_file = create_fastled_library_with_cache(
        compiler, library_cache_dir, log_timing
    )
    
    # Stage 2: Compile sketch (with caching)
    sketch_cache_dir = Path(".build/sketch_cache")
    cached_sketch_obj = check_sketch_cache(
        sketch_file, compiler.build_flags, compiler.settings, sketch_cache_dir
    )
    
    config_name = get_build_configuration_name(compiler.build_flags, compiler.settings)
    sketch_obj = sketch_cache_dir / f"{sketch_file.stem}-{config_name}.o"
    
    if cached_sketch_obj:
        log_timing(f"[SKETCH] CACHED: Using cached sketch object: {cached_sketch_obj.name}")
        sketch_obj = cached_sketch_obj
    else:
        # Compile sketch
        build_start = time.time()
        log_timing(f"[SKETCH] Compiling {sketch_file.name}...")
        
        compile_success = compile_sketch(compiler, sketch_file, sketch_obj)
        if not compile_success:
            return False
        
        build_time = time.time() - build_start
        store_sketch_cache(
            sketch_file, sketch_obj, compiler.build_flags, 
            compiler.settings, sketch_cache_dir, build_time
        )
        log_timing(f"[SKETCH] SUCCESS: Sketch compiled and cached: {sketch_obj.name}")
    
    # Stage 3: Link sketch + library (with caching)
    if check_link_cache(output_exe, sketch_obj, library_file, link_flags):
        log_timing(f"[LINKING] CACHED: Using cached executable: {output_exe.name}")
        return True
    
    # Link sketch + library
    build_start = time.time()
    log_timing(f"[LINKING] Linking {output_exe.name}...")
    
    link_success = perform_linking(compiler, sketch_obj, library_file, output_exe, link_flags)
    
    if link_success:
        build_time = time.time() - build_start
        store_link_cache(output_exe, sketch_obj, library_file, link_flags, build_time)
        log_timing(f"[LINKING] SUCCESS: Executable created and cached: {output_exe.name}")
    
    return link_success
```

### 2. Three-Stage Cache Statistics

```python
def report_cache_statistics(log_timing: Callable[[str], None]):
    """Report library, sketch, and linking cache statistics."""
    
    # Library cache stats
    lib_stats = get_library_cache_stats()
    lib_entries = lib_stats.get("total_entries", 0)
    lib_size = lib_stats.get("total_size_mb", 0)
    
    # Sketch cache stats
    sketch_entries = len(list(Path(".build/sketch_cache").glob("*.o")))
    
    # Linking cache stats
    link_entries = len(list(Path(".build/examples").glob("*.exe.json")))
    
    log_timing(f"[CACHE] Library: {lib_entries} entries ({lib_size}MB)")
    log_timing(f"[CACHE] Sketch: {sketch_entries} entries, Linking: {link_entries} entries")
```

## Implementation Plan

### Phase 1: Library Caching
- [ ] Add library cache functions to `test_example_compilation.py`
- [ ] Replace `create_fastled_library()` with cached version
- [ ] Test library caching with simple examples

### Phase 2: Sketch Caching
- [ ] Add sketch compilation cache functions
- [ ] Integrate sketch caching into build pipeline
- [ ] Test sketch caching independently

### Phase 3: Linking Integration
- [ ] Add three-stage linking cache functions
- [ ] Integrate `build_example_with_cache()` workflow
- [ ] Test complete three-stage caching

### Phase 4: Polish & Statistics
- [ ] Add comprehensive cache statistics reporting
- [ ] Implement cache cleanup commands
- [ ] Performance optimization and error handling

## Success Criteria

### **Primary Success Test**

**Command:** `bash test --examples Blink`

**Before Caching (Current Behavior):**
```bash
# First run
$ bash test --examples Blink
[LIBRARY] Compiling FastLED sources (allsrc build)...
[LIBRARY] Creating static library: libfastled-debug.a     # 3-4 seconds
[SKETCH] Compiling Blink.ino...                           # 0.5-1 second  
[LINKING] Linking Blink.exe...                            # 1-2 seconds
[SUCCESS] Blink.exe completed in 5.2 seconds

# Second run (same command)
$ bash test --examples Blink
[LIBRARY] Compiling FastLED sources (allsrc build)...
[LIBRARY] Creating static library: libfastled-debug.a     # 3-4 seconds (AGAIN!)
[SKETCH] Compiling Blink.ino...                           # 0.5-1 second (AGAIN!)
[LINKING] Linking Blink.exe...                            # 1-2 seconds (AGAIN!)
[SUCCESS] Blink.exe completed in 5.1 seconds              # No improvement
```

**After Caching (Target Behavior):**
```bash
# First run
$ bash test --examples Blink
[LIBRARY] Compiling FastLED sources (allsrc build)...
[LIBRARY] Creating static library: libfastled-debug.a     # 3-4 seconds
[SKETCH] Compiling Blink.ino...                           # 0.5-1 second
[LINKING] Linking Blink.exe...                            # 1-2 seconds  
[SUCCESS] Blink.exe completed in 5.2 seconds

# Second run (same command) - BLAZING FAST!
$ bash test --examples Blink  
[LIBRARY] CACHED: Using cached FastLED library: libfastled-debug.a     # 0.01s (stable name!)
[SKETCH] CACHED: Using cached sketch object: Blink-debug.o             # 0.01s (stable name!)
[LINKING] CACHED: Using cached executable: Blink.exe                   # 0.01s (hash validated!)
[SUCCESS] Blink.exe completed in 0.05 seconds                          # 99% faster!
```

### **Additional Test Scenarios**

**Scenario 1: Only sketch changes**
```bash
# Edit Blink.ino, then run again
$ bash test --examples Blink
[LIBRARY] CACHED: Using cached FastLED library: libfastled-debug.a     # 0.01s (stable name!)
[SKETCH] Compiling Blink.ino...                                        # 0.5-1s (rebuild - overwrites Blink-debug.o)
[LINKING] Linking Blink.exe...                                         # 1-2s (rebuild)  
[SUCCESS] Blink.exe completed in 1.8 seconds                           # 65% faster
```

**Scenario 2: Only FastLED source changes**
```bash
# Edit FastLED source, then run again  
$ bash test --examples Blink
[LIBRARY] Compiling FastLED sources (allsrc build)...                  # 3-4s (rebuild - overwrites libfastled-debug.a)
[SKETCH] CACHED: Using cached sketch object: Blink-debug.o             # 0.01s (stable name!)
[LINKING] Linking Blink.exe...                                         # 1-2s (rebuild)
[SUCCESS] Blink.exe completed in 4.2 seconds                           # 20% faster
```

**Scenario 3: Different example, same build config**
```bash
$ bash test --examples DemoReel100
[LIBRARY] CACHED: Using cached FastLED library: libfastled-debug.a     # 0.01s (stable name!)
[SKETCH] Compiling DemoReel100.ino...                                  # 0.5-1s (creates DemoReel100-debug.o)
[LINKING] Linking DemoReel100.exe...                                   # 1-2s (new exe)
[SUCCESS] DemoReel100.exe completed in 1.8 seconds                     # 65% faster
```

### **Success Metrics**

- **Library cache hit rate**: Target 95%+ for repeated builds (timestamp + hash validation)
- **Sketch cache hit rate**: Target 98%+ for repeated builds (timestamp + hash validation)  
- **Linking cache hit rate**: Target 99%+ for repeated builds (hash + flags validation)
- **Build time reduction**: 99%+ for fully cached builds (5+ seconds → 50ms)
- **Developer satisfaction**: Second run completes in under 100ms

### **Definition of Success**

**✅ SUCCESS:** When a developer runs `bash test --examples Blink` twice in a row:
- **First run**: Normal compile time (~5 seconds)
- **Second run**: Blazing fast (<100ms) with clear "CACHED" messages
- **Output clearly shows**: Which stages were cached vs rebuilt
- **No functionality lost**: Executable works identically to non-cached version

**✅ BONUS SUCCESS:** Intelligent partial caching works correctly:
- Edit sketch → Only sketch recompiles, library stays cached
- Edit FastLED → Only library recompiles, sketch stays cached  
- Different example → Library cached, only new sketch compiles

## Summary

This caching approach fixes the critical flaw in typical partial flag checking and provides bulletproof cache validation:

### **🧹 SMART CACHE MANAGEMENT: Semantic Names + Comprehensive Validation**

**❌ NAIVE APPROACH (Hash-based filenames):**
```
libfastled-a1b2c3d4e5f6.a    # Every flag change = new file
libfastled-x7y8z9a0b1c2.a    # Accumulates forever 
libfastled-m3n4o5p6q7r8.a    # Needs garbage collection
libfastled-b4r5t6y7u8i9.a    # Cache directory pollution
```

**✅ SMART APPROACH (Semantic names + metadata validation):**
```
libfastled-debug.a           # Stable filename, gets overwritten
libfastled-debug.json        # Contains comprehensive config_hash for validation  
libfastled-release.a         # Different semantic type
libfastled-release.json      # Contains different config_hash
```

**Benefits:**
- ✅ **Natural cache replacement**: Same build type overwrites previous cache
- ✅ **No accumulation**: Debug builds replace old debug builds automatically
- ✅ **Clean cache directory**: Only a few semantic cache files exist
- ✅ **No garbage collection needed**: Self-managing cache
- ✅ **Comprehensive validation**: Full config_hash in metadata ensures correctness

### **🔥 CRITICAL FIX: Comprehensive Flag Validation**

**❌ BROKEN (Typical Approach):**
```python
# Only checks optimization/debug flags - DANGEROUS!
key_flags = [f for f in flags if f.startswith(("-O", "-g"))]
```

**✅ FIXED (Our Approach):**
```python
# Checks ALL configuration elements - SAFE!
config_elements = []

# ALL compiler flags (not just optimization flags!)
config_elements.append(f"compiler_flags:{sorted(build_flags.compiler_flags)}")

# ALL preprocessor defines  
config_elements.append(f"defines:{sorted(build_flags.defines)}")

# ALL linker flags
config_elements.append(f"link_flags:{sorted(build_flags.link_flags)}")

# ALL compiler settings  
config_elements.append(f"compiler:{compiler_options.compiler}")
config_elements.append(f"compiler_args:{sorted(compiler_options.compiler_args)}")
config_elements.append(f"std_version:{compiler_options.std_version}")
config_elements.append(f"include_path:{compiler_options.include_path}")

# Create comprehensive hash from ALL configuration
config_hash = sha256("|".join(config_elements).encode('utf-8')).hexdigest()[:12]
```

**Why This Matters:**
- **Include paths**: `-I/some/path` vs `-I/other/path` → Different headers, different output
- **Language standard**: `-std=c++17` vs `-std=c++14` → Different language features  
- **Architecture**: `-march=native` vs `-march=x86-64` → Different instruction sets
- **Warning levels**: `-Wall` vs `-Wall -Wextra` → Different compile-time checks
- **Feature defines**: `-DFASTLED_FEATURE=1` → Different code paths
- **Platform flags**: `-DSTUB_PLATFORM` vs `-DAVR_PLATFORM` → Different platform code
- **Debug symbols**: `-g` vs `-g3` → Different debug information levels
- **Optimization**: `-O2` vs `-O3` → Different code generation
- **Sanitizers**: `-fsanitize=address` → Different runtime checking

**Traditional "key_flags" approach would miss ALL of these and serve stale binaries!**

### **🎯 COMPREHENSIVE FLAG TRACKING PRINCIPLE**

**CRITICAL RULE: ANY flag change = cache invalidation**

```python
# ✅ CORRECT: Track every single flag that affects compilation
all_flags_that_matter = (
    build_flags.compiler_flags +     # Every -O, -g, -I, -D, -std, -march, etc.
    build_flags.defines +            # Every -DDEFINE=value
    build_flags.link_flags +         # Every linker flag  
    compiler_options.compiler_args + # Every additional compiler argument
    [compiler_options.compiler] +    # Compiler path/version
    [compiler_options.std_version] + # Language standard
    [compiler_options.include_path]  # Include directory
)

# NEVER do partial tracking:
# ❌ WRONG: key_flags = [f for f in flags if f.startswith("-O")]
# ❌ WRONG: key_flags = [f for f in flags if f in ["-g", "-O0"]]  
# ❌ WRONG: Any subset filtering whatsoever!
```

### **Cache Validation Strategy**

This caching approach is much simpler than traditional build systems because:

### **No Dependency Tracking Needed**
- **Traditional**: Track thousands of header dependencies, complex makefiles
- **FastLED**: Just validate the final output files (library + executable)

### **Two-Tier Validation**
- **Fast path**: Timestamp comparison (sub-millisecond)
- **Medium path**: Hash comparison (50ms)
- **Slow path**: Full rebuild (3-4s)

### **Three-Stage Cache Architecture**
```
.build/
├── library_cache/
│   ├── libfastled-debug.a      # Cached FastLED library (stable semantic name)
│   └── libfastled-debug.json   # Library metadata + comprehensive config_hash
├── sketch_cache/
│   ├── Blink-debug.o           # Cached sketch objects (stable semantic name)
│   └── Blink-debug.o.json      # Sketch metadata + comprehensive config_hash
└── examples/
    ├── Blink.exe               # Final executables
    └── Blink.exe.json          # Link metadata (both inputs + all flags)
```

### **Three Independent Cache Stages**
1. **Library Cache**: FastLED source → `libfastled-{semantic_name}.a` (3-4s → 0.01s)
2. **Sketch Cache**: Sketch file → `{sketch}-{semantic_name}.o` (0.5-1s → 0.01s)  
3. **Link Cache**: Both objects → `{sketch}.exe` (1-2s → 0.01s)

**Key Insight**: Stable semantic filenames (debug, release, test-debug) get **overwritten** naturally, preventing cache accumulation while comprehensive `config_hash` validation in metadata ensures correctness.

### **99%+ Performance Improvement**
From 5-7 seconds down to 50ms for fully cached builds - making development iteration nearly instant.

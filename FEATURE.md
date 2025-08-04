# Fingerprint Cache Feature

## Overview

A two-layer file change detection system that efficiently determines if source files have been modified by combining fast modification time checks with slower but accurate MD5 hash verification.

## API

```python
def has_changed(src_path: Path, previous_modtime: float, cache_file: Path) -> bool | FileNotFoundError:
    """
    Determine if a source file has changed since the last known modification time.
    
    Args:
        src_path: Path to the source file to check
        previous_modtime: Previously known modification time (Unix timestamp)
        cache_file: Path to the fingerprint cache file
        
    Returns:
        True if the file has changed, False if unchanged
        Exception if not found.
    """
```

## Cache Structure

The cache is stored as a JSON file with the following format:

```json
{
    "/path/to/file1.cpp": {"modification_time": 1699123456.789, "md5_hash": "d41d8cd98f00b204e9800998ecf8427e"},
    "/path/to/file2.h": {"modification_time": 1699123460.123, "md5_hash": "098f6bcd4621d373cade4e832627b4f6"},
    "/path/to/dir/file3.py": {"modification_time": 1699123465.456, "md5_hash": "5d41402abc4b2a76b9719d911017c592"}
}
```

Each entry contains:
- **Key**: Absolute file path (string)
- **Value**: Cache entry object with:
  - `modification_time`: Modification time (float, Unix timestamp with fractional seconds)
  - `md5_hash`: MD5 hash (string, hexadecimal)

## Algorithm

### Two-Layer Verification Process

1. **Layer 1 - Modification Time Check (Fast)**
   - Retrieve current file modification time using `os.path.getmtime()`
   - Compare with `previous_modtime` parameter
   - If times match exactly → return False (no change detected)
   - If times differ → proceed to Layer 2

2. **Layer 2 - Content Verification (Accurate)**
   - Compute MD5 hash of current file content
   - If file exists in cache and cached modtime matches current modtime → use cached hash
   - Otherwise: update cache with new `CacheEntry(modification_time, md5_hash)`
   - Compare current hash with previous hash from cache to verify actual content change
   - **Critical**: If hashes match despite modtime difference → return False (no content change)

### Flow Diagram

```
src_path exists? ─NO─> FileNotFoundError
     │ YES
     ▼
Current modtime == previous_modtime? ─YES─> Return False (no change)
     │ NO
     ▼
File in cache? ─NO─> Compute MD5, Create CacheEntry, Assume changed (Return True)
     │ YES
     ▼
Current modtime == cached_entry.modification_time? ─YES─> Use cached hash
     │ NO                                                           │
     ▼                                                              ▼
Compute MD5, Update CacheEntry ─────────────────────────────> Compare current hash with previous cached hash
                                                                     │
                                                                     ▼
                                                           Hashes equal? ─YES─> Return False (no content change)
                                                                     │ NO
                                                                     ▼
                                                           Return True (content changed)
```

## Implementation Details

### Cache Management

```python
import os
import json
from pathlib import Path
from dataclasses import dataclass

@dataclass
class CacheEntry:
    modification_time: float
    md5_hash: str

class FingerprintCache:
    def __init__(self, cache_file: Path):
        self.cache_file = cache_file
        self.cache = self._load_cache()
    
    def _load_cache(self) -> dict[str, CacheEntry]:
        """Load cache from JSON file, return empty dict if file doesn't exist."""
        if not self.cache_file.exists():
            return {}
            
        try:
            with open(self.cache_file, 'r') as f:
                data = json.load(f)
            
            # Convert JSON dict to CacheEntry objects
            cache = {}
            for file_path, entry_data in data.items():
                cache[file_path] = CacheEntry(
                    modification_time=entry_data["modification_time"],
                    md5_hash=entry_data["md5_hash"]
                )
            return cache
        except (json.JSONDecodeError, KeyError, TypeError):
            # Cache corrupted - start fresh
            return {}
        
    def _save_cache(self) -> None:
        """Save current cache state to JSON file."""
        # Convert CacheEntry objects to JSON-serializable dict
        data = {}
        for file_path, entry in self.cache.items():
            data[file_path] = {
                "modification_time": entry.modification_time,
                "md5_hash": entry.md5_hash
            }
            
        with open(self.cache_file, 'w') as f:
            json.dump(data, f, indent=2)
        
    def _compute_md5(self, file_path: Path) -> str:
        """Compute MD5 hash of file content."""
        
    def has_changed(self, src_path: Path, previous_modtime: float) -> bool:
        """Main API function implementing the two-layer check."""
        if not src_path.exists():
            raise FileNotFoundError(f"Source file not found: {src_path}")
            
        current_modtime = os.path.getmtime(src_path)
        
        # Layer 1: Quick modification time check
        if current_modtime == previous_modtime:
            return False  # No change detected
        
        file_key = str(src_path)
        
        # Layer 2: Content verification via hash comparison
        if file_key in self.cache:
            cached_entry = self.cache[file_key]
            previous_hash = cached_entry.md5_hash  # Store previous hash before potential update
            
            if current_modtime == cached_entry.modification_time:
                # File is cached with current modtime - use cached hash
                current_hash = cached_entry.md5_hash
            else:
                # Cache is stale - compute new hash
                current_hash = self._compute_md5(src_path)
                self._update_cache_entry(src_path, current_modtime, current_hash)
            
            # Compare current hash with previous cached hash
            # If they match, content hasn't actually changed despite modtime difference
            return current_hash != previous_hash
            
        else:
            # File not in cache - compute hash and cache it
            current_hash = self._compute_md5(src_path)
            self._update_cache_entry(src_path, current_modtime, current_hash)
            return True  # Assume changed since we have no previous state to compare
        
    def _update_cache_entry(self, file_path: Path, modification_time: float, md5_hash: str) -> None:
        """Update cache with new entry."""
        self.cache[str(file_path)] = CacheEntry(
            modification_time=modification_time,
            md5_hash=md5_hash
        )
```

### Performance Characteristics

- **Cache Hit (modtime unchanged)**: ~0.1ms per file (JSON lookup + timestamp comparison)
- **Cache Miss (modtime changed)**: ~1-10ms per file (MD5 computation + cache update)
- **False Positive Avoidance**: When modtime differs but content is identical, hash comparison correctly returns False
- **Memory Usage**: ~100 bytes per cached file entry
- **Disk Usage**: JSON cache file scales linearly with number of tracked files

### Edge Case Handling

**Scenario**: File modification time changes but content remains identical (e.g., file touched, copied with different timestamp)

```python
# Initial state: file.txt has modtime=1699123456.789, hash="abc123"
cache.has_changed(Path("file.txt"), 1699123456.789)  # Returns False (exact match)

# File gets touched but content unchanged: modtime=1699123999.000, hash="abc123" 
cache.has_changed(Path("file.txt"), 1699123456.789)  # Returns False (hash comparison detects no content change)

# File content actually changes: modtime=1699124000.000, hash="xyz789"
cache.has_changed(Path("file.txt"), 1699123456.789)  # Returns True (both modtime and hash differ)
```

### Error Handling

```python
# File doesn't exist
if not src_path.exists():
    raise FileNotFoundError(f"Source file not found: {src_path}")

# File read permission issues  
try:
    with open(src_path, 'rb') as f:
        content = f.read()
except IOError as e:
    raise IOError(f"Cannot read file {src_path}: {e}")

# Cache corruption (malformed JSON)
try:
    cache = json.loads(cache_content)
except json.JSONDecodeError:
    # Rebuild cache from scratch
    cache = {}
```

## Usage Examples

### Basic Usage

```python
from pathlib import Path
import os

cache_file = Path(".fastled_cache.json")
cache = FingerprintCache(cache_file)

# Check if source file changed
src_file = Path("src/FastLED.cpp")
last_known_modtime = 1699123456.789  # From previous build state

if cache.has_changed(src_file, last_known_modtime):
    print("File has changed - recompilation needed")
    # Perform expensive operation (compilation, etc.)
    new_modtime = os.path.getmtime(src_file)  # Store for next check
else:
    print("File unchanged - using cached result")
```

### Batch File Checking

```python
import os

cache_file = Path(".fastled_cache.json")
cache = FingerprintCache(cache_file)

source_files = [
    Path("src/FastLED.cpp"),
    Path("src/hsv2rgb.cpp"), 
    Path("src/power_mgt.cpp")
]

last_modtimes = {
    "src/FastLED.cpp": 1699123456.789,
    "src/hsv2rgb.cpp": 1699123460.123,
    "src/power_mgt.cpp": 1699123465.456
}

changed_files = []
for src_file in source_files:
    if cache.has_changed(src_file, last_modtimes[str(src_file)]):
        changed_files.append(src_file)

if changed_files:
    print(f"Changed files: {changed_files}")
    # Update modtimes for next check
    for src_file in changed_files:
        last_modtimes[str(src_file)] = os.path.getmtime(src_file)
    # Rebuild only what's necessary
```

### Integration with Build Systems

```python
import os

def should_rebuild(target: Path, sources: list[Path], cache_file: Path) -> bool:
    """Determine if target needs rebuilding based on source changes."""
    
    cache = FingerprintCache(cache_file)
    
    if not target.exists():
        return True  # Target doesn't exist, must build
    
    # Get target's modification time as baseline
    target_modtime = os.path.getmtime(target)
    
    # Check if any source is newer than target
    for src in sources:
        if cache.has_changed(src, target_modtime):
            return True
            
    return False  # All sources unchanged
```

## Benefits

### Performance Improvements

1. **Fast Path Optimization**: Modification time checks avoid expensive MD5 computation in ~95% of cases
2. **Incremental Builds**: Only reprocess files that actually changed
3. **Batch Processing**: Efficiently check hundreds of files in milliseconds

### Reliability

1. **Content-Based Detection**: MD5 hashes detect actual content changes, not just timestamp updates
2. **False Positive Avoidance**: Returns False when modification time changes but content is identical
3. **Robust to Clock Changes**: Handles system time adjustments and file copying
4. **Cross-Platform**: Works consistently across Windows, macOS, and Linux

## Integration Points

### FastLED Build System

```python
# In ci/ci-compile.py
cache_file = Path(".build/fingerprint_cache.json")
cache = FingerprintCache(cache_file)

def compile_if_changed(example_path: Path, platform: str, cache_file: Path, last_compile_hash: str):
    cache = FingerprintCache(cache_file)
    source_files = list(example_path.glob("*.cpp")) + list(example_path.glob("*.h"))
    
    # Check if recompilation needed
    rebuild_needed = False
    for src_file in source_files:
        if cache.has_changed(src_file, last_compile_hash):
            rebuild_needed = True
            break
    
    if rebuild_needed:
        # Perform compilation
        compile_example(example_path, platform)
    else:
        print(f"✓ {example_path.name} unchanged, skipping compilation")
```

### Testing Framework

```python
# In test.py
def run_tests_if_changed(cache_file: Path, previous_run_hash: str):
    cache = FingerprintCache(cache_file)
    test_files = list(Path("tests/").glob("test_*.cpp"))
    
    for test_file in test_files:
        if cache.has_changed(test_file, previous_run_hash):
            run_single_test(test_file)
        else:
            print(f"✓ {test_file.name} unchanged, using cached results")
```

## Future Enhancements

1. **Directory Watching**: Integrate with filesystem watchers for real-time change detection
2. **Content Filtering**: Ignore whitespace-only changes or comment updates
3. **Parallel Hashing**: Multi-threaded MD5 computation for large file sets
4. **Compression**: Store cache using binary format for reduced disk usage
5. **Network Caching**: Distributed cache for team development environments

## Configuration

```python
# Configuration for advanced use cases - ALL VALUES REQUIRED
@dataclass
class FingerprintCacheConfig:
    cache_file: Path
    hash_algorithm: str  # md5, sha1, sha256
    ignore_patterns: list[str]
    max_cache_size: int  # Maximum number of cached entries
    cache_ttl: int  # Cache entry TTL in seconds

# Usage - all parameters must be explicitly provided
config = FingerprintCacheConfig(
    cache_file=Path(".fastled_cache.json"),
    hash_algorithm="md5",
    ignore_patterns=["*.tmp", "*.log"],
    max_cache_size=10000,
    cache_ttl=86400
)
cache = FingerprintCache(config.cache_file)
```

This fingerprint cache provides an efficient foundation for build optimization and change detection throughout the FastLED project.

## Integration Plan

### Phase 1: Core Implementation (Week 1-2)

**Goal**: Create foundational fingerprint cache module with comprehensive testing.

#### 1.1 Create Core Module
- **File**: `ci/ci/fingerprint_cache.py`
- **Classes**: `CacheEntry`, `FingerprintCache`, `FingerprintCacheConfig`
- **Dependencies**: `pathlib`, `json`, `hashlib`, `os`, `typing`

```python
# ci/ci/fingerprint_cache.py
from pathlib import Path
from typing import Optional
import json
import hashlib
import os
from dataclasses import dataclass

@dataclass
class CacheEntry:
    modification_time: float
    md5_hash: str

@dataclass
class FingerprintCacheConfig:
    cache_file: Path
    hash_algorithm: str
    ignore_patterns: list[str]
    max_cache_size: int

class FingerprintCache:
    def __init__(self, config: FingerprintCacheConfig):
        self.config = config
        self.cache_file = config.cache_file
        self.cache = self._load_cache()
        
    def _load_cache(self) -> dict[str, CacheEntry]:
        # Implementation here
        pass
        
    def has_changed(self, src_path: Path, previous_modtime: float) -> bool:
        # Implementation here
        pass
```

## Testing

### Test Environment Setup

#### Directory Structure
```
test_fingerprint_cache/
├── cache/
│   └── test_cache.json         # Test cache file
├── fixtures/
│   ├── sample.cpp              # Test source files
│   ├── header.h                # Various file types
│   ├── config.json             # Different content types
│   └── binary.o                # Binary files
├── temp/
│   └── (temporary test files)  # Created during tests
└── scripts/
    └── test_runner.py          # Test execution script
```

#### Test File Creation

```python
import os
import time
import tempfile
from pathlib import Path

def setup_test_environment():
    """Create test directory structure and sample files."""
    
    # Create test directory
    test_dir = Path("test_fingerprint_cache")
    test_dir.mkdir(exist_ok=True)
    
    # Create subdirectories
    (test_dir / "cache").mkdir(exist_ok=True)
    (test_dir / "fixtures").mkdir(exist_ok=True)
    (test_dir / "temp").mkdir(exist_ok=True)
    (test_dir / "scripts").mkdir(exist_ok=True)
    
    # Create sample test files with known content
    test_files = {
        "fixtures/sample.cpp": """
#include <iostream>
int main() {
    std::cout << "Hello World" << std::endl;
    return 0;
}
""",
        "fixtures/header.h": """
#pragma once
#ifndef HEADER_H
#define HEADER_H
void function_declaration();
#endif
""",
        "fixtures/config.json": """
{
    "version": "1.0",
    "settings": {
        "debug": true,
        "optimization": 2
    }
}
""",
        "fixtures/empty.txt": "",
        "fixtures/large.txt": "x" * 10000  # Large file for performance testing
    }
    
    # Write test files
    for file_path, content in test_files.items():
        full_path = test_dir / file_path
        with open(full_path, 'w') as f:
            f.write(content)
    
    return test_dir

def create_test_file(path: Path, content: str, modtime: float = None) -> Path:
    """Create a test file with specific content and optional modification time."""
    with open(path, 'w') as f:
        f.write(content)
    
    if modtime:
        # Set specific modification time
        os.utime(path, (modtime, modtime))
    
    return path

def touch_file(path: Path) -> float:
    """Touch a file to update its modification time without changing content."""
    # Wait a small amount to ensure modtime changes
    time.sleep(0.01)
    path.touch()
    return os.path.getmtime(path)

def modify_file_content(path: Path, new_content: str) -> float:
    """Modify file content and return new modification time."""
    time.sleep(0.01)  # Ensure modtime changes
    with open(path, 'w') as f:
        f.write(new_content)
    return os.path.getmtime(path)
```

### Core Test Scenarios

#### 1. Cache Hit Tests (Fast Path)
```python
def test_cache_hit_modtime_unchanged():
    """Test that identical modtime returns False immediately."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    # Create test file
    test_file = test_dir / "temp" / "test.txt"
    create_test_file(test_file, "original content")
    original_modtime = os.path.getmtime(test_file)
    
    # First call - file not in cache, should return True
    result1 = cache.has_changed(test_file, original_modtime)
    assert result1 == False, "Same modtime should return False"
    
    # Verify no MD5 computation occurred (check performance)
    start_time = time.time()
    result2 = cache.has_changed(test_file, original_modtime)
    elapsed = time.time() - start_time
    
    assert result2 == False, "Same modtime should return False"
    assert elapsed < 0.001, f"Cache hit should be <1ms, took {elapsed*1000:.2f}ms"

def test_cache_hit_with_existing_cache():
    """Test cache hit when file exists in cache with current modtime."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    # Create and cache a file
    test_file = test_dir / "temp" / "cached.txt"
    create_test_file(test_file, "cached content")
    current_modtime = os.path.getmtime(test_file)
    
    # Prime the cache
    cache.has_changed(test_file, current_modtime - 1)  # Different modtime to force caching
    
    # Test cache hit
    result = cache.has_changed(test_file, current_modtime)
    assert result == False, "File with current modtime in cache should return False"
```

#### 2. False Positive Avoidance Tests
```python
def test_modtime_changed_content_same():
    """Critical test: modtime changes but content identical should return False."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    # Create test file
    test_file = test_dir / "temp" / "touched.txt"
    original_content = "unchanged content"
    create_test_file(test_file, original_content)
    original_modtime = os.path.getmtime(test_file)
    
    # Prime cache with original state
    cache.has_changed(test_file, original_modtime - 1)  # Force caching
    
    # Touch file (change modtime but not content)
    new_modtime = touch_file(test_file)
    assert new_modtime != original_modtime, "Touch should change modtime"
    
    # Verify content is still the same
    with open(test_file, 'r') as f:
        current_content = f.read()
    assert current_content == original_content, "Content should be unchanged"
    
    # Test the critical behavior: should return False despite modtime change
    result = cache.has_changed(test_file, original_modtime)
    assert result == False, "File with changed modtime but same content should return False"

def test_file_copy_different_timestamp():
    """Test copied files with different timestamps but identical content."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    # Create original file
    original_file = test_dir / "temp" / "original.txt"
    content = "file content for copying"
    create_test_file(original_file, content)
    original_modtime = os.path.getmtime(original_file)
    
    # Prime cache
    cache.has_changed(original_file, original_modtime - 1)
    
    # Simulate file copy with different timestamp
    import shutil
    copied_file = test_dir / "temp" / "copied.txt"
    shutil.copy2(original_file, copied_file)
    
    # Modify the copied file's timestamp
    time.sleep(0.01)
    new_modtime = time.time()
    os.utime(copied_file, (new_modtime, new_modtime))
    
    # Test: different modtime, same content should return False
    result = cache.has_changed(copied_file, original_modtime)
    assert result == False, "Copied file with different timestamp but same content should return False"
```

#### 3. Content Change Detection Tests
```python
def test_content_actually_changed():
    """Test that actual content changes are detected."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    # Create test file
    test_file = test_dir / "temp" / "modified.txt"
    original_content = "original content"
    create_test_file(test_file, original_content)
    original_modtime = os.path.getmtime(test_file)
    
    # Prime cache
    cache.has_changed(test_file, original_modtime - 1)
    
    # Actually modify content
    new_content = "modified content"
    new_modtime = modify_file_content(test_file, new_content)
    
    # Should detect change
    result = cache.has_changed(test_file, original_modtime)
    assert result == True, "File with changed content should return True"

def test_incremental_changes():
    """Test multiple incremental changes to same file."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "test_cache.json"
    cache = FingerprintCache(cache_file)
    
    test_file = test_dir / "temp" / "incremental.txt"
    
    # Version 1
    content_v1 = "version 1"
    create_test_file(test_file, content_v1)
    modtime_v1 = os.path.getmtime(test_file)
    
    # Version 2  
    content_v2 = "version 2"
    modtime_v2 = modify_file_content(test_file, content_v2)
    
    # Version 3
    content_v3 = "version 3"
    modtime_v3 = modify_file_content(test_file, content_v3)
    
    # Test progression
    assert cache.has_changed(test_file, modtime_v1) == True, "v1->v3 should detect change"
    assert cache.has_changed(test_file, modtime_v2) == True, "v2->v3 should detect change"
    assert cache.has_changed(test_file, modtime_v3) == False, "v3->v3 should be unchanged"
```

#### 4. Cache Management Tests
```python
def test_cache_persistence():
    """Test that cache persists across FingerprintCache instances."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "persistent.json"
    
    # First cache instance
    cache1 = FingerprintCache(cache_file)
    test_file = test_dir / "temp" / "persistent.txt"
    create_test_file(test_file, "persistent content")
    modtime = os.path.getmtime(test_file)
    
    # Prime cache
    cache1.has_changed(test_file, modtime - 1)
    
    # Second cache instance (reload from disk)
    cache2 = FingerprintCache(cache_file)
    
    # Should use cached data
    result = cache2.has_changed(test_file, modtime)
    assert result == False, "New cache instance should load existing cache"

def test_cache_corruption_recovery():
    """Test graceful handling of corrupted cache files."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "corrupted.json"
    
    # Create corrupted cache file
    with open(cache_file, 'w') as f:
        f.write("{ invalid json content")
    
    # Should handle corruption gracefully
    cache = FingerprintCache(cache_file)
    test_file = test_dir / "temp" / "recovery.txt"
    create_test_file(test_file, "recovery content")
    modtime = os.path.getmtime(test_file)
    
    # Should work despite corrupted cache
    result = cache.has_changed(test_file, modtime - 1)
    assert result == True, "Should work with corrupted cache"
```

### Performance Testing

#### 5. Performance Benchmarks
```python
def test_performance_cache_hit():
    """Benchmark cache hit performance."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "perf.json"
    cache = FingerprintCache(cache_file)
    
    # Create test files
    test_files = []
    for i in range(100):
        test_file = test_dir / "temp" / f"perf_{i}.txt"
        create_test_file(test_file, f"content {i}")
        test_files.append(test_file)
    
    # Measure cache hit performance
    start_time = time.time()
    for test_file in test_files:
        modtime = os.path.getmtime(test_file)
        cache.has_changed(test_file, modtime)  # Cache hit
    elapsed = time.time() - start_time
    
    avg_time = elapsed / len(test_files) * 1000  # ms per file
    assert avg_time < 0.5, f"Cache hit average {avg_time:.2f}ms should be <0.5ms"

def test_performance_large_files():
    """Test performance with large files."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "large.json"
    cache = FingerprintCache(cache_file)
    
    # Create large test file (1MB)
    large_file = test_dir / "temp" / "large.txt"
    large_content = "x" * (1024 * 1024)
    create_test_file(large_file, large_content)
    modtime = os.path.getmtime(large_file)
    
    # Measure MD5 computation time
    start_time = time.time()
    result = cache.has_changed(large_file, modtime - 1)  # Force MD5 computation
    elapsed = time.time() - start_time
    
    assert result == True, "Large file should be detected as changed"
    assert elapsed < 0.1, f"Large file processing {elapsed*1000:.2f}ms should be <100ms"
```

### Integration Testing

#### 6. Build System Integration Tests
```python
def test_build_system_workflow():
    """Test complete build system workflow."""
    test_dir = setup_test_environment()
    cache_file = test_dir / "cache" / "build.json"
    cache = FingerprintCache(cache_file)
    
    # Simulate source files
    source_files = [
        test_dir / "temp" / "main.cpp",
        test_dir / "temp" / "utils.cpp", 
        test_dir / "temp" / "config.h"
    ]
    
    # Create initial files
    for i, src_file in enumerate(source_files):
        create_test_file(src_file, f"source content {i}")
    
    # First build - all files should be "changed" (new)
    changed_files = []
    baseline_time = time.time() - 3600  # 1 hour ago
    
    for src_file in source_files:
        if cache.has_changed(src_file, baseline_time):
            changed_files.append(src_file)
    
    assert len(changed_files) == 3, "All new files should be detected as changed"
    
    # Second build - no changes
    current_modtimes = [os.path.getmtime(f) for f in source_files]
    changed_files = []
    
    for src_file, modtime in zip(source_files, current_modtimes):
        if cache.has_changed(src_file, modtime):
            changed_files.append(src_file)
    
    assert len(changed_files) == 0, "Unchanged files should not be detected as changed"
    
    # Third build - modify one file
    modify_file_content(source_files[0], "modified main.cpp")
    changed_files = []
    
    for src_file, modtime in zip(source_files, current_modtimes):
        if cache.has_changed(src_file, modtime):
            changed_files.append(src_file)
    
    assert len(changed_files) == 1, "Only modified file should be detected"
    assert changed_files[0] == source_files[0], "Should detect the correct modified file"

def run_all_tests():
    """Execute all test scenarios."""
    print("Setting up test environment...")
    test_dir = setup_test_environment()
    
    print("Running core functionality tests...")
    test_cache_hit_modtime_unchanged()
    test_cache_hit_with_existing_cache()
    
    print("Running false positive avoidance tests...")
    test_modtime_changed_content_same()
    test_file_copy_different_timestamp()
    
    print("Running content change detection tests...")
    test_content_actually_changed()
    test_incremental_changes()
    
    print("Running cache management tests...")
    test_cache_persistence()
    test_cache_corruption_recovery()
    
    print("Running performance tests...")
    test_performance_cache_hit()
    test_performance_large_files()
    
    print("Running integration tests...")
    test_build_system_workflow()
    
    print("✅ All tests passed!")

if __name__ == "__main__":
    run_all_tests()
```

### Test Execution

#### Running the Test Suite
```bash
# Setup test environment
cd test_fingerprint_cache/scripts/
python test_runner.py

# Run specific test categories
python test_runner.py --category performance
python test_runner.py --category integration  
python test_runner.py --category edge-cases

# Verbose output for debugging
python test_runner.py --verbose

# Clean up test files
python test_runner.py --cleanup
```

#### Continuous Integration Tests
```yaml
# .github/workflows/fingerprint-cache-test.yml
name: Fingerprint Cache Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Run Fingerprint Cache Tests
        run: |
          cd test_fingerprint_cache/scripts/
          python test_runner.py --ci
      - name: Performance Benchmarks
        run: |
          python test_runner.py --category performance --benchmark
```

### Expected Test Results

**Performance Targets**:
- Cache hit: <0.5ms per file
- Cache miss: <10ms per file (including MD5)
- Large file (1MB): <100ms
- 100 files batch: <50ms total

**Reliability Targets**:
- Zero false positives for touched files
- Zero false negatives for content changes  
- Graceful handling of cache corruption
- Cross-platform consistency

**Coverage Goals**:
- Line coverage: >95%
- Branch coverage: >90%  
- Edge case coverage: 100%

### Phase 2: Build System Integration (Week 3-4)

**Goal**: Integrate cache into existing compilation workflows with backward compatibility.

#### 2.1 Compilation System Integration
- **File**: `ci/ci/clang_compiler.py` - Add cache-aware compilation
- **File**: `ci/ci-compile.py` - Main entry point integration

```python
# ci/ci/clang_compiler.py modifications
class Compiler:
    def __init__(self, cache: FingerprintCache):
        self.cache = cache
    
    def compile_if_changed(self, sources: list[Path], target: Path, baseline_hash: str) -> bool:
        """Compile only if sources changed since last successful build"""
        # Check if any source changed
        rebuild_needed = self._check_sources_changed(sources, baseline_hash)
        if rebuild_needed:
            result = self._compile_always(sources, target)
            if result:
                self._update_cache_success(sources, target)
            return result
        else:
            print(f"✓ {target.name} unchanged, skipping compilation")
            return True
            
    def _check_sources_changed(self, sources: list[Path], baseline_hash: str) -> bool:
        """Check if any source file has changed"""
        for src_file in sources:
            if self.cache.has_changed(src_file, baseline_hash):
                return True
        return False
```

#### 2.2 Example Compilation Integration
- **File**: `ci/ci-compile.py` - Add `--use-cache` flag for gradual adoption

```python
# ci/ci-compile.py command line integration
def parse_args(args: Optional[list[str]]) -> CompileArgs:
    parser = argparse.ArgumentParser()
    parser.add_argument("--use-cache", action="store_true", 
                       help="Enable fingerprint cache for faster incremental builds")
    parser.add_argument("--cache-file", type=Path, required=True,
                       help="Path to fingerprint cache file")
    # ... existing arguments
    
    parsed = parser.parse_args(args)
    
    # Create explicit config when cache is enabled
    if parsed.use_cache:
        cache_config = FingerprintCacheConfig(
            cache_file=parsed.cache_file,
            hash_algorithm="md5",
            ignore_patterns=[],
            max_cache_size=10000
        )
        parsed.cache_config = cache_config
    return parsed
```

#### 2.3 Cache Management Commands
- **File**: `ci/cache_manager.py` - Cache maintenance utilities

```python
# ci/cache_manager.py - Administrative tools
def cache_status(cache_file: Path) -> None:
    """Display cache statistics and health"""

def cache_clean(cache_file: Path, max_age_days: int) -> None:
    """Remove stale cache entries"""

def cache_rebuild(source_dirs: list[Path], cache_file: Path) -> None:
    """Rebuild cache from scratch by scanning source directories"""
```

### Phase 3: Testing Framework Integration (Week 5)

**Goal**: Accelerate test execution by skipping unchanged test files.

#### 3.1 Test Framework Integration
- **File**: `test.py` - Add cache support to main test runner

```python
# test.py modifications
def run_cpp_tests(args: TestArgs) -> bool:
    if args.use_cache:
        cache_config = FingerprintCacheConfig(
            cache_file=Path(".build/test_cache.json"),
            hash_algorithm="md5",
            ignore_patterns=[],
            max_cache_size=10000
        )
        cache = FingerprintCache(cache_config)
        return run_cpp_tests_with_cache(args, cache)
    else:
        return run_cpp_tests_traditional(args)

def run_cpp_tests_with_cache(args: TestArgs, cache: FingerprintCache) -> bool:
    """Run only tests whose source files have changed"""
    test_files = discover_test_files()
    
    for test_file in test_files:
        if should_run_test(test_file, cache, args.baseline_hash):
            run_single_test(test_file)
        else:
            print(f"✓ {test_file.name} unchanged, using cached results")
```

#### 3.2 Test Command Integration
```bash
# New command options
bash test --use-cache                    # Enable cache for all tests
bash test --use-cache --force-rebuild    # Rebuild cache and run all tests
bash test --cache-stats                  # Show cache hit/miss statistics
```

### Phase 4: MCP Server Integration (Week 6)

**Goal**: Expose cache functionality through MCP server for automated workflows.

#### 4.1 MCP Server Tools
- **File**: `mcp_server.py` - Add fingerprint cache tools

```python
# mcp_server.py - New tools
@server.call_tool()
async def fingerprint_cache_status(cache_file: str) -> list[TextContent]:
    """Show fingerprint cache statistics and status"""

@server.call_tool()
async def fingerprint_cache_clean(cache_file: str, max_age_days: int) -> list[TextContent]:
    """Clean stale entries from fingerprint cache"""

@server.call_tool()
async def compile_with_cache(board: str, examples: str, cache_file: str) -> list[TextContent]:
    """Compile examples using fingerprint cache for optimization"""
```

#### 4.2 Background Agent Integration
```python
# Background agents can now use:
# - fingerprint_cache_status tool for cache health monitoring
# - compile_with_cache tool for optimized compilation
# - fingerprint_cache_clean tool for maintenance
```

### Phase 5: Advanced Features (Week 7-8)

**Goal**: Add sophisticated caching features for maximum performance.

#### 5.1 Directory-Level Caching
- **Feature**: Cache entire directory modification times
- **Benefit**: Skip scanning unchanged directories entirely

```python
class DirectoryCache:
    def directory_changed(self, dir_path: Path) -> bool:
        """Check if any file in directory changed using directory modtime"""
```

#### 5.2 Dependency Tracking
- **Feature**: Track file dependencies for smarter invalidation
- **Example**: If header file changes, invalidate all dependent .cpp files

```python
class DependencyTracker:
    def get_dependencies(self, source_file: Path) -> list[Path]:
        """Extract #include dependencies from source file"""
    
    def invalidate_dependents(self, changed_file: Path) -> list[Path]:
        """Find all files that depend on the changed file"""
```

#### 5.3 Build Result Caching
- **Feature**: Cache compiled object files and link results
- **Benefit**: Avoid recompilation even when switching branches

### Phase 6: Performance Validation (Week 9)

**Goal**: Measure and optimize cache performance across realistic workloads.

#### 6.1 Performance Benchmarks
```python
# ci/benchmark_cache.py - Performance measurement suite
def benchmark_cache_performance():
    """Measure cache performance across different scenarios"""
    
    scenarios = [
        "clean_build_1000_files",      # Worst case - all cache misses
        "incremental_build_1_change",   # Best case - 1 miss, 999 hits  
        "incremental_build_10_changes", # Typical case - 10 misses, 990 hits
        "directory_scan_unchanged",     # Directory-level caching test
    ]
```

#### 6.2 Real-World Testing
- **Test environments**: CI/CD pipelines, developer workstations
- **Metrics collection**: Build time reduction, cache hit rates, disk usage
- **Target performance**:
  - 50%+ reduction in incremental build times
  - 90%+ cache hit rate in typical development
  - <100MB cache file size for large projects

### Phase 7: Production Rollout (Week 10)

**Goal**: Enable cache by default with monitoring and rollback capability.

#### 7.1 Default Enable Strategy
```python
# Gradual rollout approach:
# 1. Default enabled for background agents (MCP server)
# 2. Default enabled for CI/CD pipelines  
# 3. Default enabled for all users with opt-out flag
# 4. Remove opt-out flag after stability period
```

#### 7.2 Monitoring and Telemetry
```python
# ci/cache_telemetry.py - Performance monitoring
def collect_cache_metrics() -> CacheMetrics:
    """Collect cache performance data for analysis"""
    
@dataclass
class CacheMetrics:
    hit_rate: float
    average_lookup_time: float
    cache_size_mb: float
    build_time_savings: float
```

#### 7.3 Documentation and Training
- **Update README.md** with cache usage instructions
- **Update development docs** with cache best practices
- **Create troubleshooting guide** for cache-related issues

### Integration Milestones

| Week | Milestone | Success Criteria |
|------|-----------|------------------|
| 2 | Core Module Complete | 95% test coverage, all unit tests pass |
| 4 | Build Integration | 30% faster incremental builds |
| 5 | Test Integration | Test suite 50% faster on incremental runs |
| 6 | MCP Integration | Background agents use cache successfully |
| 8 | Advanced Features | Directory caching reduces scan time 80% |
| 9 | Performance Validation | All benchmarks meet targets |
| 10 | Production Ready | Cache enabled by default, <1% error rate |

### Risk Mitigation

#### 1. Cache Corruption
- **Mitigation**: Automatic cache rebuilding on corruption detection
- **Monitoring**: Cache validation checks in test suite

#### 2. False Cache Hits
- **Mitigation**: Content-based MD5 hashing prevents false positives
- **Testing**: Extensive edge case testing (clock changes, file copying)

#### 3. Performance Regression
- **Mitigation**: Cache can be disabled via `--no-cache` flag
- **Monitoring**: Continuous performance benchmarking

#### 4. Disk Space Usage
- **Mitigation**: Automatic cache cleanup of stale entries
- **Monitoring**: Cache size limits and cleanup policies

### Rollback Plan

If cache causes issues:
1. **Immediate**: Add `--no-cache` flag to disable cache
2. **Short-term**: Default cache to disabled while debugging
3. **Long-term**: Fix issues and re-enable with additional safeguards

### Success Metrics

- **Build Performance**: 30-50% reduction in incremental build times
- **Test Performance**: 50%+ reduction in incremental test execution time  
- **Developer Productivity**: Faster feedback loops for code changes
- **CI/CD Efficiency**: Reduced pipeline execution times
- **Resource Usage**: Lower CPU/disk usage for unchanged code

This integration plan provides a systematic approach to rolling out the fingerprint cache feature across the FastLED project while maintaining stability and backward compatibility.

# Fingerprint and Cache Management Refactoring Plan

## Current Issues

The fingerprint and cache management code in `test.py` has several issues:

1. **Mixed Responsibilities**: Fingerprint calculation and cache management are mixed together
2. **Nested Function Definitions**: `write_fingerprint` and `read_fingerprint` are defined inside `main()`
3. **Limited Reusability**: Cache operations can't be reused by other modules
4. **Tight Coupling**: Cache path handling is mixed with business logic
5. **Limited Testing**: Nested functions make unit testing difficult

## Refactoring Goals

1. **Separate Cache Management**: Create dedicated cache management module
2. **Isolate Fingerprint Logic**: Move fingerprint operations to their own module
3. **Improve Testability**: Make components independently testable
4. **Enable Reusability**: Allow other modules to use cache operations
5. **Maintain Existing Functionality**: Preserve current behavior

## Architecture Changes

### 1. Create Cache Management Module

Create `ci/ci/cache_manager.py`:

```python
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional
import json

@typechecked
@dataclass
class CacheConfig:
    """Configuration for cache operations"""
    cache_dir: Path
    create_if_missing: bool = True

class CacheManager:
    """Manages cache operations for the test system"""
    
    def __init__(self, config: CacheConfig):
        self.config = config
        if config.create_if_missing:
            self.config.cache_dir.mkdir(exist_ok=True)
    
    def write_json(self, filename: str, data: Any) -> None:
        """Write JSON data to cache file"""
        cache_file = self.config.cache_dir / filename
        with open(cache_file, "w") as f:
            json.dump(data, f, indent=2)
            
    def read_json(self, filename: str) -> Optional[Any]:
        """Read JSON data from cache file"""
        cache_file = self.config.cache_dir / filename
        if cache_file.exists():
            with open(cache_file, "r") as f:
                try:
                    return json.load(f)
                except json.JSONDecodeError:
                    return None
        return None
```

### 2. Create Fingerprint Manager Module

Create `ci/ci/fingerprint_manager.py`:

```python
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from ci.ci.cache_manager import CacheManager
from ci.ci.test_types import FingerprintResult

@typechecked
@dataclass
class FingerprintConfig:
    """Configuration for fingerprint operations"""
    cache_manager: CacheManager
    cache_filename: str = "fingerprint.json"

class FingerprintManager:
    """Manages source code fingerprinting operations"""
    
    def __init__(self, config: FingerprintConfig):
        self.config = config
        
    def write_fingerprint(self, fingerprint: FingerprintResult) -> None:
        """Write fingerprint to cache"""
        fingerprint_dict = {
            "hash": fingerprint.hash,
            "elapsed_seconds": fingerprint.elapsed_seconds,
            "status": fingerprint.status,
        }
        self.config.cache_manager.write_json(
            self.config.cache_filename, 
            fingerprint_dict
        )
        
    def read_fingerprint(self) -> Optional[FingerprintResult]:
        """Read fingerprint from cache"""
        data = self.config.cache_manager.read_json(self.config.cache_filename)
        if data:
            return FingerprintResult(
                hash=data.get("hash", ""),
                elapsed_seconds=data.get("elapsed_seconds"),
                status=data.get("status"),
            )
        return None

    def check_for_changes(self, current: FingerprintResult) -> bool:
        """Check if source code has changed since last fingerprint"""
        previous = self.read_fingerprint()
        return True if previous is None else current.hash != previous.hash
```

### 3. Update Main Script

Update `test.py` to use new modules:

```python
def main() -> None:
    try:
        # ... existing setup code ...

        # Set up cache and fingerprint management
        cache_manager = CacheManager(CacheConfig(
            cache_dir=Path(".cache")
        ))
        fingerprint_manager = FingerprintManager(FingerprintConfig(
            cache_manager=cache_manager
        ))

        # Calculate and check fingerprint
        fingerprint_data = calculate_fingerprint()
        src_code_change = fingerprint_manager.check_for_changes(fingerprint_data)
        fingerprint_manager.write_fingerprint(fingerprint_data)

        # Run tests using the test runner
        test_runner(args, src_code_change)

        # ... rest of existing code ...
```

## Implementation Plan

1. **Phase 1: Cache Manager Implementation**
   - Create CacheManager class with basic operations
   - Add JSON read/write support
   - Add error handling and validation

2. **Phase 2: Fingerprint Manager Implementation**
   - Create FingerprintManager class
   - Move fingerprint operations from test.py
   - Add change detection logic

3. **Phase 3: Integration**
   - Update test.py to use new modules
   - Verify functionality matches existing behavior
   - Add logging and error handling

4. **Phase 4: Testing**
   - Add unit tests for CacheManager
   - Add unit tests for FingerprintManager
   - Add integration tests for combined functionality

## Benefits

1. **Better Organization**: Clear separation of responsibilities
2. **Improved Testing**: Each component can be tested independently
3. **Reusability**: Cache operations available to other modules
4. **Maintainability**: Simpler code structure
5. **Extensibility**: Easy to add new cache operations

## Migration Strategy

1. Implement new modules alongside existing code
2. Add tests for new functionality
3. Switch test.py to use new modules
4. Verify all tests pass
5. Remove old implementation

## Testing Requirements

1. **Cache Manager Tests**:
   - Test file creation/reading
   - Test JSON operations
   - Test error handling

2. **Fingerprint Manager Tests**:
   - Test fingerprint reading/writing
   - Test change detection
   - Test invalid cache handling

3. **Integration Tests**:
   - Test full workflow
   - Test error conditions
   - Test cache persistence

## Future Enhancements

1. Add cache expiration
2. Add cache compression
3. Add multi-process safe operations
4. Add cache cleanup utilities
5. Add cache statistics tracking

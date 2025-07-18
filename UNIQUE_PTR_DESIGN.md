# FastLED fl::scoped_ptr to fl::unique_ptr Conversion Design Document

## Overview

This document outlines the design and implementation plan for converting `fl::scoped_ptr` to `fl::unique_ptr` in the FastLED library. The conversion will follow `std::unique_ptr` semantics for commonly accessed features while maintaining backward compatibility through template aliases.

## File Analysis

### Files containing `scoped_ptr` references

Based on codebase analysis, the following files contain `scoped_ptr` usage:

#### Source Files (`src/`):
- `src/fl/scoped_ptr.h` - Primary implementation file
- `src/fl/functional.h` - Forward declaration and type trait specialization
- `src/fl/fft_impl.h` - Member variable usage
- `src/fl/rectangular_draw_buffer.h` - Include statement
- `src/fl/ui.h` - Member variable usage (2 instances)
- `src/fl/vector.h` - Include statement
- `src/fl/wave_simulation_real.h` - Include statement
- `src/fl/xypath.h` - Member variable usage
- `src/fl/wave_simulation.h` - Include statement (2 instances)
- `src/fl/ptr.h` - Include statement
- `src/fl/lut.h` - Member variable usage
- `src/fl/fft.h` - Include statement and member variable usage
- `src/fx/2d/noisepalette.h` - Comment reference
- `src/fx/2d/animartrix.hpp` - Include statement and member variable usage
- `src/platforms/shared/ui/json/ui.cpp.hpp` - Include statement and static member usage
- `src/platforms/esp/32/clockless_spi_esp32.h` - Include statement and member variable usage
- `src/platforms/esp/32/clockless_i2s_esp32s3.cpp.hpp` - Include statement and member variable usage
- `src/platforms/arm/k20/clockless_objectfled.cpp.hpp` - Member variable usage

#### Test Files (`tests/`):
- `tests/test_ui.cpp` - Multiple instances of variable declarations and usage
- `tests/test_invoke.cpp` - Include statement, multiple test cases, and variable declarations

#### Example Files (`examples/`):
- `examples/JsonConsole/JsonConsole.ino` - Include statement, comment, and variable declaration
- `examples/FestivalStick/curr.h` - Member variable usage (2 instances)

### Files containing `scoped_array` references (for consolidation)

#### Source Files (`src/`):
- `src/fl/scoped_ptr.h` - Primary implementation file (both `scoped_array` and `scoped_array2`)
- `src/fl/circular_buffer.h` - Member variable usage (`fl::scoped_array<T>`)
- `src/fl/rectangular_draw_buffer.h` - Member variable usage (`scoped_array<u8>`)

#### Test Files (`tests/`):
- `tests/test_fx2d_blend.cpp` - Member variable usage (`scoped_array<CRGB>`)

## Header Design

### New Header Location
The `fl::unique_ptr` implementation should be placed in:
```
src/fl/unique_ptr.h
```

This follows the STL-compatible header naming convention established in the `fl/` namespace (e.g., `shared_ptr.h`, `weak_ptr.h`, `memory.h`).

### FastLED STL Equivalents Usage
**CRITICAL**: The `fl::unique_ptr` implementation must use FastLED's own STL equivalents, not standard library functions:

#### Required FL Headers:
```cpp
#include "fl/utility.h"       // for fl::move, fl::forward, fl::swap
#include "fl/type_traits.h"   // for fl::enable_if, fl::is_convertible, etc.
#include "fl/stdint.h"        // for fl::size_t
#include "fl/cstddef.h"       // for fl::nullptr_t
#include "fl/initializer_list.h"  // for consistency (though not used in unique_ptr itself)
```

#### FL Functions to Use:
- ✅ `fl::move()` instead of `std::move()`
- ✅ `fl::forward()` instead of `std::forward()`
- ✅ `fl::swap()` instead of `std::swap()`
- ✅ `fl::nullptr_t` instead of `std::nullptr_t`
- ✅ `fl::size_t` instead of `std::size_t`
- ✅ `fl::enable_if` instead of `std::enable_if`
- ✅ `fl::initializer_list` instead of `std::initializer_list` (when needed elsewhere in codebase)

#### Why This Matters:
- **Platform Compatibility**: FastLED targets embedded systems where standard library may not be available
- **Consistency**: All FastLED smart pointers use the same FL equivalents
- **Code Standards**: Follows established FastLED pattern of avoiding `std::` prefixed functions

### Template Alias for Backward Compatibility
To maintain backward compatibility, add the following template alias to `src/fl/scoped_ptr.h`:

```cpp
#pragma once

// Backward compatibility - include the new unique_ptr header
#include "fl/unique_ptr.h"

namespace fl {

// Template alias for backward compatibility
template<typename T, typename Deleter = default_delete<T>>
using scoped_ptr = unique_ptr<T, Deleter>;

// Keep existing scoped_array and scoped_array2 classes unchanged
// as they serve different purposes than unique_ptr
template <typename T, typename Deleter = ArrayDeleter<T>> 
class scoped_array {
    // ... existing implementation unchanged ...
};

template <typename T, typename Alloc = fl::allocator<T>> 
class scoped_array2 {
    // ... existing implementation unchanged ...
};

} // namespace fl
```

### Array Classes Consolidation Strategy

The codebase contains two array management classes that should be evaluated for consolidation:

#### scoped_array Analysis
- **Purpose**: Manages arrays with custom deleters (similar to `std::unique_ptr<T[]>`)
- **Usage**: Found in 3 files: `tests/test_fx2d_blend.cpp`, `src/fl/circular_buffer.h`, `src/fl/rectangular_draw_buffer.h`
- **Status**: **DEPRECATED** (marked with `FASTLED_DEPRECATED_CLASS`)
- **Consolidation**: **POSSIBLE** - Can be replaced with `unique_ptr<T[], Deleter>`

#### scoped_array2 Analysis  
- **Purpose**: Manages arrays with allocator-based memory management (constructor allocates, tracks size)
- **Usage**: **No direct usage found** in codebase (only definition)
- **Status**: **DEPRECATED** (marked with `FASTLED_DEPRECATED_CLASS`)
- **Consolidation**: **NOT RECOMMENDED** - Serves different purpose (allocator-based array with size tracking)

### Memory Header Integration
Update `src/fl/memory.h` to include the new `unique_ptr`:

```cpp
#pragma once

#include "fl/ptr.h"
#include "fl/shared_ptr.h"
#include "fl/weak_ptr.h"
#include "fl/unique_ptr.h"  // Add this line
#include "fl/type_traits.h"

namespace fl {

// ... existing content ...

// Add make_unique function for consistency with std::make_unique
template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(fl::forward<Args>(args)...));
}

// Add make_unique for arrays  
template<typename T>
unique_ptr<T[]> make_unique(fl::size_t size) {
    return unique_ptr<T[]>(new T[size]());
}

} // namespace fl
```

## Implementation Design

### Core unique_ptr Interface
The `fl::unique_ptr` implementation should provide the following `std::unique_ptr`-compatible interface:

```cpp
#pragma once

#include "fl/namespace.h"
#include "fl/type_traits.h"
#include "fl/utility.h"       // for fl::move, fl::forward
#include "fl/stdint.h"
#include "fl/cstddef.h"

namespace fl {

template<typename T>
struct default_delete {
    void operator()(T* ptr) const {
        delete ptr;
    }
};

template<typename T>
struct default_delete<T[]> {
    void operator()(T* ptr) const {
        delete[] ptr;
    }
};

template<typename T, typename Deleter = default_delete<T>>
class unique_ptr {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = T*;

private:
    pointer ptr_;
    Deleter deleter_;

public:
    // Constructors
    constexpr unique_ptr() noexcept : ptr_(nullptr), deleter_() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), deleter_() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : ptr_(p), deleter_(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), deleter_(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(fl::move(u.deleter_)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : ptr_(u.release()), deleter_(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            deleter_ = fl::move(u.deleter_);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        deleter_ = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }
    
    // Observers
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return deleter_; }
    const Deleter& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Access
    T& operator*() const { return *ptr_; }
    pointer operator->() const noexcept { return ptr_; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = p;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(ptr_, u.ptr_);
        swap(deleter_, u.deleter_);
    }
};

// Array specialization for scoped_array consolidation
template<typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = T*;

private:
    pointer ptr_;
    Deleter deleter_;

public:
    // Constructors
    constexpr unique_ptr() noexcept : ptr_(nullptr), deleter_() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), deleter_() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : ptr_(p), deleter_(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), deleter_(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(fl::move(u.deleter_)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : ptr_(u.release()), deleter_(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            deleter_ = fl::move(u.deleter_);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        deleter_ = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }
    
    // Observers
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return deleter_; }
    const Deleter& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Array access (replaces scoped_array functionality)
    T& operator[](fl::size_t i) const { return ptr_[i]; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = p;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(ptr_, u.ptr_);
        swap(deleter_, u.deleter_);
    }
};

// Non-member functions using FL equivalents
template<typename T, typename Deleter>
void swap(unique_ptr<T, Deleter>& lhs, unique_ptr<T, Deleter>& rhs) noexcept {
    lhs.swap(rhs);
}

// Comparison operators using FL equivalents
template<typename T1, typename D1, typename T2, typename D2>
bool operator==(const unique_ptr<T1, D1>& lhs, const unique_ptr<T2, D2>& rhs) {
    return lhs.get() == rhs.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator!=(const unique_ptr<T1, D1>& lhs, const unique_ptr<T2, D2>& rhs) {
    return !(lhs == rhs);
}

template<typename T, typename Deleter>
bool operator==(const unique_ptr<T, Deleter>& ptr, fl::nullptr_t) noexcept {
    return !ptr;
}

template<typename T, typename Deleter>
bool operator==(fl::nullptr_t, const unique_ptr<T, Deleter>& ptr) noexcept {
    return !ptr;
}

template<typename T, typename Deleter>
bool operator!=(const unique_ptr<T, Deleter>& ptr, fl::nullptr_t) noexcept {
    return static_cast<bool>(ptr);
}

template<typename T, typename Deleter>
bool operator!=(fl::nullptr_t, const unique_ptr<T, Deleter>& ptr) noexcept {
    return static_cast<bool>(ptr);
}

} // namespace fl
```

## Search and Replace Patterns

### Header Includes

#### Pattern 1: Direct scoped_ptr header includes
**Search Regex:**
```regex
#include\s+"fl/scoped_ptr\.h"
```

**Replace:**
```cpp
#include "fl/unique_ptr.h"
```

**Files to apply:** All files except `src/fl/scoped_ptr.h` (which will become the compatibility header)

#### Pattern 2: Add unique_ptr to functional.h forward declarations
**In `src/fl/functional.h`:**

**Search:**
```cpp
template <typename T, typename Deleter>
class scoped_ptr; // Forward declare scoped_ptr to avoid header inclusion
```

**Replace:**
```cpp
template <typename T, typename Deleter>
class scoped_ptr; // Forward declare scoped_ptr to avoid header inclusion

template <typename T, typename Deleter>
class unique_ptr; // Forward declare unique_ptr to avoid header inclusion
```

#### Pattern 3: Update type trait specializations in functional.h
**In `src/fl/functional.h`:**

**Search:**
```cpp
template <typename T, typename Deleter>
struct is_pointer_like<fl::scoped_ptr<T, Deleter>> : true_type {};
```

**Replace:**
```cpp
template <typename T, typename Deleter>
struct is_pointer_like<fl::scoped_ptr<T, Deleter>> : true_type {};

template <typename T, typename Deleter>
struct is_pointer_like<fl::unique_ptr<T, Deleter>> : true_type {};
```

### Implementation and Definition Changes

#### Pattern 4: Class member variable declarations
**Search Regex:**
```regex
fl::scoped_ptr<([^>]+)>\s+(\w+);
```

**Replace:**
```cpp
fl::unique_ptr<$1> $2;
```

#### Pattern 5: Class member variable declarations with custom deleter
**Search Regex:**
```regex
fl::scoped_ptr<([^,]+),\s*([^>]+)>\s+(\w+);
```

**Replace:**
```cpp
fl::unique_ptr<$1, $2> $3;
```

#### Pattern 6: Variable declarations in functions/methods
**Search Regex:**
```regex
fl::scoped_ptr<([^>]+)>\s+(\w+)(\([^)]*\))?;
```

**Replace:**
```cpp
fl::unique_ptr<$1> $2$3;
```

#### Pattern 7: Variable declarations with custom deleter in functions/methods
**Search Regex:**
```regex
fl::scoped_ptr<([^,]+),\s*([^>]+)>\s+(\w+)(\([^)]*\))?;
```

**Replace:**
```cpp
fl::unique_ptr<$1, $2> $3$4;
```

#### Pattern 8: Function return types
**Search Regex:**
```regex
fl::scoped_ptr<([^>]+)>&
```

**Replace:**
```cpp
fl::unique_ptr<$1>&
```

#### Pattern 9: Function parameters
**Search Regex:**
```regex
fl::scoped_ptr<([^>]+)>\s+(\w+)
```

**Replace:**
```cpp
fl::unique_ptr<$1> $2
```

#### Pattern 10: Template type parameters
**Search Regex:**
```regex
template\s*<[^>]*fl::scoped_ptr<([^>]+)>[^>]*>
```

This requires manual review as template contexts are complex.

#### Pattern 11: Comments referencing scoped_ptr
**Search Regex:**
```regex
scoped_ptr
```

**Replace:**
```cpp
unique_ptr
```

**Manual Review Required:** Comments should be updated to reflect the new naming while preserving meaning.

### scoped_array Consolidation Patterns

#### Pattern 12: scoped_array to unique_ptr<T[]> conversion
**Search Regex:**
```regex
fl::scoped_array<([^>]+)>\s+(\w+);
```

**Replace:**
```cpp
fl::unique_ptr<$1[]> $2;
```

**Files to apply:** Only the 3 files with actual usage:
- `tests/test_fx2d_blend.cpp`
- `src/fl/circular_buffer.h` 
- `src/fl/rectangular_draw_buffer.h`

#### Pattern 13: scoped_array constructor calls
**Search Regex:**
```regex
(\w+)\s*\(\s*new\s+([^[]+)\[([^\]]+)\]\s*\)
```

**Replace:**
```cpp
$1(fl::make_unique<$2[]>($3))
```

**Note:** This pattern requires manual review as constructor syntax varies.

#### Pattern 14: scoped_array with custom deleter
**Search Regex:**
```regex
fl::scoped_array<([^,]+),\s*([^>]+)>\s+(\w+);
```

**Replace:**
```cpp
fl::unique_ptr<$1[], $2> $3;
```

#### scoped_array2 Handling
**Recommendation:** **LEAVE AS LEGACY**
- `scoped_array2` serves a different purpose (allocator-based array with size management)
- Already marked as deprecated with recommendation to use `fl::vector<T, fl::allocator_psram<T>>`
- No current usage found in codebase
- Consolidation would require significant interface changes that break its intended purpose

## Files to Exclude from Search and Replace

### Complete Exclusions:
1. **`src/fl/scoped_ptr.h`** - This file will be completely rewritten as the compatibility header
2. **Binary files and build artifacts** - `.bin`, `.elf`, `.hex`, `.map`, etc.
3. **Documentation files** - `*.md` files should be manually reviewed
4. **Git and CI configuration** - `.git/`, `.github/`, `ci/` scripts that might reference scoped_ptr for testing

### Partial Exclusions (Manual Review Required):
1. **Test files** - Should be updated but with careful review to ensure test semantics remain correct
2. **Platform-specific files** - May require special handling for embedded constraints
3. **Example files** - Should be updated to demonstrate best practices with unique_ptr

## Implementation Steps

### Phase 1: Create New Header
1. Create `src/fl/unique_ptr.h` with std::unique_ptr-compatible interface
2. Update `src/fl/memory.h` to include new header
3. Add forward declaration to `src/fl/functional.h`

### Phase 2: Update Compatibility Header
1. Rewrite `src/fl/scoped_ptr.h` as compatibility header with template alias
2. Keep existing `scoped_array` and `scoped_array2` classes unchanged

### Phase 3: Systematic Replacement
1. Apply header include updates across all source files
2. Apply implementation pattern replacements using regex
3. Manual review of complex template contexts

### Phase 3.5: Array Consolidation (Optional)
1. **scoped_array Consolidation:**
   - Apply `scoped_array` to `unique_ptr<T[]>` conversion patterns
   - Update only the 3 files with actual usage
   - Test array functionality carefully (subscript operators, deleters)
   - Verify no behavioral changes

2. **scoped_array2 Decision:**
   - **LEAVE AS LEGACY** - Keep existing implementation unchanged
   - Already deprecated with clear migration path to `fl::vector`
   - No current usage in codebase
   - Serves different purpose than `unique_ptr`

### Phase 4: Testing and Validation
1. Run full test suite: `bash test`
2. Compile examples for multiple platforms
3. Verify no compilation errors or warnings
4. Ensure backward compatibility works correctly

### Phase 5: Documentation Update
1. Update any design documents mentioning scoped_ptr
2. Update code comments referencing the old naming
3. Add examples demonstrating both unique_ptr and legacy scoped_ptr usage

## Backward Compatibility Strategy

The template alias approach ensures that:
1. **Existing code continues to work** without modification
2. **New code can use modern unique_ptr semantics**
3. **Gradual migration is possible** over time
4. **No breaking changes** for existing consumers

## Validation Requirements

### Compilation Tests
- All existing examples must compile without modification
- All platform targets must build successfully
- No new compiler warnings should be introduced

### Runtime Tests
- All existing unit tests must pass
- Behavior should be identical to current scoped_ptr usage
- Memory management semantics must be preserved

### API Compatibility
- All existing scoped_ptr methods must work through the alias
- Custom deleters must continue to function
- Move semantics must work correctly

### FL Equivalents Compliance
- All FL functions (`fl::move`, `fl::forward`, `fl::swap`, etc.) must be used instead of std equivalents
- No `std::` prefixed functions should appear in the implementation
- All type traits should use FL versions (`fl::enable_if`, `fl::is_convertible`, etc.)
- Platform compatibility must be maintained across all embedded targets

This design ensures a smooth transition from `fl::scoped_ptr` to `fl::unique_ptr` while maintaining full backward compatibility and following established FastLED coding patterns. 

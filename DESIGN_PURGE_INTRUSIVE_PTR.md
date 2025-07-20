# Design Document: Purging intrusive_ptr from FastLED

## Executive Summary

This document outlines the complete migration strategy for removing `fl::intrusive_ptr<T>` from FastLED and replacing it with `fl::shared_ptr<T>` while maintaining the existing `FASTLED_SMART_PTR` macro pattern and introducing a new `fl::make_shared_no_tracking<T>()` mechanism.

## Current State Analysis

### Current Architecture

1. **Intrusive Pointer System (`fl::intrusive_ptr<T>`)**:
   - Alias for `fl::Ptr<T>` 
   - Objects must inherit from `fl::Referent`
   - Reference counting is intrusive (stored inside the object)
   - Factory function: `fl::make_intrusive<T>(args...)`
   - Pattern: `FASTLED_SMART_PTR(Foo)` → `FooPtr` typedef

2. **Current Usage Patterns**:
   ```cpp
   // Declaration pattern
   FASTLED_SMART_PTR(MyClass);
   
   // Creation pattern  
   MyClassPtr ptr = fl::make_intrusive<MyClass>(args...);
   
   // Class definition pattern
   class MyClass : public fl::Referent {
       // implementation
   };
   ```

3. **Existing shared_ptr Implementation**:
   - `fl::shared_ptr<T>` with external control blocks
   - `fl::make_shared<T>()` with inlined storage optimization
   - Standard shared_ptr semantics

## Migration Goals

1. **Complete Removal**: Eliminate all traces of `intrusive_ptr<T>` from the codebase
2. **Pattern Preservation**: Maintain `FASTLED_SMART_PTR(Foo)` → `FooPtr` naming convention
3. **Reference Counting Migration**: Move from intrusive to external reference counting
4. **No-Tracking Support**: Implement `fl::make_shared_no_tracking<T>()` functionality
5. **Backward Compatibility**: Minimize API surface changes for user code

## Design Strategy

### Phase 1: Enhanced shared_ptr Implementation

#### 1.1 Control Block Enhancement

Enhance the existing control block system to support no-tracking using a special reserved value (`0xffffffff`) in the `shared_count` field instead of a separate boolean flag. This approach provides several advantages:

- **Memory Efficiency**: No additional boolean field needed, saving 4+ bytes per control block
- **Cache Efficiency**: Fewer memory accesses during reference counting operations  
- **Atomic Operations**: Single atomic value to check instead of multiple fields
- **ABI Stability**: No change to control block size or layout

The special value `0xffffffff` (maximum uint32_t value) is ideal because:
- It's extremely unlikely to be reached through normal reference counting
- It provides a clear, unambiguous indicator of no-tracking mode
- Arithmetic operations (increment/decrement) naturally avoid this value in normal usage

```cpp
namespace fl {
namespace detail {

// Enhanced control block with no-tracking support
struct ControlBlockBase {
    fl::atomic_u32 shared_count;
    fl::atomic_u32 weak_count;
    
    // Special value indicating no-tracking mode
    static constexpr fl::u32 NO_TRACKING_VALUE = 0xffffffff;
    
    ControlBlockBase(bool track = true) 
        : shared_count(track ? 1 : NO_TRACKING_VALUE), weak_count(1) {}
        
    virtual ~ControlBlockBase() = default;
    virtual void destroy_object() = 0;
    virtual void destroy_control_block() = 0;
    
    // NEW: No-tracking aware increment/decrement
    void add_shared_ref() {
        if (shared_count != NO_TRACKING_VALUE) {
            ++shared_count;
        }
    }
    
    bool remove_shared_ref() {
        if (shared_count == NO_TRACKING_VALUE) {
            return false;  // Never destroy in no-tracking mode
        }
        return (--shared_count == 0);
    }
    
    // Check if this control block is in no-tracking mode
    bool is_no_tracking() const {
        return shared_count == NO_TRACKING_VALUE;
    }
};

// Enhanced control block for external objects with no-tracking
template<typename T, typename Deleter = default_delete<T>>
struct ControlBlock : public ControlBlockBase {
    T* ptr;
    Deleter deleter;
    
    ControlBlock(T* p, Deleter d, bool track = true) 
        : ControlBlockBase(track), ptr(p), deleter(d) {}
    
    void destroy_object() override {
        if (ptr && !is_no_tracking()) {  // Only delete if tracking
            deleter(ptr);
            ptr = nullptr;
        }
    }
    
    void destroy_control_block() override {
        delete this;
    }
};

// No-tracking tag for make_shared_no_tracking
struct no_tracking_tag {};

} // namespace detail
} // namespace fl
```

#### 1.2 Enhanced shared_ptr Class

Modify `fl::shared_ptr<T>` to support no-tracking:

```cpp
template<typename T>
class shared_ptr {
private:
    T* ptr_;
    detail::ControlBlockBase* control_block_;
    
    // Internal constructor for no-tracking
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::no_tracking_tag) 
        : ptr_(ptr), control_block_(control_block) {
        // Control block created with no_tracking=true, no reference increment needed
    }
    
    void release() {
        if (control_block_) {
            if (control_block_->remove_shared_ref()) {
                control_block_->destroy_object();
                if (--control_block_->weak_count == 0) {
                    control_block_->destroy_control_block();
                }
            }
        }
    }
    
    void acquire() {
        if (control_block_) {
            control_block_->add_shared_ref();
        }
    }

public:
    // ... existing interface remains the same ...
    
    // NEW: Check if this is a no-tracking shared_ptr
    bool is_no_tracking() const noexcept {
        return control_block_ && control_block_->is_no_tracking();
    }
    
    // NEW: use_count returns 0 for no-tracking shared_ptrs
    long use_count() const noexcept {
        if (!control_block_) return 0;
        if (control_block_->shared_count == detail::ControlBlockBase::NO_TRACKING_VALUE) {
            return 0;
        }
        return static_cast<long>(control_block_->shared_count);
    }
    
    template<typename Y> friend class shared_ptr;
    template<typename Y> friend class weak_ptr;
    
    template<typename Y, typename... Args>
    friend shared_ptr<Y> make_shared_no_tracking(Y& obj);
};
```

#### 1.3 make_shared_no_tracking Implementation

Implement the requested no-tracking factory function:

```cpp
namespace fl {

// Creates a shared_ptr that does not modify the reference count
// The shared_ptr and any copies will not affect object lifetime
template<typename T>
shared_ptr<T> make_shared_no_tracking(T& obj) {
    auto* control = new detail::ControlBlock<T, detail::no_op_deleter<T>>(
        &obj, detail::no_op_deleter<T>{}, false);  // track = false (enables no-tracking mode)
    return shared_ptr<T>(&obj, control, detail::no_tracking_tag{});
}

namespace detail {
    // Deleter that does nothing (for stack/static objects)
    template<typename T>
    struct no_op_deleter {
        void operator()(T*) const {
            // Intentionally do nothing - object lifetime managed externally
        }
    };
} // namespace detail

} // namespace fl
```

### Phase 2: Macro System Redesign

#### 2.1 Updated FASTLED_SMART_PTR Macro

Redefine the macro to use `shared_ptr` instead of `Ptr`:

```cpp
// OLD implementation (to be removed):
// #define FASTLED_SMART_PTR(type) \
//     class type; \
//     using type##Ptr = fl::Ptr<type>;

// NEW implementation:
#define FASTLED_SMART_PTR(type) \
    class type; \
    using type##Ptr = fl::shared_ptr<type>;

#define FASTLED_SMART_PTR_STRUCT(type) \
    struct type; \
    using type##Ptr = fl::shared_ptr<type>;

#define FASTLED_SMART_PTR_NO_FWD(type) \
    using type##Ptr = fl::shared_ptr<type>;
```

#### 2.2 Factory Function Redirection

Create compatibility layer for factory functions:

```cpp
namespace fl {

// Replace make_intrusive with make_shared
template <typename T, typename... Args>
shared_ptr<T> make_intrusive(Args&&... args) {
    return make_shared<T>(fl::forward<Args>(args)...);
}

// Convenience factory for the new pattern
template <typename T, typename... Args>
shared_ptr<T> make_shared_ptr(Args&&... args) {
    return make_shared<T>(fl::forward<Args>(args)...);
}

} // namespace fl
```

### Phase 3: Migration Strategy

#### 3.1 Class Definition Migration

Convert classes from inheriting `fl::Referent` to regular classes:

```cpp
// OLD pattern:
class MyClass : public fl::Referent {
public:
    MyClass(int value) : value_(value) {}
    int getValue() const { return value_; }
private:
    int value_;
};

// NEW pattern:
class MyClass {  // No inheritance from Referent
public:
    MyClass(int value) : value_(value) {}
    int getValue() const { return value_; }
private:
    int value_;
};
```

#### 3.2 Usage Pattern Migration

Update creation and usage patterns:

```cpp
// OLD usage:
FASTLED_SMART_PTR(MyClass);
MyClassPtr ptr = fl::make_intrusive<MyClass>(42);

// NEW usage (identical interface):
FASTLED_SMART_PTR(MyClass);
MyClassPtr ptr = fl::make_intrusive<MyClass>(42);  // Now calls make_shared internally

// Or explicitly:
MyClassPtr ptr = fl::make_shared<MyClass>(42);
```

#### 3.3 No-Tracking Usage

New capability for stack/static objects:

```cpp
// For stack objects:
MyClass stackObj(42);
MyClassPtr ptr = fl::make_shared_no_tracking(stackObj);

// For static objects:
static MyClass staticObj(42);
MyClassPtr ptr = fl::make_shared_no_tracking(staticObj);

// ptr and its copies will NOT affect stackObj/staticObj lifetime
// The objects must outlive all shared_ptrs pointing to them
```

### Phase 4: Implementation Plan

#### 4.1 File Modifications

1. **src/fl/memory.h**:
   - Remove `intrusive_ptr` alias
   - Add `make_shared_no_tracking` declaration
   - Update `make_intrusive` to delegate to `make_shared`

2. **src/fl/ptr.h**:
   - Mark entire file as deprecated
   - Add migration warnings
   - Eventually remove

3. **src/fl/shared_ptr.h**:
   - Add no-tracking support
   - Enhance control block system
   - Add `make_shared_no_tracking` implementation

4. **src/fl/referent.h**:
   - Mark as deprecated
   - Eventually remove

#### 4.2 Migration Order

1. **Phase 4.1**: Implement enhanced shared_ptr with no-tracking
2. **Phase 4.2**: Update FASTLED_SMART_PTR macros
3. **Phase 4.3**: Migrate core classes (FX engine, video, etc.)
4. **Phase 4.4**: Migrate peripheral classes (XY paths, audio, etc.)
5. **Phase 4.5**: Remove Referent inheritance from all classes
6. **Phase 4.6**: Remove intrusive_ptr alias and Ptr implementation
7. **Phase 4.7**: Remove Referent class entirely

#### 4.3 Automated Migration Tools

Create scripts to assist with migration:

```bash
# Script to find classes inheriting from Referent
find src/ -name "*.h" -o -name "*.cpp" | xargs grep -l "public fl::Referent"

# Script to find intrusive_ptr usage
find src/ -name "*.h" -o -name "*.cpp" | xargs grep -n "intrusive_ptr"

# Script to find make_intrusive usage  
find src/ -name "*.h" -o -name "*.cpp" | xargs grep -n "make_intrusive"
```

### Phase 5: Testing and Validation

#### 5.1 Unit Tests

```cpp
TEST_CASE("shared_ptr no-tracking functionality with 0xffffffff") {
    bool destructor_called = false;
    
    {
        TestClass obj(42, &destructor_called);
        auto ptr1 = fl::make_shared_no_tracking(obj);
        auto ptr2 = ptr1;  // Copy should not affect refcount
        
        CHECK(ptr1.use_count() == 0);  // Returns 0 for special value 0xffffffff
        CHECK(ptr2.use_count() == 0);  // Returns 0 for special value 0xffffffff
        CHECK(ptr1.is_no_tracking());
        CHECK(ptr2.is_no_tracking());
        
        // Verify internal state uses special reserved value
        auto* control = ptr1.control_block_;
        CHECK(control->shared_count == fl::detail::ControlBlockBase::NO_TRACKING_VALUE);
        CHECK(control->shared_count == 0xffffffff);
        
        // ptr1 and ptr2 destroyed here - no deletion attempted due to special value
    }
    
    // Object destroyed here by stack unwinding, not by shared_ptr
    CHECK(destructor_called);
}

TEST_CASE("no-tracking shared_ptr special value preservation") {
    TestClass obj(42);
    auto ptr1 = fl::make_shared_no_tracking(obj);
    
    // Multiple copies should preserve the special value 0xffffffff
    auto ptr2 = ptr1;
    auto ptr3 = ptr2;
    auto ptr4 = fl::shared_ptr<TestClass>(ptr3);  // Copy constructor
    
    // All should have the special value
    CHECK(ptr1.control_block_->shared_count == 0xffffffff);
    CHECK(ptr2.control_block_->shared_count == 0xffffffff);  
    CHECK(ptr3.control_block_->shared_count == 0xffffffff);
    CHECK(ptr4.control_block_->shared_count == 0xffffffff);
    
    // All should report use_count as 0 (special case for 0xffffffff)
    CHECK(ptr1.use_count() == 0);
    CHECK(ptr2.use_count() == 0);
    CHECK(ptr3.use_count() == 0);
    CHECK(ptr4.use_count() == 0);
}

TEST_CASE("FASTLED_SMART_PTR migration compatibility") {
    FASTLED_SMART_PTR(TestClass);
    
    // Should work with both old and new factory functions
    auto ptr1 = fl::make_intrusive<TestClass>(42);  // Delegates to make_shared
    auto ptr2 = fl::make_shared<TestClass>(42);     // Direct make_shared
    
    CHECK(ptr1.use_count() == 1);
    CHECK(ptr2.use_count() == 1);
    
    TestClassPtr ptr3 = ptr1;  // Copy semantics should work
    CHECK(ptr1.use_count() == 2);
    CHECK(ptr3.use_count() == 2);
    
    // Ensure these are NOT no-tracking (should not have special value)
    CHECK_FALSE(ptr1.is_no_tracking());
    CHECK_FALSE(ptr2.is_no_tracking());
    CHECK(ptr1.control_block_->shared_count != 0xffffffff);
}
```

#### 5.2 Performance Testing

Compare performance between old and new implementations:

```cpp
void benchmark_creation_performance() {
    const int iterations = 100000;
    
    // Time old intrusive_ptr creation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto ptr = fl::make_intrusive<TestClass>(i);
    }
    auto old_time = std::chrono::high_resolution_clock::now() - start;
    
    // Time new shared_ptr creation
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto ptr = fl::make_shared<TestClass>(i);
    }
    auto new_time = std::chrono::high_resolution_clock::now() - start;
    
    // Report performance comparison
}
```

### Phase 6: Documentation and Migration Guide

#### 6.1 User Migration Guide

```markdown
# FastLED Smart Pointer Migration Guide

## Breaking Changes
- Classes no longer need to inherit from `fl::Referent`
- `fl::intrusive_ptr<T>` has been removed
- `fl::make_intrusive<T>()` now delegates to `fl::make_shared<T>()`

## Updated Patterns

### Class Definition
```cpp
// OLD:
class MyEffect : public fl::Referent { /* ... */ };

// NEW:
class MyEffect { /* ... */ };  // No inheritance needed
```

### Smart Pointer Declaration
```cpp
// UNCHANGED:
FASTLED_SMART_PTR(MyEffect);  // Still creates MyEffectPtr
```

### Object Creation
```cpp
// STILL WORKS (recommended for compatibility):
auto effect = fl::make_intrusive<MyEffect>(args...);

// NEW (preferred):
auto effect = fl::make_shared<MyEffect>(args...);

// NEW (for stack/static objects):
MyEffect stackEffect(args...);
auto effectPtr = fl::make_shared_no_tracking(stackEffect);
```

## New Capabilities

### No-Tracking shared_ptr
Use `fl::make_shared_no_tracking()` for objects whose lifetime is managed externally:

```cpp
static MyEffect globalEffect(params);
auto effectPtr = fl::make_shared_no_tracking(globalEffect);
// effectPtr can be copied and passed around without affecting globalEffect lifetime
```
```

#### 6.2 Implementation Notes

```markdown
# Implementation Notes for FastLED Developers

## Memory Layout Changes
- Objects no longer contain reference count (external control block)
- Slightly higher memory overhead per shared_ptr instance
- Better cache locality for object data (no ref count mixing)

## Performance Implications
- make_shared() provides better allocation efficiency (single allocation)
- Copy operations now atomic on control block instead of object
- No-tracking shared_ptrs have zero reference counting overhead

## No-Tracking Implementation Benefits (0xffffffff approach)
- **Memory Efficient**: No additional boolean field, saves 4+ bytes per control block
- **Cache Friendly**: Single memory location to check instead of multiple fields
- **Branch Predictor Friendly**: Single comparison `shared_count == 0xffffffff`
- **Atomic Friendly**: One atomic value instead of checking multiple fields
- **ABI Stable**: Control block size unchanged, no layout modifications needed

## Thread Safety
- shared_ptr reference counting is atomic by default
- Object access still requires external synchronization
- No-tracking mode bypasses atomic operations entirely (special value never changes)
```

## Risk Assessment

### High Risk
1. **Memory Leaks**: Incorrect migration could introduce leaks
2. **Double Deletion**: Mixing old and new patterns during transition
3. **Performance Regression**: Control block overhead vs intrusive counting

### Medium Risk  
1. **API Compatibility**: Subtle behavior changes in edge cases
2. **Build Breakage**: Template instantiation changes
3. **Testing Coverage**: Ensuring all usage patterns are tested

### Low Risk
1. **Documentation**: User confusion during transition
2. **Migration Time**: Large codebase requires careful planning

## Success Metrics

1. **Functional**: All existing functionality preserved
2. **Performance**: < 5% performance regression on critical paths  
3. **Memory**: No memory leaks introduced
4. **Maintainability**: Cleaner, more standard C++ smart pointer usage
5. **Compatibility**: `FASTLED_SMART_PTR` pattern preserved
6. **Innovation**: `make_shared_no_tracking` enables new usage patterns

## Conclusion

This migration removes intrusive pointer dependencies while preserving the familiar FastLED patterns. The new `make_shared_no_tracking` functionality uses an innovative special value approach (`0xffffffff`) to enable zero-overhead shared_ptr semantics for externally managed objects, providing both compatibility and new capabilities without additional memory overhead.

Key innovations:
- **Special Value Design**: Using `0xffffffff` in `shared_count` eliminates need for separate boolean flags
- **Memory Efficiency**: No control block size increase while adding no-tracking functionality  
- **Performance Optimization**: Single atomic value check instead of multiple field access
- **Pattern Preservation**: `FASTLED_SMART_PTR` macros continue to work unchanged

The phased approach ensures stability during transition while the enhanced shared_ptr implementation provides a robust, memory-efficient foundation for future FastLED development.
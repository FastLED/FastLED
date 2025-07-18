# FastLED Intrusive Shared Pointer Design Document

## Overview
Design document for converting the existing `fl::shared_ptr` (which is actually intrusive) to properly named `fl::intrusive_ptr`, and then implementing a true `std::`-like `shared_ptr` system.

## Phase 1: Convert Existing `fl::shared_ptr` to `fl::intrusive_ptr`

### Current Situation
The existing `fl::shared_ptr<T>` in `fl/memory.h` is actually an **intrusive** smart pointer because:
- It aliases `fl::Ptr<T>` which requires objects to inherit from `fl::Referent`
- Objects manage their own reference counting
- This is the behavior of `std::intrusive_ptr`, not `std::shared_ptr`

### Transformation Plan

#### 1. Understand Current System
The current system in `fl/memory.h`:
```cpp
// This is actually intrusive behavior, not std::shared_ptr behavior
template <typename T>
using shared_ptr = fl::Ptr<T>;  // ← This should be intrusive_ptr

template <typename T, typename... Args>
fl::Ptr<T> make_shared(Args&&... args) {  // ← This should be make_intrusive
    return fl::NewPtr<T>(fl::forward<Args>(args)...);
}
```

#### 2. Required Changes

**Step 1: Rename shared_ptr to intrusive_ptr**
```bash
# Find all uses of fl::shared_ptr (the alias)
grep -r "fl::shared_ptr" src/ tests/ examples/
grep -r "fl::make_shared" src/ tests/ examples/
```

**Step 2: Create templated alias for fl::Referent**
```cpp
// Add to fl/memory.h or new fl/intrusive_ptr.h
namespace fl {
    // Templated alias for intrusive reference-counted base
    template<typename T = void>
    using intrusive_referent = fl::Referent;
    
    // Alias the old name for compatibility
    using referent = fl::Referent;
}
```

**Step 3: Update API naming**
```cpp
// Before (incorrect naming):
template <typename T>
using shared_ptr = fl::Ptr<T>;

template <typename T, typename... Args>
fl::Ptr<T> make_shared(Args&&... args);

// After (correct naming):
template <typename T>
using intrusive_ptr = fl::Ptr<T>;

template <typename T, typename... Args>
fl::Ptr<T> make_intrusive(Args&&... args);
```

#### 3. Grep/Sed Transformation Commands

```bash
# 1. Find current usage patterns
echo "=== Finding fl::shared_ptr usage ==="
grep -r "fl::shared_ptr" src/ tests/ examples/

echo "=== Finding fl::make_shared usage ==="
grep -r "fl::make_shared" src/ tests/ examples/

# 2. Transform fl::shared_ptr -> fl::intrusive_ptr
find src/ tests/ examples/ -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.ino" | \
    xargs sed -i 's/fl::shared_ptr/fl::intrusive_ptr/g'

# 3. Transform fl::make_shared -> fl::make_intrusive  
find src/ tests/ examples/ -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.ino" | \
    xargs sed -i 's/fl::make_shared/fl::make_intrusive/g'

# 4. Update fl/memory.h with correct naming
sed -i 's/using shared_ptr = fl::Ptr<T>;/using intrusive_ptr = fl::Ptr<T>;/g' src/fl/memory.h
sed -i 's/fl::Ptr<T> make_shared(/fl::Ptr<T> make_intrusive(/g' src/fl/memory.h

# 5. Update comments and documentation
find src/ tests/ examples/ -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.ino" -o -name "*.md" | \
    xargs sed -i 's/std::shared_ptr/std::intrusive_ptr/g' 
```

#### 4. Agent Instructions

**CRITICAL: Agent must follow this exact sequence:**

1. **RENAME PHASE**: 
   - Execute the grep/sed commands above to rename `fl::shared_ptr` → `fl::intrusive_ptr`
   - Execute the grep/sed commands above to rename `fl::make_shared` → `fl::make_intrusive`
   - Update `fl/memory.h` with correct intrusive naming

2. **TEST PHASE**:
   - Run `bash test` to ensure all tests pass
   - Run `bash lint` to ensure no linting errors
   - Verify compilation works: `bash compile uno --examples Blink`

3. **HALT PHASE**: 
   - **STOP and report results**
   - **DO NOT PROCEED** to Phase 2 until user confirms merge
   - Wait for user to review and merge changes

---

## Phase 2: Implement True `std::`-like `shared_ptr` System

### Overview
Implement a complete `shared_ptr` system that follows `std::shared_ptr` design exactly, including weak pointer support and aligned allocation.

### Core Design Principles

1. **Non-intrusive**: Objects don't need to inherit from anything
2. **Standard API**: Exact match to `std::shared_ptr` interface
3. **Weak pointer support**: `fl::weak_ptr` with proper semantics
4. **Allocator support**: Custom allocators via `allocate_shared`
5. **Aligned allocation**: `allocate_shared_aligned` for cache optimization
6. **Thread safety**: Reference counting (where supported by platform)

### File Structure After Phase 1 & 2

```
src/fl/
├── shared_ptr.h          # NEW: Main shared_ptr implementation (std:: equivalent)
├── weak_ptr.h            # NEW: Weak pointer implementation  
├── memory.h              # UPDATED: Contains intrusive_ptr alias + new shared_ptr
└── intrusive_ptr.h       # OPTIONAL: Dedicated intrusive pointer header

tests/
├── test_shared_ptr.cpp   # NEW: Comprehensive shared_ptr tests (std:: equivalent)
├── test_weak_ptr.cpp     # NEW: Weak pointer tests
├── test_memory.cpp       # UPDATED: Tests intrusive_ptr (renamed from shared_ptr)
└── test_intrusive_ptr.cpp # OPTIONAL: Dedicated intrusive tests
```

### Core Implementation Design

#### 1. Control Block Structure

```cpp
namespace fl {

struct ControlBlockBase {
    volatile uint32_t shared_count;
    volatile uint32_t weak_count;
    
    ControlBlockBase() : shared_count(1), weak_count(1) {}
    virtual ~ControlBlockBase() = default;
    virtual void destroy_object() = 0;
    virtual void destroy_control_block() = 0;
};

template<typename T, typename Deleter = void(*)(T*)>
struct ControlBlock : public ControlBlockBase {
    T* ptr;
    Deleter deleter;
    
    ControlBlock(T* p, Deleter d) : ptr(p), deleter(d) {}
    
    void destroy_object() override {
        deleter(ptr);
        ptr = nullptr;
    }
    
    void destroy_control_block() override {
        delete this;
    }
};

// Optimized inlined control block for make_shared
template<typename T>
struct InlinedControlBlock : public ControlBlockBase {
    alignas(T) char object_storage[sizeof(T)];
    
    T* get_object() { return reinterpret_cast<T*>(object_storage); }
    
    void destroy_object() override {
        get_object()->~T();
    }
    
    void destroy_control_block() override {
        delete this;
    }
};

}
```

#### 2. Shared Pointer Implementation

```cpp
template<typename T>
class shared_ptr {
private:
    T* ptr_;
    ControlBlockBase* control_block_;
    
    void release() {
        if (control_block_) {
            if (--control_block_->shared_count == 0) {
                control_block_->destroy_object();
                if (--control_block_->weak_count == 0) {
                    control_block_->destroy_control_block();
                }
            }
        }
    }
    
    void acquire() {
        if (control_block_) {
            ++control_block_->shared_count;
        }
    }

public:
    // Standard constructors
    constexpr shared_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    constexpr shared_ptr(std::nullptr_t) noexcept : ptr_(nullptr), control_block_(nullptr) {}
    
    template<typename Y>
    explicit shared_ptr(Y* ptr) : ptr_(ptr), control_block_(new ControlBlock<Y>(ptr, default_delete<Y>{})) {}
    
    template<typename Y, typename Deleter>
    shared_ptr(Y* ptr, Deleter d) : ptr_(ptr), control_block_(new ControlBlock<Y, Deleter>(ptr, d)) {}
    
    // Copy/move constructors
    shared_ptr(const shared_ptr& other) : ptr_(other.ptr_), control_block_(other.control_block_) {
        acquire();
    }
    
    shared_ptr(shared_ptr&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Weak pointer constructor
    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y>& weak);
    
    // Standard interface
    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    long use_count() const noexcept {
        return control_block_ ? control_block_->shared_count : 0;
    }
    
    bool unique() const noexcept { return use_count() == 1; }
    
    void reset() noexcept {
        release();
        ptr_ = nullptr;
        control_block_ = nullptr;
    }
    
    template<typename Y>
    void reset(Y* ptr) {
        shared_ptr(ptr).swap(*this);
    }
    
    template<typename Y, typename Deleter>
    void reset(Y* ptr, Deleter d) {
        shared_ptr(ptr, d).swap(*this);
    }
    
    void swap(shared_ptr& other) noexcept {
        fl::swap(ptr_, other.ptr_);
        fl::swap(control_block_, other.control_block_);
    }
};
```

#### 3. Weak Pointer Implementation

```cpp
template<typename T>
class weak_ptr {
private:
    T* ptr_;
    ControlBlockBase* control_block_;

public:
    constexpr weak_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    
    weak_ptr(const shared_ptr<T>& shared) : ptr_(shared.ptr_), control_block_(shared.control_block_) {
        if (control_block_) {
            ++control_block_->weak_count;
        }
    }
    
    long use_count() const noexcept {
        return control_block_ ? control_block_->shared_count : 0;
    }
    
    bool expired() const noexcept { return use_count() == 0; }
    
    shared_ptr<T> lock() const noexcept {
        return expired() ? shared_ptr<T>() : shared_ptr<T>(*this);
    }
    
    void reset() noexcept {
        if (control_block_) {
            if (--control_block_->weak_count == 0 && control_block_->shared_count == 0) {
                control_block_->destroy_control_block();
            }
        }
        ptr_ = nullptr;
        control_block_ = nullptr;
    }
};
```

#### 4. Factory Functions

```cpp
// Standard make_shared (optimized with inlined storage)
template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    auto* control = new InlinedControlBlock<T>();
    new(control->get_object()) T(fl::forward<Args>(args)...);
    return shared_ptr<T>(control->get_object(), control);
}

// Allocator-aware version
template<typename T, typename A, typename... Args>
shared_ptr<T> allocate_shared(const A& alloc, Args&&... args) {
    using AllocTraits = typename A::template rebind<InlinedControlBlock<T>>::other;
    AllocTraits control_alloc(alloc);
    
    auto* control = control_alloc.allocate(1);
    new(control) InlinedControlBlock<T>(control_alloc);
    new(control->get_object()) T(fl::forward<Args>(args)...);
    
    return shared_ptr<T>(control->get_object(), control);
}

// Cache-aligned version for performance
template<typename T, typename... Args>
shared_ptr<T> make_shared_aligned(size_t alignment, Args&&... args) {
    size_t total_size = sizeof(InlinedControlBlock<T>) + alignment - 1;
    void* raw = aligned_alloc(alignment, total_size);
    
    auto* control = new(raw) InlinedControlBlock<T>();
    new(control->get_object()) T(fl::forward<Args>(args)...);
    return shared_ptr<T>(control->get_object(), control);
}

// Weak pointer factory
template<typename T>
weak_ptr<T> make_weak(const shared_ptr<T>& shared) {
    return weak_ptr<T>(shared);
}
```

### API Compatibility Matrix

| std::shared_ptr Feature | fl::shared_ptr Support | Implementation Status |
|------------------------|------------------------|----------------------|
| `shared_ptr(T* ptr)` | ✅ Yes | Phase 2 |
| `shared_ptr(T* ptr, Deleter d)` | ✅ Yes | Phase 2 |
| `make_shared<T>(args...)` | ✅ Yes | Phase 2 |
| `allocate_shared<T>(alloc, args...)` | ✅ Yes | Phase 2 |
| `get()`, `operator*`, `operator->` | ✅ Yes | Phase 2 |
| `use_count()`, `unique()` | ✅ Yes | Phase 2 |
| `reset()`, `reset(ptr)`, `reset(ptr, deleter)` | ✅ Yes | Phase 2 |
| `swap()` | ✅ Yes | Phase 2 |
| `operator bool()` | ✅ Yes | Phase 2 |
| `weak_ptr` support | ✅ Yes | Phase 2 |
| Custom deleters | ✅ Yes | Phase 2 |
| Thread safety (where available) | ✅ Yes | Phase 2 |
| Cache-aligned allocation | ⭐ Enhanced | Phase 2 |

### Memory Layout Optimization

#### Standard `make_shared` Layout:
```
[ControlBlock][Object Data]
- Single allocation
- Optimal cache locality
- Automatic alignment
```

#### `allocate_shared` Layout:
```
[Custom Allocator][ControlBlock][Object Data]
- Custom allocator stored in control block
- Deallocator function pointer for type erasure
- Allocator lifetime managed by control block
```

#### `make_shared_aligned` Layout:
```
[Aligned ControlBlock][Aligned Object Data]
- Cache line alignment (64 bytes typical)
- Optimized for high-performance scenarios
- SIMD-friendly memory layout
```

### Migration Strategy

#### Phase 1: Rename Current System (IMMEDIATE)
1. **AGENT TASK**: Rename `fl::shared_ptr` → `fl::intrusive_ptr` (correct naming)
2. **AGENT TASK**: Rename `fl::make_shared` → `fl::make_intrusive` (correct naming)  
3. **AGENT TASK**: Add templated `fl::intrusive_referent<T>` alias for `fl::Referent`
4. **AGENT TASK**: Test everything works (`bash test`, `bash lint`, `bash compile`)
5. **AGENT TASK**: **HALT** and wait for user merge approval

#### Phase 2: Implement True std:: shared_ptr (AFTER MERGE)
1. **AGENT TASK**: Implement `fl/shared_ptr.h` with full `std::shared_ptr` compatibility
2. **AGENT TASK**: Implement `fl/weak_ptr.h` with `std::weak_ptr` support
3. **AGENT TASK**: Implement `make_shared_aligned` for cache-optimized allocation
4. **AGENT TASK**: Update `fl/memory.h` to include both intrusive_ptr and shared_ptr
5. **AGENT TASK**: Create comprehensive test suite matching std:: behavior
6. **AGENT TASK**: Benchmark against `std::shared_ptr` for performance validation

#### Phase 3: Integration and Documentation
1. Keep both `intrusive_ptr` and `shared_ptr` available
2. Update high-level APIs to use `shared_ptr` (std:: equivalent) by default
3. Provide clear documentation on when to use each pointer type:
   - `fl::intrusive_ptr<T>`: When objects inherit from `fl::Referent` (existing code)
   - `fl::shared_ptr<T>`: For new code requiring std:: compatibility
4. Create migration guides and best practices

### Performance Considerations

1. **Reference Counting Overhead**: Atomic operations where thread safety needed
2. **Memory Allocation**: Single allocation for `make_shared` vs double allocation for constructor
3. **Cache Locality**: Inlined storage improves cache performance
4. **Weak Pointer Overhead**: Additional weak count tracking
5. **Virtual Function Overhead**: Control block virtual calls for type erasure

### Testing Strategy

1. **Unit Tests**: Comprehensive coverage of all API functions
2. **Memory Tests**: Leak detection and proper cleanup verification
3. **Thread Safety Tests**: Multi-threaded reference counting (where applicable)
4. **Performance Tests**: Benchmarks against std::shared_ptr
5. **Compatibility Tests**: Drop-in replacement verification

### Documentation Updates

1. **API Reference**: Complete documentation matching std:: style
2. **Migration Guide**: Converting from intrusive_ptr to shared_ptr
3. **Performance Guide**: When to use each pointer type
4. **Best Practices**: Memory management patterns in FastLED
5. **Examples**: Real-world usage patterns

---

## Summary

This design provides:

1. **Standard compliance**: True `std::shared_ptr` equivalent as the primary smart pointer
2. **Performance optimization**: Aligned allocation options and optimized control blocks
3. **Complete ecosystem**: Weak pointer support with proper semantics
4. **Optional intrusive support**: `intrusive_ptr` for specialized use cases
5. **Gradual migration**: Phased adoption strategy from existing `fl::Ptr<T>` system
6. **Comprehensive testing**: Full validation suite

The result is a modern, efficient, and standards-compliant smart pointer system for FastLED that focuses on `std::`-like `shared_ptr` as the primary smart pointer, with optional intrusive support for specialized performance requirements. 

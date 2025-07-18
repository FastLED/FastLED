# FastLED True Shared Pointer Implementation

## Overview
Design document for implementing a complete `std::`-like `shared_ptr` system for FastLED. The existing intrusive pointer system (`fl::intrusive_ptr`) has been completed and is now available alongside the new non-intrusive system.

## Implementation Task: Add True `std::`-like `shared_ptr` System

**COMPLETED:** ✅ The intrusive pointer system has been renamed correctly:
- `fl::shared_ptr` → `fl::intrusive_ptr` 
- `fl::make_shared` → `fl::make_intrusive`
- All references updated in `src/`, `examples/`, and `tests/`

**REMAINING TASK:** Implement a complete `std::shared_ptr`-compatible system alongside the existing intrusive system.

### Overview
Implement a complete `shared_ptr` system that follows `std::shared_ptr` design exactly, including weak pointer support and aligned allocation.

### Core Design Principles

1. **Non-intrusive**: Objects don't need to inherit from anything
2. **Standard API**: Exact match to `std::shared_ptr` interface
3. **Weak pointer support**: `fl::weak_ptr` with proper semantics
4. **Allocator support**: Custom allocators via `allocate_shared`
5. **Aligned allocation**: `allocate_shared_aligned` for cache optimization
6. **Thread safety**: Reference counting (where supported by platform)

### File Structure for Implementation

**EXISTING FILES (already correctly updated):**
```
src/fl/
├── memory.h              # ✅ Contains intrusive_ptr alias and make_intrusive
└── [other existing files with intrusive_ptr references]

tests/
├── test_memory.cpp       # ✅ Tests intrusive_ptr (renamed from shared_ptr)
└── [other test files with intrusive_ptr references]
```

**NEW FILES TO CREATE:**
```
src/fl/
├── shared_ptr.h          # NEW: Main shared_ptr implementation (std:: equivalent)
├── weak_ptr.h            # NEW: Weak pointer implementation  

tests/
├── test_shared_ptr.cpp   # NEW: Comprehensive shared_ptr tests (std:: equivalent)
├── test_weak_ptr.cpp     # NEW: Weak pointer tests
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

### Implementation Instructions for Next Agent

**CURRENT STATUS:** ✅ Phase 1 Complete - Intrusive pointer system properly renamed

**YOUR TASK:** Implement the new `std::shared_ptr`-compatible system

#### Step 1: Implement Core Files
1. **Create `src/fl/shared_ptr.h`** with full `std::shared_ptr` API compatibility
2. **Create `src/fl/weak_ptr.h`** with `std::weak_ptr` support  
3. **Update `src/fl/memory.h`** to include the new shared_ptr alongside existing intrusive_ptr

#### Step 2: Create Test Suite
1. **Create `tests/test_shared_ptr.cpp`** with comprehensive std:: compatibility tests
2. **Create `tests/test_weak_ptr.cpp`** for weak pointer functionality

#### Step 3: Integration and Validation
1. **Run `bash test`** to ensure all tests pass (including existing intrusive_ptr tests)
2. **Run `bash lint`** to ensure code quality
3. **Test compilation:** `bash compile uno --examples Blink`

#### Step 4: Documentation
1. Keep both `intrusive_ptr` and `shared_ptr` available for different use cases
2. Document when to use each pointer type:
   - `fl::intrusive_ptr<T>`: When objects inherit from `fl::Referent` (existing code)
   - `fl::shared_ptr<T>`: For new code requiring std:: compatibility

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

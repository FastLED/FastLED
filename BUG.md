fastled_async_controller.js:317 Fatal error in FastLED pure JavaScript async loop: RuntimeError: memory access out of bounds
    at fastled.wasm.fl::AtomicFake<unsigned int>::load() const (http://localhost:8145/fastled.wasm:wasm-function[83]:0xa7c3)
    at fastled.wasm.fl::AtomicFake<unsigned int>::operator unsigned int() const (http://localhost:8145/fastled.wasm:wasm-function[82]:0xa751)
    at fastled.wasm.fl::detail::ControlBlockBase::remove_shared_ref() (http://localhost:8145/fastled.wasm:wasm-function[80]:0xa5c4)
    at fastled.wasm.fl::shared_ptr<fl::Value>::reset() (http://localhost:8145/fastled.wasm:wasm-function[1171]:0x72284)
    at fastled.wasm.fl::shared_ptr<fl::Value>::operator=(fl::shared_ptr<fl::Value> const&) (http://localhost:8145/fastled.wasm:wasm-function[1166]:0x71956)
    at fastled.wasm.fl::Json::operator=(fl::Json const&) (http://localhost:8145/fastled.wasm:wasm-function[3842]:0x1708b5)
    at fastled.wasm.fl::Json::set(fl::string const&, fl::Json const&) (http://localhost:8145/fastled.wasm:wasm-function[3838]:0x16fad3)
    at fastled.wasm.fl::Json::set(fl::string const&, fl::string const&) (http://localhost:8145/fastled.wasm:wasm-function[3852]:0x172138)
    at fastled.wasm.fl::JsonUiTitleInternal::toJson(fl::Json&) const (http://localhost:8145/fastled.wasm:wasm-function[3968]:0x17f129)
    at fastled.wasm.fl::JsonUiManager::toJson(fl::Json&) (http://localhost:8145/fastled.wasm:wasm-function[1956]:0xb4802)

## Analysis of WASM-only Crash

After analyzing the code, I've identified three likely factors causing this crash to occur specifically in the WebAssembly environment:

### 1. Memory Alignment Issues with Atomic Types in WASM

The `ControlBlockBase` structure contains two `fl::atomic_u32` members:
```cpp
struct ControlBlockBase {
    fl::atomic_u32 shared_count;
    fl::atomic_u32 weak_count;
    // ...
};
```

According to `fl/align.h`, when compiling for Emscripten/WASM:
```cpp
#ifdef __EMSCRIPTEN__
#define FL_ALIGN_BYTES 8
#define FL_ALIGN alignas(FL_ALIGN_BYTES)
#define FL_ALIGN_AS(T) alignas(alignof(T))
#else
#define FL_ALIGN_BYTES 1
#define FL_ALIGN
#define FL_ALIGN_AS(T)
#endif
```

However, the `AtomicFake<T>` class doesn't use any alignment directives:
```cpp
template <typename T> class AtomicFake {
  private:
    T mValue;  // No alignment specified
};
```

In WASM, atomic operations require proper alignment (4-byte alignment for `uint32_t`), but the `AtomicFake` class doesn't enforce this. This could cause the "memory access out of bounds" error when trying to access the atomic value in WASM, which has stricter alignment requirements than native platforms.

### 2. WebAssembly Memory Model Differences

In WASM:
- Linear memory is laid out differently than in native environments
- Bounds checking is more strict
- Memory access patterns that work in native code might fail in WASM due to how memory pages are allocated and protected

When a `ControlBlockBase` is allocated and later deallocated, the memory might be marked as inaccessible. If there's a dangling reference to this memory (due to a bug in shared_ptr reference counting), accessing it in native code might just return garbage values, but in WASM it triggers a hard "memory access out of bounds" error.

### 3. Initialization Order Issues with Static UI Components

The crash occurs during UI serialization (`JsonUiTitleInternal::toJson`), which suggests it's related to static UI components. In WASM:
- Global/static object initialization order can be different than in native environments
- Objects might be accessed before they're fully constructed
- The WASM module instantiation process might have different timing characteristics

The `JsonUiManager` maintains a collection of UI components, and during serialization, it's trying to access a component that might not be properly initialized in the WASM environment, leading to the crash when its shared_ptr tries to access a deallocated control block.

These factors explain why the same code works fine in native environments but crashes specifically in WASM - it's a combination of alignment requirements, memory model differences, and initialization timing issues that are unique to the WebAssembly execution environment.
## Memory Access Out of Bounds in WASM Build

### Error Details
```
fastled_async_controller.js:317 Fatal error in FastLED pure JavaScript async loop: RuntimeError: memory access out of bounds
    at fastled.wasm.fl::AtomicFake<unsigned int>::load() const (http://localhost:8142/fastled.wasm:wasm-function[83]:0xa712)
    at fastled.wasm.fl::AtomicFake<unsigned int>::operator unsigned int() const (http://localhost:8142/fastled.wasm:wasm-function[82]:0xa6a0)
    at fastled.wasm.fl::detail::ControlBlockBase::remove_shared_ref() (http://localhost:8142/fastled.wasm:wasm-function[80]:0xa52c)
    at fastled.wasm.fl::shared_ptr<fl::json2::Value>::release() (http://localhost:8142/fastled.wasm:wasm-function[1164]:0x719bf)
    at fastled.wasm.fl::shared_ptr<fl::json2::Value>::operator=(fl::shared_ptr<fl::json2::Value> const&) (http://localhost:8142/fastled.wasm:wasm-function[1161]:0x712b4)
    at fastled.wasm.fl::json2::Json::operator=(fl::json2::Json const&) (http://localhost:8142/fastled.wasm:wasm-function[1156]:0x70682)
    at fastled.wasm.fl::json2::Json::set(fl::string const&, fl::json2::Json const&) (http://localhost:8142/fastled.wasm:wasm-function[3915]:0x17200f)
    at fastled.wasm.fl::json2::Json::set(fl::string const&, fl::string const&) (http://localhost:8142/fastled.wasm:wasm-function[4077]:0x17eca0)
    at fastled.wasm.fl::JsonTitleImpl::toJson(fl::json2::Json&) const (http://localhost:8142/fastled.wasm:wasm-function[4171]:0x18a2d3)
    at fastled.wasm.fl::JsonTitleImpl::JsonTitleImpl(fl::string const&)::$_0::operator()(fl::json2::Json&) const (http://localhost:8142/fastled.wasm:wasm-function[4170]:0x189f10)
```

### Root Cause Analysis

The error occurs when using `shared_ptr` in a WASM environment. The stack trace shows that the issue happens during reference counting operations in `AtomicFake<unsigned int>::load()`.

Key components involved:
1. `AtomicFake` class in `src/fl/atomic.h` - Provides a non-atomic implementation of atomic operations for single-threaded environments
2. `shared_ptr` implementation in `src/fl/shared_ptr.h` - Uses `AtomicFake` for reference counting
3. JSON UI components in `src/platforms/shared/ui/json/` - Particularly `JsonTitleImpl` which uses `shared_ptr`
4. The JSON implementation also uses `Variant`, `optional`, and other complex data structures

The issue is happening during the `JsonTitleImpl::toJson()` method where it calls `json.set()` with string values. This eventually leads to a copy assignment of `shared_ptr<fl::json2::Value>` which triggers the reference counting mechanism.

The problem appears to be a memory access out of bounds error when the `AtomicFake::load()` method tries to access memory that has already been deallocated or is outside the valid memory range.

It's particularly concerning that we're using variants, optional types, and complex data structures that may have intricate memory management requirements. It's possible that the code is accessing data from a dead object that it shouldn't be deleting, especially given the complex interplay between JSON values, shared pointers, and variant types.

### Potential Solutions

1. **Check for invalid use of fl::variant/fl::optional**: These will not protect the user from accessing memory when they use some operations. This is the most likely cause.
1. **Check for null control block**: Add null checks in `AtomicFake::load()` before accessing `mValue`
2. **Verify control block lifetime**: Ensure that `ControlBlockBase` objects have proper lifetime management
3. **Use no-tracking shared_ptr**: Consider using `make_shared_no_tracking()` for UI components that don't need reference counting
4. **Memory alignment issues**: Check for potential memory alignment issues in WASM environment

### Next Steps

1. Add additional null checks in the `AtomicFake` and `shared_ptr` implementations
2. Investigate the lifetime of `JsonUiInternal` objects and their associated control blocks
3. Consider using the no-tracking mode for UI components that have static lifetime
4. Test with memory debugging tools in the WASM environment

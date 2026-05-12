#pragma once

// IWYU pragma: private

/// @file coroutine_platform_wasm.hpp
/// @brief WASM coroutine platform using JSPI (JavaScript Promise Integration)
///
/// Implements ICoroutinePlatform using JSPI for cooperative context switching
/// in WASM. Unlike asyncify, JSPI handles stack management at the engine level
/// with zero WASM code transformation — no whitelist needed, no size overhead.
///
/// Each coroutine runs as a separate JSPI-promising WASM call. Context switching
/// suspends the caller via an EM_ASYNC_JS import and resumes the target by
/// resolving its pending Promise. A lightweight JS scheduler coordinates this.
///
/// Requires -sJSPI linker flag. Supported in Chrome 123+.

#include "platforms/wasm/is_wasm.h"

// JSPI platform is compiled out when the pthread back-end is selected.
// See platforms/wasm/coroutine_platform_wasm_pthread.hpp.
#if defined(FL_IS_WASM) && !defined(FASTLED_WASM_PTHREADS)

// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/em_js.h>
// IWYU pragma: end_keep

#include "platforms/coroutine_runtime.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

static constexpr int kMaxJspiContexts = 32;

/// Entry function table — indexed by context ID.
/// ID 0 is reserved for the runner (main thread) context.
static void (*g_jspi_entries[kMaxJspiContexts])() = {};
static int g_jspi_next_id = 1;

}  // namespace platforms
}  // namespace fl

// ==========================================================================
// WASM export: coroutine entry point (called from JS scheduler)
// ==========================================================================
// Must be in JSPI_EXPORTS so the engine wraps it as a promising export.
// When called, the WASM runs synchronously until the coroutine's first yield.

extern "C" EMSCRIPTEN_KEEPALIVE void jspi_run_entry(int id) {
    using namespace fl::platforms;
    if (id >= 0 && id < kMaxJspiContexts && g_jspi_entries[id]) {
        g_jspi_entries[id]();
    }
}

// ==========================================================================
// JS scheduler glue — forward declarations (defined via EM_JS in
// coroutine_platform_wasm.js.cpp.hpp, included from _build.cpp.hpp)
// ==========================================================================
extern "C" {
void _jspi_register(int id);
void _jspi_unregister(int id);
void _jspi_context_switch(int from_id, int to_id);
}

// ==========================================================================
// CoroutinePlatformWasm — ICoroutinePlatform implementation
// ==========================================================================

namespace fl {
namespace platforms {

// NOLINTBEGIN(reinterpret_cast)
// Context IDs are small ints stored in void* — reinterpret_cast is unavoidable
// because the ICoroutinePlatform interface uses void* for platform contexts.

inline void* id_to_ctx(int id) FL_NOEXCEPT {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(id));  // ok reinterpret cast
}

inline int ctx_to_id(void* ctx) FL_NOEXCEPT {
    return static_cast<int>(reinterpret_cast<uintptr_t>(ctx));  // ok reinterpret cast
}
// NOLINTEND(reinterpret_cast)

class CoroutinePlatformWasm : public ICoroutinePlatform {
public:
    void* createContext(void (*entry_fn)(), size_t /*stack_size*/) override {
        // JSPI manages stacks at the engine level — stack_size is ignored
        int id = g_jspi_next_id++;
        if (id >= kMaxJspiContexts) {
            return nullptr;
        }
        g_jspi_entries[id] = entry_fn;
        _jspi_register(id);
        return id_to_ctx(id);
    }

    void* createRunnerContext() FL_NOEXCEPT override {
        // Runner is always ID 0
        _jspi_register(0);
        return id_to_ctx(0);
    }

    void destroyContext(void* ctx) FL_NOEXCEPT override {
        int id = ctx_to_id(ctx);
        if (id >= 0 && id < kMaxJspiContexts) {
            g_jspi_entries[id] = nullptr;
            _jspi_unregister(id);
        }
    }

    void contextSwitch(void* from_ctx, void* to_ctx) FL_NOEXCEPT override {
        _jspi_context_switch(ctx_to_id(from_ctx), ctx_to_id(to_ctx));
    }

    fl::u32 micros() const FL_NOEXCEPT override {
        // emscripten_get_now() returns milliseconds as double
        return static_cast<fl::u32>(emscripten_get_now() * 1000.0);
    }
};

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM && !FASTLED_WASM_PTHREADS

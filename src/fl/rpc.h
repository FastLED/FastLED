#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

// =============================================================================
// RPC System - Main Public API
// =============================================================================
//
// This header provides the complete typed RPC system for FastLED.
//
// PUBLIC API:
//   - fl::Rpc          : Main RPC registry class
//   - fl::RpcHandle<S> : Callable handle returned from method() registration
//
// EXAMPLE USAGE:
//
//   #include "fl/rpc.h"
//
//   fl::Rpc rpc;
//
//   // Register method with auto-deduced signature (RECOMMENDED)
//   auto add = rpc.method("add", [](int a, int b) { return a + b; });
//   int result = add(2, 3);  // Direct call via handle
//
//   // Group methods using dot notation for namespacing:
//   rpc.method("led.setBrightness", [](int b) { /* ... */ });
//   rpc.method("led.setColor", [](int r, int g, int b) { /* ... */ });
//   rpc.method("system.status", []() -> fl::string { return "ok"; });
//
//   // Bind by name and call later
//   auto bound = rpc.bind<int(int, int)>("add");
//   result = bound(5, 7);
//
//   // Direct call without binding
//   result = rpc.call<int>("add", 10, 20);
//
//   // JSON-RPC transport
//   fl::Json request = fl::Json::parse(R"({"method":"add","params":[6,7],"id":1})");
//   fl::Json response = rpc.handle(request);
//
//   // Schema generation (OpenRPC format)
//   fl::Json schema = rpc.schema();      // Full OpenRPC document
//   fl::Json methods = rpc.methods();    // Just method list
//
//   // Method registration with metadata (fluent API)
//   auto mul = rpc.method_with("multiply", [](int a, int b) { return a * b; })
//       .params({"a", "b"})
//       .description("Multiplies two integers")
//       .tags({"math"})
//       .done();
//
// =============================================================================

// Internal detail headers
#include "fl/detail/rpc/rpc_handle.h"
#include "fl/detail/rpc/rpc_registry.h"
#include "fl/detail/rpc/rpc_method_builder.h"

// STL headers required for public API
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/function.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/initializer_list.h"

namespace fl {

// =============================================================================
// Rpc - Main typed RPC registry
// =============================================================================
//
// The primary class for registering and invoking RPC methods.
// Methods can be registered with auto-deduced signatures and called either
// directly (via RpcHandle), by binding, or through JSON-RPC transport.

class Rpc {
public:
    Rpc() : mDiscoverEnabled(false) {}
    ~Rpc() = default;

    // Non-copyable but movable
    Rpc(const Rpc&) = delete;
    Rpc& operator=(const Rpc&) = delete;
    Rpc(Rpc&&) = default;
    Rpc& operator=(Rpc&&) = default;

    // =========================================================================
    // Method Registration
    // =========================================================================

    /// Register a method with auto-deduced signature.
    /// Returns an RpcHandle for immediate invocation.
    /// Supports dot notation for namespacing: "led.setBrightness", "system.status"
    template<typename Callable>
    auto method(const char* name, Callable&& fn)
        -> RpcHandle<typename callable_traits<typename decay<Callable>::type>::signature> {
        using Sig = typename callable_traits<typename decay<Callable>::type>::signature;
        RpcFn<Sig> wrapped(fl::forward<Callable>(fn));
        bool ok = registerMethod<Sig>(name, wrapped, fl::vector<fl::string>(), "", fl::vector<fl::string>());
        if (ok) {
            return RpcHandle<Sig>(wrapped);
        }
        return RpcHandle<Sig>();
    }

    /// Register a method with explicit signature (backwards compatible).
    template<class Sig>
    bool method(const char* name, RpcFn<Sig> fn) {
        return registerMethod<Sig>(name, fn, fl::vector<fl::string>(), "", fl::vector<fl::string>());
    }

    /// Fluent builder for method registration with metadata.
    /// Call .params(), .description(), .tags() and finally .done() to register.
    template<typename Callable>
    auto method_with(const char* name, Callable&& fn)
        -> detail::MethodBuilder<typename callable_traits<typename decay<Callable>::type>::signature> {
        using Sig = typename callable_traits<typename decay<Callable>::type>::signature;
        return detail::MethodBuilder<Sig>(this, name, RpcFn<Sig>(fl::forward<Callable>(fn)));
    }

    // =========================================================================
    // Method Binding and Invocation
    // =========================================================================

    /// Returns a typed callable for local use.
    /// Returns empty function if method not found or signature mismatch.
    template<class Sig>
    RpcFn<Sig> bind(const char* name) const {
        auto result = try_bind<Sig>(name);
        if (result.has_value()) {
            return result.value();
        }
        return RpcFn<Sig>();
    }

    /// Returns optional - nullopt if method not found or signature mismatch.
    template<class Sig>
    fl::optional<RpcFn<Sig>> try_bind(const char* name) const {
        fl::string key(name);
        auto it = mRegistry.find(key);
        if (it == mRegistry.end()) {
            return fl::nullopt;
        }
        if (it->second.mTypeTag != detail::TypeTag<Sig>::id()) {
            return fl::nullopt;
        }
        auto* holder = static_cast<detail::TypedCallableHolder<Sig>*>(
            it->second.mTypedCallable.get());
        if (!holder) {
            return fl::nullopt;
        }
        return holder->mFn;
    }

    /// Check if a method is registered (regardless of signature).
    bool has(const char* name) const {
        return mRegistry.find(fl::string(name)) != mRegistry.end();
    }

    /// Direct invocation without binding.
    /// Returns R (undefined behavior if method not found or signature mismatch).
    template<typename R, typename... Args>
    R call(const char* name, Args&&... args) {
        auto fn = bind<R(Args...)>(name);
        return fn(fl::forward<Args>(args)...);
    }

    /// Safe direct invocation with optional return.
    /// Returns nullopt if method not found or signature mismatch.
    template<typename R, typename... Args>
    fl::optional<R> try_call(const char* name, Args&&... args) {
        auto opt = try_bind<R(Args...)>(name);
        if (!opt.has_value()) {
            return fl::nullopt;
        }
        return opt.value()(fl::forward<Args>(args)...);
    }

    // =========================================================================
    // JSON-RPC Transport
    // =========================================================================

    /// Process a JSON-RPC request.
    /// Request format: {"method": "name", "params": [...], "id": ...}
    /// Response format: {"result": ..., "id": ...} or {"error": {...}, "id": ...}
    Json handle(const Json& request);

    /// For notifications (no id), returns nullopt.
    fl::optional<Json> handle_maybe(const Json& request);

    // =========================================================================
    // Schema and Discovery
    // =========================================================================

    /// Enable built-in rpc.discover method.
    void enableDiscover(const char* title = "RPC API", const char* version = "1.0.0") {
        if (mDiscoverEnabled) return;
        mDiscoverEnabled = true;
        mSchemaTitle = title;
        mSchemaVersion = version;
    }

    /// Returns array of method schemas.
    Json methods() const;

    /// Returns full OpenRPC document.
    /// See: https://spec.open-rpc.org/
    Json schema(const char* title = "RPC API", const char* version = "1.0.0") const;

    /// Returns number of registered methods.
    fl::size count() const {
        return mRegistry.size();
    }

    /// Returns list of unique tags used across all methods.
    fl::vector<fl::string> tags() const;

    // =========================================================================
    // Internal Registration (used by MethodBuilder)
    // =========================================================================

    template<class Sig>
    bool registerMethod(const char* name, RpcFn<Sig> fn,
                        const fl::vector<fl::string>& paramNames,
                        const fl::string& description,
                        const fl::vector<fl::string>& tags) {
        fl::string key(name);
        auto it = mRegistry.find(key);
        if (it != mRegistry.end()) {
            if (it->second.mTypeTag != detail::TypeTag<Sig>::id()) {
                return false;
            }
        }

        detail::RpcEntry entry;
        entry.mTypeTag = detail::TypeTag<Sig>::id();
        entry.mInvoker = fl::make_shared<detail::TypedInvoker<Sig>>(fn);
        entry.mTypedCallable = fl::make_shared<detail::TypedCallableHolder<Sig>>(fn);
        entry.mSchemaGenerator = fl::make_shared<detail::TypedSchemaGenerator<Sig>>();
        entry.mDescription = description;
        entry.mTags = tags;

        if (!paramNames.empty()) {
            entry.mSchemaGenerator->setParamNames(paramNames);
        }

        mRegistry[key] = fl::move(entry);
        return true;
    }

private:
    fl::unordered_map<fl::string, detail::RpcEntry> mRegistry;
    bool mDiscoverEnabled;
    fl::string mSchemaTitle;
    fl::string mSchemaVersion;
};

// RpcFactory is kept as an alias for backwards compatibility
using RpcFactory = Rpc;

// =============================================================================
// MethodBuilder::done() implementation (requires Rpc to be fully defined)
// =============================================================================

namespace detail {

template<typename Sig>
RpcHandle<Sig> MethodBuilder<Sig>::done() {
    bool ok = mFactory->registerMethod<Sig>(
        mName.c_str(), mFn, mParamNames, mDescription, mTags);
    if (ok) {
        return RpcHandle<Sig>(mFn);
    }
    return RpcHandle<Sig>();
}

} // namespace detail

} // namespace fl

#endif // FASTLED_ENABLE_JSON

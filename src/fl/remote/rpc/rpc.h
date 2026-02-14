#pragma once

#include "fl/json.h"

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
//   #include "fl/remote/rpc.h"
//
//   fl::Rpc rpc;
//
//   // Bind (register) method - simple (constructor style)
//   rpc.bind(Rpc::Config{"add", [](int a, int b) { return a + b; }});
//
//   // Bind method - simple (designated initializer style)
//   rpc.bind({.name = "add", .fn = [](int a, int b) { return a + b; }});
//
//   // With metadata - designated initializer style
//   rpc.bind({
//       .name = "multiply",
//       .fn = [](int a, int b) { return a * b; },
//       .params = {"a", "b"},
//       .description = "Multiplies two integers",
//       .tags = {"math"}
//   });
//
//   // With metadata - constructor style
//   rpc.bind(Rpc::Config{
//       "divide",                             // name
//       [](int a, int b) { return a / b; },   // function
//       {"dividend", "divisor"},              // params
//       "Divides two integers"                // description
//   });
//
//   // Group methods using dot notation for namespacing:
//   rpc.bind({"led.setBrightness", [](int b) { /* ... */ }});
//   rpc.bind({"led.setColor", [](int r, int g, int b) { /* ... */ }});
//   rpc.bind({"system.status", []() -> fl::string { return "ok"; }});
//
//   // Get by name and call later
//   auto addFn = rpc.get<int(int, int)>("add");
//   if (addFn) {  // Check if binding succeeded
//       int sum = addFn(5, 7);  // Call directly!
//   } else if (addFn.error() == fl::BindError::NotFound) {
//       // Method not registered
//   } else {  // fl::BindError::SignatureMismatch
//       // Method exists but signature doesn't match
//   }
//
//   // JSON-RPC transport
//   fl::Json request = fl::Json::parse(R"({"method":"add","params":[6,7],"id":1})");
//   fl::Json response = rpc.handle(request);
//
//   // Schema generation (flat tuple format) - always available
//   fl::Json schema = rpc.schema();      // Flat schema document
//   fl::Json methods = rpc.methods();    // Flat method array
//
//   // Built-in rpc.discover method (always available)
//   fl::Json request = fl::Json::parse(R"({"method":"rpc.discover","id":1})");
//   fl::Json response = rpc.handle(request);  // Returns flat schema
//
// =============================================================================

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

// Internal RPC headers
#include "fl/remote/rpc/rpc_handle.h"
#include "fl/remote/rpc/rpc_registry.h"

// STL headers required for public API
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/expected.h"
#include "fl/stl/function.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/initializer_list.h"

namespace fl {

// =============================================================================
// BindError - Error codes for bind() failures
// =============================================================================

enum class BindError {
    NotFound,          // No method registered with that name
    SignatureMismatch  // Method exists but signature doesn't match
};

// =============================================================================
// RPC Schema Types
// =============================================================================

/**
 * @brief Method parameter information
 */
struct ParamInfo {
    fl::string name;
    fl::string type;
};

/**
 * @brief Method information
 */
struct MethodInfo {
    fl::string name;
    fl::vector<ParamInfo> params;
    fl::string returnType;
    fl::string description;
    fl::vector<fl::string> tags;
};

// =============================================================================
// BindResult - Result wrapper for bind() operations
// =============================================================================

/// Wraps the result of binding to a method by name.
/// Contains either a callable function or a BindError indicating why binding failed.
/// Can be called directly like a function if binding succeeded.
template<typename Sig>
struct BindResult;

template<typename R, typename... Args>
struct BindResult<R(Args...)> {
    fl::expected<RpcFn<R(Args...)>, BindError> inner;

    /// Construct from expected
    BindResult(fl::expected<RpcFn<R(Args...)>, BindError> exp) : inner(fl::move(exp)) {}

    /// Construct from RpcFn (success case)
    BindResult(RpcFn<R(Args...)> fn) : inner(fl::expected<RpcFn<R(Args...)>, BindError>::success(fl::move(fn))) {}

    /// Check if binding succeeded
    bool has_value() const { return inner.has_value(); }
    bool ok() const { return inner.has_value(); }
    explicit operator bool() const { return inner.has_value(); }

    /// Get the callable (throws if error)
    RpcFn<R(Args...)> value() const { return inner.value(); }
    RpcFn<R(Args...)>& value() { return inner.value(); }

    /// Get the error (undefined if has_value())
    BindError error() const { return inner.error(); }

    /// Access the underlying expected
    const fl::expected<RpcFn<R(Args...)>, BindError>& get() const { return inner; }

    /// Call the bound function directly (undefined behavior if binding failed)
    /// Always check ok() or has_value() before calling!
    R operator()(Args... args) const {
        return inner.value()(fl::forward<Args>(args)...);
    }
};

// =============================================================================
// Rpc - Main typed RPC registry
// =============================================================================
//
// The primary class for registering and invoking RPC methods.
// Methods can be registered with auto-deduced signatures and called either
// directly (via RpcHandle), by binding, or through JSON-RPC transport.

class Rpc {
public:
    /// Configuration for method registration with optional metadata.
    /// Name and function are required, metadata fields are optional.
    template<typename Callable>
    struct Config {
        fl::string name;                            ///< Method name (REQUIRED)
        Callable fn;                                ///< Function to register (REQUIRED)
        fl::vector<fl::string> params = {};         ///< Parameter names (optional)
        fl::string description = "";                ///< Method description (optional)
        fl::vector<fl::string> tags = {};           ///< Tags for grouping (optional)

        /// Constructor requiring name and function (metadata is optional)
        Config(fl::string n, Callable f)
            : name(fl::move(n)), fn(fl::move(f)) {}

        /// Constructor with params
        Config(fl::string n, Callable f, fl::vector<fl::string> p)
            : name(fl::move(n)), fn(fl::move(f)), params(fl::move(p)) {}

        /// Constructor with all fields
        Config(fl::string n,
               Callable f,
               fl::vector<fl::string> p,
               fl::string desc,
               fl::vector<fl::string> t = {})
            : name(fl::move(n)), fn(fl::move(f)), params(fl::move(p)),
              description(fl::move(desc)), tags(fl::move(t)) {}
    };

    Rpc() = default;
    ~Rpc() = default;

    // Non-copyable but movable
    Rpc(const Rpc&) = delete;
    Rpc& operator=(const Rpc&) = delete;
    Rpc(Rpc&&) = default;
    Rpc& operator=(Rpc&&) = default;

    // =========================================================================
    // Method Registration (Binding)
    // =========================================================================

    /// Bind a method with configuration (name, function, optional metadata).
    /// Supports dot notation for namespacing: "led.setBrightness", "system.status"
    /// Use get() to retrieve a callable if you need to invoke the method from C++ code.
    template<typename Callable>
    void bind(const Config<Callable>& config) {
        using Sig = typename callable_traits<typename decay<Callable>::type>::signature;
        RpcFn<Sig> wrapped(config.fn);
        registerMethod<Sig>(config.name.c_str(), wrapped, config.params, config.description, config.tags);
    }

    /// Convenience overload: bind method by name and function
    template<typename Callable>
    void bind(const char* name, Callable fn) {
        bind(Config<Callable>{name, fl::move(fn)});
    }

    // =========================================================================
    // Method Retrieval
    // =========================================================================

    /// Get a registered method by name.
    /// Returns BindResult containing typed callable, or error if not found or signature mismatch.
    template<class Sig>
    BindResult<Sig> get(const char* name) const {
        fl::string key(name);
        auto it = mRegistry.find(key);
        if (it == mRegistry.end()) {
            return BindResult<Sig>(fl::expected<RpcFn<Sig>, BindError>::failure(BindError::NotFound));
        }
        if (it->second.mTypeTag != detail::TypeTag<Sig>::id()) {
            return BindResult<Sig>(fl::expected<RpcFn<Sig>, BindError>::failure(BindError::SignatureMismatch));
        }
        auto* holder = static_cast<detail::TypedCallableHolder<Sig>*>(
            it->second.mTypedCallable.get());
        if (!holder) {
            return BindResult<Sig>(fl::expected<RpcFn<Sig>, BindError>::failure(BindError::NotFound));
        }
        return BindResult<Sig>(holder->mFn);
    }

    /// Check if a method is registered (regardless of signature).
    bool has(const char* name) const {
        return mRegistry.find(fl::string(name)) != mRegistry.end();
    }

    /// Unbind (unregister) a previously registered method.
    /// Returns true if method was found and removed, false otherwise.
    bool unbind(const char* name) {
        fl::string key(name);
        auto it = mRegistry.find(key);
        if (it != mRegistry.end()) {
            mRegistry.erase(it);
            return true;
        }
        return false;
    }

    /// Clear all registered methods.
    void clear() {
        mRegistry.clear();
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

    /// Returns flat method array: [["name", "returnType", [["param1", "type1"], ...]], ...]
    /// Format: Array of method tuples optimized for low-memory devices.
    /// Each method is represented as: ["methodName", "returnType", [["param1", "type1"], ...]]
    Json methods() const;

    /// Returns flat schema document.
    /// Format: {"schema": [["methodName", "returnType", [["param1", "type1"], ...]], ...]}
    /// The built-in "rpc.discover" method returns this schema.
    Json schema() const;

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
};

// RpcFactory is kept as an alias for backwards compatibility
using RpcFactory = Rpc;

} // namespace fl

#endif // FASTLED_ENABLE_JSON

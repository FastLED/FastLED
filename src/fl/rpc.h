#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

// =============================================================================
// RPC System - Main Include Point
// =============================================================================
//
// This header provides the complete typed RPC system for FastLED. Include this
// single header to get access to the high-level API:
//
// HIGH-LEVEL API (use these):
//   - RpcFactory      : Main RPC registry - register methods, bind, call
//   - RpcFn<Sig>      : Type alias for fl::function<Sig>
//   - RpcHandle<Sig>  : Callable handle returned from method() registration
//
// EXAMPLE USAGE:
//
//   #include "fl/rpc.h"
//
//   fl::RpcFactory rpc;
//
//   // Register with auto-deduced signature (RECOMMENDED)
//   auto add = rpc.method("add", [](int a, int b) { return a + b; });
//   int result = add(2, 3);  // Direct call via handle
//
//   // Or bind later by name
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
// =============================================================================

// Detail headers containing implementation (hidden from users)
#include "fl/detail/rpc/type_conversion_result.h"
#include "fl/detail/rpc/json_visitors.h"
#include "fl/detail/rpc/json_to_type.h"
#include "fl/detail/rpc/type_to_json.h"
#include "fl/detail/rpc/function_traits.h"
#include "fl/detail/rpc/json_arg_converter.h"
#include "fl/detail/rpc/typed_rpc_binding.h"

// Required STL headers for the public API
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/function.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/type_traits.h"

namespace fl {

// =============================================================================
// RpcFn - Type alias for typed RPC callables
// =============================================================================

template<class Sig>
using RpcFn = fl::function<Sig>;

// Forward declaration
class RpcFactory;

// =============================================================================
// RpcHandle - Callable handle returned from method() for immediate use
// =============================================================================

template<typename Sig>
class RpcHandle;

// Specialization for function signatures
template<typename R, typename... Args>
class RpcHandle<R(Args...)> {
public:
    using signature = R(Args...);
    using function_type = fl::function<R(Args...)>;

    // Default constructor - creates invalid handle
    RpcHandle() : mValid(false) {}

    // Constructor from function (internal use by RpcFactory)
    explicit RpcHandle(function_type fn) : mFn(fn), mValid(true) {}

    // Call operator - invoke the underlying function
    template<typename... CallArgs>
    R operator()(CallArgs&&... args) const {
        return mFn(fl::forward<CallArgs>(args)...);
    }

    // Validity check
    explicit operator bool() const { return mValid && static_cast<bool>(mFn); }

    // Get underlying function
    function_type get() const { return mFn; }

    // Implicit conversion to fl::function
    operator function_type() const { return mFn; }

private:
    function_type mFn;
    bool mValid;
};

// =============================================================================
// Type tag for signature matching at runtime
// =============================================================================

namespace detail {

// Type tag generator - each signature gets a unique identifier
// Uses function-local static address as unique type tag
template<typename Sig>
struct TypeTag {
    static const void* id() {
        static const char tag = 0;
        return &tag;
    }
};

// Erased invoker interface for JSON transport
class ErasedInvoker {
public:
    virtual ~ErasedInvoker() = default;
    virtual fl::tuple<TypeConversionResult, Json> invoke(const Json& args) = 0;
};

// Typed invoker implementation
template<typename Sig>
class TypedInvoker;

// Specialization for non-void return types
template<typename R, typename... Args>
class TypedInvoker<R(Args...)> : public ErasedInvoker {
public:
    TypedInvoker(fl::function<R(Args...)> fn) : mBinding(fn) {}

    fl::tuple<TypeConversionResult, Json> invoke(const Json& args) override {
        return mBinding.invokeWithReturn(args);
    }

private:
    TypedRpcBinding<R(Args...)> mBinding;
};

// Specialization for void return type
template<typename... Args>
class TypedInvoker<void(Args...)> : public ErasedInvoker {
public:
    TypedInvoker(fl::function<void(Args...)> fn) : mBinding(fn) {}

    fl::tuple<TypeConversionResult, Json> invoke(const Json& args) override {
        TypeConversionResult result = mBinding.invoke(args);
        return fl::make_tuple(result, Json(nullptr));
    }

private:
    TypedRpcBinding<void(Args...)> mBinding;
};

} // namespace detail

// =============================================================================
// RpcFactory - Main typed RPC registry
// =============================================================================

class RpcFactory {
public:
    RpcFactory() = default;
    ~RpcFactory() = default;

    // Non-copyable but movable
    RpcFactory(const RpcFactory&) = delete;
    RpcFactory& operator=(const RpcFactory&) = delete;
    RpcFactory(RpcFactory&&) = default;
    RpcFactory& operator=(RpcFactory&&) = default;

    // =========================================================================
    // Register a typed callable under a name
    // =========================================================================

    // =========================================================================
    // IDEAL API: Auto-deducing registration from lambda/callable
    // Returns RpcHandle for immediate use without re-binding
    // =========================================================================

    template<typename Callable>
    auto method(const char* name, Callable&& fn)
        -> RpcHandle<typename callable_traits<typename decay<Callable>::type>::signature> {
        using Sig = typename callable_traits<typename decay<Callable>::type>::signature;
        RpcFn<Sig> wrapped(fl::forward<Callable>(fn));
        bool ok = method_impl<Sig>(name, wrapped);
        if (ok) {
            return RpcHandle<Sig>(wrapped);
        }
        return RpcHandle<Sig>();  // Invalid handle on failure
    }

    // =========================================================================
    // Legacy API: Explicit signature registration (backwards compatible)
    // =========================================================================

    template<class Sig>
    bool method(const char* name, RpcFn<Sig> fn) {
        return method_impl<Sig>(name, fn);
    }

private:
    // Internal implementation shared by both method() overloads
    template<class Sig>
    bool method_impl(const char* name, RpcFn<Sig> fn) {
        fl::string key(name);

        // Check for duplicate registration with different signature
        auto it = mRegistry.find(key);
        if (it != mRegistry.end()) {
            // Name already registered - check if same signature
            if (it->second.mTypeTag != detail::TypeTag<Sig>::id()) {
                // Different signature - reject
                return false;
            }
            // Same signature - update the binding
        }

        Entry entry;
        entry.mTypeTag = detail::TypeTag<Sig>::id();
        entry.mInvoker = fl::make_shared<detail::TypedInvoker<Sig>>(fn);
        entry.mTypedCallable = fl::make_shared<TypedCallableHolder<Sig>>(fn);

        mRegistry[key] = fl::move(entry);
        return true;
    }

public:

    // =========================================================================
    // Bind returns a typed callable for local use
    // Fails fast (returns empty function) if missing or signature mismatch
    // =========================================================================

    template<class Sig>
    RpcFn<Sig> bind(const char* name) const {
        auto result = try_bind<Sig>(name);
        if (result.has_value()) {
            return result.value();
        }
        // Return empty function on failure
        return RpcFn<Sig>();
    }

    // =========================================================================
    // try_bind returns optional - nullopt if missing or signature mismatch
    // =========================================================================

    template<class Sig>
    fl::optional<RpcFn<Sig>> try_bind(const char* name) const {
        fl::string key(name);

        auto it = mRegistry.find(key);
        if (it == mRegistry.end()) {
            // Name not found
            return fl::nullopt;
        }

        // Check signature match
        if (it->second.mTypeTag != detail::TypeTag<Sig>::id()) {
            // Signature mismatch
            return fl::nullopt;
        }

        // Extract the typed callable
        auto* holder = static_cast<TypedCallableHolder<Sig>*>(it->second.mTypedCallable.get());
        if (!holder) {
            return fl::nullopt;
        }

        return holder->mFn;
    }

    // =========================================================================
    // Check if a method is registered (regardless of signature)
    // =========================================================================

    bool has(const char* name) const {
        return mRegistry.find(fl::string(name)) != mRegistry.end();
    }

    // =========================================================================
    // DIRECT CALL: Skip bind step for one-off calls
    // =========================================================================

    // call<R>() - Direct invocation without binding
    // Returns R (undefined behavior if method not found or signature mismatch)
    template<typename R, typename... Args>
    R call(const char* name, Args&&... args) {
        auto fn = bind<R(Args...)>(name);
        return fn(fl::forward<Args>(args)...);
    }

    // try_call<R>() - Safe direct invocation with optional return
    // Returns nullopt if method not found or signature mismatch
    template<typename R, typename... Args>
    fl::optional<R> try_call(const char* name, Args&&... args) {
        auto opt = try_bind<R(Args...)>(name);
        if (!opt.has_value()) {
            return fl::nullopt;
        }
        return opt.value()(fl::forward<Args>(args)...);
    }

    // =========================================================================
    // JSON transport - dynamic dispatch
    // =========================================================================

    // handle() processes a JSON-RPC request and returns a response
    // Request format: {"method": "name", "params": [...], "id": ...}
    // Response format: {"result": ..., "id": ...} or {"error": {...}, "id": ...}
    Json handle(const Json& request) {
        // Extract method name
        if (!request.contains("method")) {
            return makeError(-32600, "Invalid Request: missing 'method'", request["id"]);
        }

        auto methodOpt = request["method"].as_string();
        if (!methodOpt.has_value()) {
            return makeError(-32600, "Invalid Request: 'method' must be a string", request["id"]);
        }
        fl::string methodName = methodOpt.value();

        // Look up the method
        auto it = mRegistry.find(methodName);
        if (it == mRegistry.end()) {
            return makeError(-32601, "Method not found: " + methodName, request["id"]);
        }

        // Extract params (default to empty array)
        Json params = request.contains("params") ? request["params"] : Json::parse("[]");
        if (!params.is_array()) {
            return makeError(-32602, "Invalid params: must be an array", request["id"]);
        }

        // Invoke the method
        fl::tuple<TypeConversionResult, Json> resultTuple = it->second.mInvoker->invoke(params);
        TypeConversionResult convResult = fl::get<0>(resultTuple);
        Json returnVal = fl::get<1>(resultTuple);

        // Check for conversion errors
        if (!convResult.ok()) {
            return makeError(-32602, "Invalid params: " + convResult.errorMessage(), request["id"]);
        }

        // Build success response
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", returnVal);

        // Include id if present (for request/response correlation)
        if (request.contains("id")) {
            response.set("id", request["id"]);
        }

        // Include warnings if any
        if (convResult.hasWarning()) {
            Json warnings = Json::array();
            for (fl::size i = 0; i < convResult.warnings().size(); ++i) {
                warnings.push_back(Json(convResult.warnings()[i]));
            }
            response.set("warnings", warnings);
        }

        return response;
    }

    // handle_maybe() - for notifications (no id), returns nullopt
    fl::optional<Json> handle_maybe(const Json& request) {
        // If no id, this is a notification - process but don't return response
        if (!request.contains("id")) {
            // Still need to execute the method
            if (request.contains("method")) {
                auto methodOpt = request["method"].as_string();
                if (methodOpt.has_value()) {
                    fl::string methodName = methodOpt.value();
                    auto it = mRegistry.find(methodName);
                    if (it != mRegistry.end()) {
                        Json params = request.contains("params") ? request["params"] : Json::parse("[]");
                        if (params.is_array()) {
                            it->second.mInvoker->invoke(params);
                        }
                    }
                }
            }
            return fl::nullopt;
        }

        return handle(request);
    }

private:
    // Type-erased holder for typed callables
    struct CallableHolderBase {
        virtual ~CallableHolderBase() = default;
    };

    template<typename Sig>
    struct TypedCallableHolder : CallableHolderBase {
        fl::function<Sig> mFn;
        TypedCallableHolder(fl::function<Sig> fn) : mFn(fn) {}
    };

    // Registry entry
    struct Entry {
        const void* mTypeTag = nullptr;
        fl::shared_ptr<detail::ErasedInvoker> mInvoker;
        fl::shared_ptr<CallableHolderBase> mTypedCallable;
    };

    // Helper to create JSON-RPC error response
    static Json makeError(int code, const fl::string& message, const Json& id) {
        Json response = Json::object();
        response.set("jsonrpc", "2.0");

        Json error = Json::object();
        error.set("code", code);
        error.set("message", message);
        response.set("error", error);

        if (id.has_value()) {
            response.set("id", id);
        }

        return response;
    }

    fl::unordered_map<fl::string, Entry> mRegistry;
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON

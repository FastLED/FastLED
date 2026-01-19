#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/tuple.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/detail/rpc/type_conversion_result.h"
#include "fl/detail/rpc/typed_rpc_binding.h"
#include "fl/detail/rpc/type_schema.h"

namespace fl {
namespace detail {

// =============================================================================
// TypeTag - Unique identifier for signatures at runtime
// =============================================================================

template<typename Sig>
struct TypeTag {
    static const void* id() {
        static const char tag = 0;
        return &tag;
    }
};

// =============================================================================
// ErasedInvoker - Type-erased interface for JSON transport invocation
// =============================================================================

class ErasedInvoker {
public:
    virtual ~ErasedInvoker() = default;
    virtual fl::tuple<TypeConversionResult, Json> invoke(const Json& args) = 0;
};

// =============================================================================
// ErasedSchemaGenerator - Type-erased interface for schema generation
// =============================================================================

class ErasedSchemaGenerator {
public:
    virtual ~ErasedSchemaGenerator() = default;
    virtual Json params() const = 0;
    virtual Json result() const = 0;
    virtual bool hasResult() const = 0;
    virtual void setParamNames(const fl::vector<fl::string>& names) = 0;
};

// =============================================================================
// TypedSchemaGenerator - Generates schema for a specific signature
// =============================================================================

template<typename Sig>
class TypedSchemaGenerator : public ErasedSchemaGenerator {
public:
    Json params() const override {
        return MethodSchema<Sig>::paramsWithNames(mParamNames);
    }

    Json result() const override {
        return MethodSchema<Sig>::result();
    }

    bool hasResult() const override {
        return MethodSchema<Sig>::hasResult();
    }

    void setParamNames(const fl::vector<fl::string>& names) override {
        mParamNames = names;
    }

private:
    fl::vector<fl::string> mParamNames;
};

// =============================================================================
// TypedInvoker - Invokes typed functions via JSON transport
// =============================================================================

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
} // namespace fl

#endif // FASTLED_ENABLE_JSON

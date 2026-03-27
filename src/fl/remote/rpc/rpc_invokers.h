#pragma once

#include "fl/stl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/tuple.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/remote/rpc/typed_rpc_binding.h"
#include "fl/remote/rpc/type_schema.h"
#include "fl/stl/noexcept.h"

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
    virtual ~ErasedInvoker() FL_NOEXCEPT = default;
    virtual fl::tuple<TypeConversionResult, json> invoke(const json& args) = 0;
};

// =============================================================================
// ErasedSchemaGenerator - Type-erased interface for schema generation
// =============================================================================

class ErasedSchemaGenerator {
public:
    virtual ~ErasedSchemaGenerator() FL_NOEXCEPT = default;
    virtual void setParamNames(const fl::vector<fl::string>& names) = 0;

    // Flat tuple format: [["name", "type"], ...] optimized for low-memory devices
    virtual json params() const = 0;
    virtual const char* resultTypeName() const = 0;
};

// =============================================================================
// TypedSchemaGenerator - Generates schema for a specific signature
// =============================================================================

template<typename Sig>
class TypedSchemaGenerator : public ErasedSchemaGenerator {
public:
    void setParamNames(const fl::vector<fl::string>& names) override {
        mParamNames = names;
    }

    // Flat tuple format: [["name", "type"], ...] optimized for low-memory devices
    json params() const override {
        return MethodSchema<Sig>::params(mParamNames);
    }

    const char* resultTypeName() const override {
        return MethodSchema<Sig>::resultTypeName();
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

    fl::tuple<TypeConversionResult, json> invoke(const json& args) override {
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

    fl::tuple<TypeConversionResult, json> invoke(const json& args) override {
        TypeConversionResult result = mBinding.invoke(args);
        return fl::make_tuple(result, json(nullptr));
    }

private:
    TypedRpcBinding<void(Args...)> mBinding;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON

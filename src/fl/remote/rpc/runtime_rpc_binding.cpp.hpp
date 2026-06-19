/// @file runtime_rpc_binding.cpp.hpp
/// @brief Non-template RuntimeRpcBinding::invoke body (FastLED #3246 Tier 1A).
///
/// One signature-agnostic decode/encode/dispatch body, shared across every
/// RPC signature on `!FL_PLATFORM_HAS_LARGE_MEMORY` targets. Replaces
/// `TypedRpcBinding<Sig>::invokeWithReturn` (~2.1 KB per signature observed
/// on LPC845-BRK / AutoResearch).

#include "fl/remote/rpc/runtime_rpc_binding.h"

#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {
namespace detail {

namespace {

// RAII cleanup for partially-constructed arg slots. Destroys + frees only
// the slots actually constructed (tracked by `constructed`).
struct RuntimeRpcSlotCleanup {
    const RpcTypeOps* const* arg_types;
    fl::vector<unsigned char*>& slots;
    fl::size constructed;

    RuntimeRpcSlotCleanup(const RpcTypeOps* const* types,
                          fl::vector<unsigned char*>& s) FL_NOEXCEPT
        : arg_types(types), slots(s), constructed(0) {}

    ~RuntimeRpcSlotCleanup() FL_NOEXCEPT {
        for (fl::size i = 0; i < slots.size(); ++i) {
            if (!slots[i]) continue;
            if (i < constructed) arg_types[i]->destroy(slots[i]);
            delete[] slots[i];  // ok bare allocation
        }
    }
};

} // namespace

fl::tuple<TypeConversionResult, json> RuntimeRpcBinding::invoke(const json& jsonArgs) FL_NOEXCEPT {
    TypeConversionResult result;

    if (!jsonArgs.is_array()) {
        result.setError("arguments must be a JSON array");
        return fl::make_tuple(result, json(nullptr));
    }

    fl::size received = jsonArgs.size();
    if (received != mArgCount) {
        result.setError("argument count mismatch: expected " +
                        fl::to_string(static_cast<fl::i64>(mArgCount)) +
                        ", got " + fl::to_string(static_cast<fl::i64>(received)));
        return fl::make_tuple(result, json(nullptr));
    }

    fl::vector<unsigned char*> raw_slots;
    fl::vector<void*> slot_ptrs;
    raw_slots.reserve(mArgCount);
    slot_ptrs.reserve(mArgCount);

    RuntimeRpcSlotCleanup cleanup(mArgTypes, raw_slots);

    for (fl::size i = 0; i < mArgCount; ++i) {
        const RpcTypeOps* ops = mArgTypes[i];
        // `operator new[]` on `unsigned char` returns memory aligned to at
        // least alignof(max_align_t), which is sufficient for every type
        // currently routable through `rpc_storage_type<T>::type`
        // (fl::string, ints, floats, ConstCharPtrWrapper, ConstSpanWrapper).
        // Raw byte buffer for placement-new of `T` -- ops drives lifetime.
        unsigned char* buf = new unsigned char[ops->storage_size];  // ok bare allocation
        raw_slots.push_back(buf);
        slot_ptrs.push_back(buf);
        ops->default_construct(buf);
        cleanup.constructed = i + 1;

        TypeConversionResult convResult = ops->decode(jsonArgs[i], buf);
        for (fl::size w = 0; w < convResult.warnings().size(); ++w) {
            result.addWarning("arg " + fl::to_string(static_cast<fl::i64>(i)) +
                              ": " + convResult.warnings()[w]);
        }
        if (convResult.hasError()) {
            result.setError("arg " + fl::to_string(static_cast<fl::i64>(i)) +
                            ": " + convResult.errorMessage());
            return fl::make_tuple(result, json(nullptr));
        }
    }

    if (mResultType) {
        // Raw byte buffer for placement-new of `R`; same pattern as arg slots.
        unsigned char* result_buf = new unsigned char[mResultType->storage_size];  // ok bare allocation
        mResultType->default_construct(result_buf);
        mTrampoline(mUserFn, result_buf, slot_ptrs.data());
        json jsonResult = mResultType->encode(result_buf);
        mResultType->destroy(result_buf);
        delete[] result_buf;  // ok bare allocation
        return fl::make_tuple(result, jsonResult);
    }

    mTrampoline(mUserFn, nullptr, slot_ptrs.data());
    return fl::make_tuple(result, json(nullptr));
}

} // namespace detail
} // namespace fl

#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/initializer_list.h"
#include "fl/detail/rpc/rpc_handle.h"

namespace fl {

// Forward declaration - Rpc is defined in fl/rpc.h
class Rpc;

namespace detail {

// =============================================================================
// MethodBuilder - Fluent API for setting method metadata
// =============================================================================
// This is an internal detail class used by Rpc::method_with().
// Users get this via the fluent API but shouldn't need to reference it directly.

template<typename Sig>
class MethodBuilder {
public:
    MethodBuilder(Rpc* factory, const char* name, RpcFn<Sig> fn)
        : mFactory(factory), mName(name), mFn(fn) {}

    // Set parameter names (in order)
    MethodBuilder& params(fl::initializer_list<const char*> names) {
        for (const char* n : names) {
            mParamNames.push_back(fl::string(n));
        }
        return *this;
    }

    // Set method description
    MethodBuilder& description(const char* desc) {
        mDescription = desc;
        return *this;
    }

    // Set tags for grouping (OpenRPC tags)
    MethodBuilder& tags(fl::initializer_list<const char*> tagList) {
        for (const char* t : tagList) {
            mTags.push_back(fl::string(t));
        }
        return *this;
    }

    // Finalize and register the method
    // Implementation is in rpc.h after Rpc is fully defined
    RpcHandle<Sig> done();

    // Accessors for done() implementation
    Rpc* factory() const { return mFactory; }
    const fl::string& name() const { return mName; }
    const RpcFn<Sig>& fn() const { return mFn; }
    const fl::vector<fl::string>& paramNames() const { return mParamNames; }
    const fl::string& descriptionStr() const { return mDescription; }
    const fl::vector<fl::string>& tagsVec() const { return mTags; }

private:
    Rpc* mFactory;
    fl::string mName;
    RpcFn<Sig> mFn;
    fl::vector<fl::string> mParamNames;
    fl::string mDescription;
    fl::vector<fl::string> mTags;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON

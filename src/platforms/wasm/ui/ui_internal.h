#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "fl/namespace.h"
#include "fl/callback.h"
#include "fl/json.h"
#include "fl/ptr.h"
#include "fl/str.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_PTR(jsUiInternal);

class jsUiInternal : public fl::Referent {
public:

    using UpdateFunction = fl::Callback<const FLArduinoJson::JsonVariantConst&>;
    using ToJsonFunction = fl::Callback<FLArduinoJson::JsonObject&>;

    jsUiInternal(const fl::Str& name, UpdateFunction updateFunc, ToJsonFunction toJsonFunc);
    ~jsUiInternal() {
        const bool functions_exist = mUpdateFunc || mtoJsonFunc;
        if (functions_exist) {
            clearFunctions();
            printf(
                "Warning: %s: The owner of the jsUiInternal should clear the functions, not this destructor.\n",
                mName.c_str());
        }
    }

    const fl::Str& name() const;
    void update(const FLArduinoJson::JsonVariantConst& json);
    void toJson(FLArduinoJson::JsonObject& json) const;
    int id() const;

    bool clearFunctions();

private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
    fl::Str mName;
    UpdateFunction mUpdateFunc;
    ToJsonFunction mtoJsonFunc;
    mutable std::mutex mMutex;
};

FASTLED_NAMESPACE_END

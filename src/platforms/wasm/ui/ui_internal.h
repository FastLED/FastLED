#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "namespace.h"
#include "callback.h"
#include "json.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(jsUiInternal);

class jsUiInternal : public Referent {
public:

    using UpdateFunction = Callback<const FLArduinoJson::JsonVariantConst&>;
    using ToJsonFunction = Callback<FLArduinoJson::JsonObject&>;

    jsUiInternal(const char* name, UpdateFunction updateFunc, ToJsonFunction toJsonFunc);
    ~jsUiInternal() {
        const bool functions_exist = mUpdateFunc || mtoJsonFunc;
        if (functions_exist) {
            clearFunctions();
            printf("Warning: %s: The owner of the jsUiInternal should clear the functions, not this destructor.\n", mName);
        }
    }

    const char* name() const;
    void update(const FLArduinoJson::JsonVariantConst& json);
    void toJson(FLArduinoJson::JsonObject& json) const;
    int id() const;

    bool clearFunctions();

private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
    const char* mName;
    UpdateFunction mUpdateFunc;
    ToJsonFunction mtoJsonFunc;
    mutable std::mutex mMutex;
};

FASTLED_NAMESPACE_END

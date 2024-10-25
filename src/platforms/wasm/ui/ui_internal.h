#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "namespace.h"
#include "callback.h"
#include "json.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(jsUiInternal);

class jsUiInternal : public Referent {
public:

    using UpdateFunction = Callback<const ArduinoJson::JsonVariantConst&>;
    using ToJsonFunction = Callback<ArduinoJson::JsonObject&>;

    jsUiInternal(const char* name, UpdateFunction updateFunc, ToJsonFunction toJsonFunc);
    ~jsUiInternal() {
        const bool functions_exist = mUpdateFunc || mtoJsonFunc;
        if (functions_exist) {
            clearFunctions();
            printf("Warning: %s: The owner of the jsUiInternal should clear the functions, not this destructor.\n", mName);
        }
    }

    const char* name() const;
    void update(const ArduinoJson::JsonVariantConst& json);
    void toJson(ArduinoJson::JsonObject& json) const;
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

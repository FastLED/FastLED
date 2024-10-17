#pragma once

#include <atomic>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "namespace.h"
#include "third_party/arduinojson/json.h"

FASTLED_NAMESPACE_BEGIN

template<typename T, typename ...Args>
struct Callback {
    void* self = nullptr;
    void (*callback)(void* self, T arg, Args... args) = nullptr;
    operator bool() const { return callback != nullptr; }
    void operator()(T arg, Args... args) const { if (callback) callback(self, arg, args...); }
};

class jsUiInternal : public std::enable_shared_from_this<jsUiInternal> {
public:

    using UpdateFunction = Callback<const char*>;
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
    void update(const char* jsonStr);
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

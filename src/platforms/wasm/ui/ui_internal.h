#pragma once

#include <atomic>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class jsUiInternal : public std::enable_shared_from_this<jsUiInternal> {
public:
    using UpdateFunction = std::function<void(const char*)>; // jsonStr
    using ToJsonStrFunction = std::function<std::string()>;  // returns jsonStr of element

    jsUiInternal(const char* name, UpdateFunction updateFunc, ToJsonStrFunction toJsonStrFunc);
    ~jsUiInternal() {
        const bool functions_exist = mUpdateFunc || mToJsonStrFunc;
        if (functions_exist) {
            clearFunctions();
            printf("Warning: %s: The owner of the jsUiInternal should clear the functions, not this destructor.\n", mName);
        }
    }

    const char* name() const;
    void update(const char* jsonStr);
    std::string toJsonStr() const;
    int id() const;

    bool clearFunctions();

private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
    const char* mName;
    UpdateFunction mUpdateFunc;
    ToJsonStrFunction mToJsonStrFunc;
    mutable std::mutex mMutex;
};

FASTLED_NAMESPACE_END

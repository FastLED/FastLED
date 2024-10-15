#pragma once

#include <atomic>
#include <string>
#include <functional>
#include <memory>
#include <mutex>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class jsUiInternal : public std::enable_shared_from_this<jsUiInternal> {
public:
    using UpdateFunction = std::function<void(const char*)>;

    jsUiInternal(const std::string& name, const std::string& type, UpdateFunction updateFunc);
    ~jsUiInternal() { clearUpdateFunction(); }

    std::string type() const;
    std::string name() const;
    void update(const char* jsonStr);
    std::string toJsonStr() const;
    int id() const;

    void clearUpdateFunction();

private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
    std::string mName;
    std::string mType;
    UpdateFunction mUpdateFunc;
    std::mutex mMutex;
};

FASTLED_NAMESPACE_END

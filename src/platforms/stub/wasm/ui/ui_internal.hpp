#pragma once

#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

inline jsUiInternal::jsUiInternal(const std::string& name, const std::string& type, UpdateFunction updateFunc)
    : mName(name), mType(type), mUpdateFunc(std::move(updateFunc)), mId(nextId()) {}

inline std::string jsUiInternal::type() const { return mType; }
inline std::string jsUiInternal::name() const { return mName; }
inline void jsUiInternal::update(const char* jsonStr) { 
    std::lock_guard<std::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(jsonStr);
    }
}
inline std::string jsUiInternal::toJsonStr() const { return "{}"; } // Default implementation
inline int jsUiInternal::id() const { return mId; }

inline void jsUiInternal::clearUpdateFunction() {
    std::lock_guard<std::mutex> lock(mMutex);
    mUpdateFunc = nullptr;
}

inline int jsUiInternal::nextId() {
    return sNextId.fetch_add(1, std::memory_order_relaxed);
}

std::atomic<uint32_t> jsUiInternal::sNextId(0);

FASTLED_NAMESPACE_END

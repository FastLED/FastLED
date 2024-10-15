#pragma once

#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

inline jsUiInternal::jsUiInternal(const char* name, UpdateFunction updateFunc, ToJsonStrFunction toJsonStrFunc)
    : mName(name), mUpdateFunc(std::move(updateFunc)), mToJsonStrFunc(std::move(toJsonStrFunc)), mId(nextId()), mMutex() {}

inline std::string jsUiInternal::name() const { return mName; }
inline void jsUiInternal::update(const char* jsonStr) { 
    std::lock_guard<std::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(jsonStr);
    }
}
inline std::string jsUiInternal::toJsonStr() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mToJsonStrFunc();
}
inline int jsUiInternal::id() const { return mId; }

inline bool jsUiInternal::clearFunctions() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool wasCleared = (mUpdateFunc != nullptr) || (mToJsonStrFunc != nullptr);
    mUpdateFunc = nullptr;
    mToJsonStrFunc = nullptr;
    return wasCleared;
}

inline int jsUiInternal::nextId() {
    return sNextId.fetch_add(1, std::memory_order_seq_cst);
}

std::atomic<uint32_t> jsUiInternal::sNextId(0);

FASTLED_NAMESPACE_END

#pragma once

#include "ui_internal.h"

FASTLED_NAMESPACE_BEGIN

inline jsUiInternal::jsUiInternal(const char* name, UpdateFunction updateFunc, toJsonFunction toJsonFunc)
    : mName(name), mUpdateFunc(std::move(updateFunc)), mtoJsonFunc(std::move(toJsonFunc)), mId(nextId()), mMutex() {}

inline const char* jsUiInternal::name() const { return mName; }
inline void jsUiInternal::update(const char* jsonStr) { 
    std::lock_guard<std::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(jsonStr);
    }
}
inline void jsUiInternal::toJson(ArduinoJson::JsonObject& json) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mtoJsonFunc(json);
}
inline int jsUiInternal::id() const { return mId; }

inline bool jsUiInternal::clearFunctions() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool wasCleared = (mUpdateFunc != nullptr) || (mtoJsonFunc != nullptr);
    mUpdateFunc = nullptr;
    mtoJsonFunc = nullptr;
    return wasCleared;
}

inline int jsUiInternal::nextId() {
    return sNextId.fetch_add(1, std::memory_order_seq_cst);
}

std::atomic<uint32_t> jsUiInternal::sNextId(0);

FASTLED_NAMESPACE_END

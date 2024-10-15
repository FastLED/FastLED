#pragma once

#include "ui.h"
#include "ui_manager.hpp"

FASTLED_NAMESPACE_BEGIN

inline jsUI::jsUI(std::weak_ptr<jsUiInternal> internal) 
    : mInternal(std::move(internal))
{
}

inline jsUI::~jsUI() {
    jsUiManager::removeComponent(mInternal);
}

inline std::string jsUI::type() const {
    if (auto internal = mInternal.lock()) {
        return internal->type();
    }
    return "";
}

inline std::string jsUI::name() const {
    if (auto internal = mInternal.lock()) {
        return internal->name();
    }
    return "";
}

inline void jsUI::update(const char* jsonStr) {
    if (auto internal = mInternal.lock()) {
        internal->update(jsonStr);
    }
}

inline std::string jsUI::toJsonStr() const {
    if (auto internal = mInternal.lock()) {
        return internal->toJsonStr();
    }
    return "{}";
}

inline int jsUI::id() const {
    if (auto internal = mInternal.lock()) {
        return internal->id();
    }
    return -1;
}

inline void jsUI::releaseInternal() {
    std::lock_guard<std::mutex> lock(mMutex);
    mInternal.reset();
}

inline std::shared_ptr<jsUiInternal> jsUI::getInternal() {
    return mInternal.lock();
}

FASTLED_NAMESPACE_END

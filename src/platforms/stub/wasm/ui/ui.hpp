#pragma once

#include "ui.h"
#include "ui_manager.hpp"

FASTLED_NAMESPACE_BEGIN

inline jsUI::jsUI() : mId(nextId()) {}

inline jsUI::~jsUI() {
    jsUiManager::removeComponent(jsUIPtr::TakeOwnership(this));
}

inline int jsUI::id() const {
    return mId;
}

inline int jsUI::nextId() {
    static std::atomic<uint32_t> sNextId(0);
    return sNextId++;
}

FASTLED_NAMESPACE_END

#include <stdio.h>

#include "fl/atomic.h"
#include "fl/compiler_control.h"
#include "fl/mutex.h"
#include "fl/warn.h"
#include "ui_internal.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON


namespace fl {

// Constructor: Initializes the base JsonUiInternal with name.
JsonUiInternal::JsonUiInternal(const fl::string &name)
    : mName(name), mId(nextId()), mGroup(), mMutex(), mHasChanged(false) {}

JsonUiInternal::~JsonUiInternal() {}

const fl::string &JsonUiInternal::name() const { return mName; }

void JsonUiInternal::updateInternal(const fl::Json &json) {
    // Default no-op implementation
}

void JsonUiInternal::toJson(fl::Json &json) const {
    // Default no-op implementation
}

int JsonUiInternal::id() const { return mId; }

void JsonUiInternal::setGroup(const fl::string &groupName) { 
    fl::lock_guard<fl::mutex> lock(mMutex);
    mGroup = groupName; 
}

const fl::string &JsonUiInternal::groupName() const { 
    fl::lock_guard<fl::mutex> lock(mMutex);
    return mGroup; 
}

bool JsonUiInternal::hasChanged() const {
    fl::lock_guard<fl::mutex> lock(mMutex);
    return mHasChanged;
}

void JsonUiInternal::markChanged() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mHasChanged = true;
}

void JsonUiInternal::clearChanged() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mHasChanged = false;
}

int JsonUiInternal::nextId() {
    static fl::atomic<uint32_t> sNextId(0);
    return sNextId.fetch_add(1);
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON

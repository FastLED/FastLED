#include <stdio.h>

#include "fl/atomic.h"
#include "fl/compiler_control.h"
#include "fl/mutex.h"
#include "fl/warn.h"
#include "ui_internal.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

namespace fl {

const fl::string &JsonUiInternal::name() const { return mName; }

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

// IWYU pragma: private

#include "fl/stl/stdio.h"
#include "fl/stl/atomic.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/mutex.h"
#include "fl/log/log.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/stl/json.h"
#include "fl/stl/noexcept.h"
namespace fl {

const fl::string &JsonUiInternal::name() const FL_NOEXCEPT { return mName; }

int JsonUiInternal::id() const FL_NOEXCEPT { return mId; }

void JsonUiInternal::setGroup(const fl::string &groupName) FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    mGroup = groupName;
}

fl::string JsonUiInternal::groupName() const FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    return mGroup;
}

bool JsonUiInternal::hasChanged() const FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    return mHasChanged;
}

void JsonUiInternal::markChanged() FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    mHasChanged = true;
}

void JsonUiInternal::clearChanged() FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(mMutex);
    mHasChanged = false;
}

int JsonUiInternal::nextId() FL_NOEXCEPT {
    static fl::atomic<u32> sNextId(0);
    return sNextId.fetch_add(1);
}

} // namespace fl

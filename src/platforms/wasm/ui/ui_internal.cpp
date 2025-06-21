#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include <memory>
#include <stdio.h>

#include "fl/atomic.h"
#include "fl/mutex.h"
#include "ui_internal.h"
#include "fl/compiler_control.h"
#include "fl/warn.h"

using namespace fl;

namespace fl {


jsUiInternal::jsUiInternal(const Str &name, UpdateFunction updateFunc,
                           ToJsonFunction toJsonFunc, const fl::string &group)
    : mName(name), mGroup(group), mUpdateFunc(updateFunc),
      mtoJsonFunc(toJsonFunc), mId(nextId()), mMutex() {}


jsUiInternal::~jsUiInternal() {
    const bool functions_exist = mUpdateFunc || mtoJsonFunc;
    if (functions_exist) {
        clearFunctions();
        FL_WARN("Warning: " << mName << ": The owner of the jsUiInternal should clear "
               "the functions, not this destructor.");
    }
}

const Str &jsUiInternal::name() const { return mName; }
void jsUiInternal::update(const FLArduinoJson::JsonVariantConst &json) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(json);
    }
}
void jsUiInternal::toJson(FLArduinoJson::JsonObject &json) const {
    fl::lock_guard<fl::mutex> lock(mMutex);
    if (mtoJsonFunc) {
        mtoJsonFunc(json);
    }
}
int jsUiInternal::id() const { return mId; }

const fl::string &jsUiInternal::group() const { return mGroup; }

void jsUiInternal::setGroup(const fl::string &group) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mGroup = group;
}

bool jsUiInternal::clearFunctions() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    bool wasCleared = !mUpdateFunc || !mtoJsonFunc;
    mUpdateFunc =
        UpdateFunction([](const FLArduinoJson::JsonVariantConst &) {});
    mtoJsonFunc = ToJsonFunction([](FLArduinoJson::JsonObject &) {});
    return wasCleared;
}

int jsUiInternal::nextId() {
    static fl::atomic<uint32_t> sNextId(0);
    return sNextId.fetch_add(1);
}


} // namespace fl

#endif // __EMSCRIPTEN__

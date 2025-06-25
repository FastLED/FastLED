#include <stdio.h>

#include "fl/atomic.h"
#include "fl/compiler_control.h"
#include "fl/mutex.h"
#include "fl/warn.h"
#include "ui_internal.h"

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

using namespace fl;

namespace fl {

JsonUiInternal::JsonUiInternal(const string &name, UpdateFunction updateFunc,
                           ToJsonFunction toJsonFunc)
    : mName(name), mUpdateFunc(updateFunc), mtoJsonFunc(toJsonFunc),
      mId(nextId()), mGroup(), mMutex(), mHasChanged(false) {}

JsonUiInternal::~JsonUiInternal() {
    const bool functions_exist = mUpdateFunc || mtoJsonFunc;
    if (functions_exist) {
        clearFunctions();
        // printf("Warning: %s: The owner of the JsonUiInternal should clear "
        //        "the functions, not this destructor.\n",
        //        mName.c_str());
        FL_WARN("Warning: " << mName << ": The owner of the JsonUiInternal should clear "
               "the functions, not this destructor.");
    }
}

const string &JsonUiInternal::name() const { return mName; }
void JsonUiInternal::update(const FLArduinoJson::JsonVariantConst &json) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(json);
    }
}
void JsonUiInternal::toJson(FLArduinoJson::JsonObject &json) const {
    fl::lock_guard<fl::mutex> lock(mMutex);
    if (mtoJsonFunc) {
        mtoJsonFunc(json);
    }
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

bool JsonUiInternal::clearFunctions() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    bool wasCleared = !mUpdateFunc || !mtoJsonFunc;
    mUpdateFunc =
        UpdateFunction([](const FLArduinoJson::JsonVariantConst &) {});
    mtoJsonFunc = ToJsonFunction([](FLArduinoJson::JsonObject &) {});
    return wasCleared;
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

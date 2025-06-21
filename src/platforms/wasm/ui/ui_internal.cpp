#ifdef __EMSCRIPTEN__

#include "fl/namespace.h"

#include "ui_internal.h"

using namespace fl;

namespace fl {

jsUiInternal::jsUiInternal(const Str &name, UpdateFunction updateFunc,
                           ToJsonFunction toJsonFunc, const fl::string &group)
    : mName(name), mGroup(group), mUpdateFunc(updateFunc), mtoJsonFunc(toJsonFunc),
      mId(nextId()), mMutex() {}

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
    return sNextId.fetch_add(1, std::memory_order_seq_cst);
}

std::atomic<uint32_t> jsUiInternal::sNextId(0);

} // namespace fl

#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__

#include "fl/namespace.h"

#include "ui_internal.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN

 jsUiInternal::jsUiInternal(const Str& name, UpdateFunction updateFunc, ToJsonFunction toJsonFunc)
    : mName(name), mUpdateFunc(updateFunc), mtoJsonFunc(toJsonFunc), mId(nextId()), mMutex() {}

 const Str& jsUiInternal::name() const { return mName; }
 void jsUiInternal::update(const FLArduinoJson::JsonVariantConst& json) { 
    std::lock_guard<std::mutex> lock(mMutex);
    if (mUpdateFunc) {
        mUpdateFunc(json);
    }
}
 void jsUiInternal::toJson(FLArduinoJson::JsonObject& json) const {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mtoJsonFunc) {
        mtoJsonFunc(json);
    }
}
 int jsUiInternal::id() const { return mId; }

 bool jsUiInternal::clearFunctions() {
    std::lock_guard<std::mutex> lock(mMutex);
    bool wasCleared = !mUpdateFunc || !mtoJsonFunc;
    mUpdateFunc.clear();
    mtoJsonFunc.clear();
    return wasCleared;
}

 int jsUiInternal::nextId() {
    return sNextId.fetch_add(1, std::memory_order_seq_cst);
}

std::atomic<uint32_t> jsUiInternal::sNextId(0);

FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__


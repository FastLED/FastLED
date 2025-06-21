#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string>

#include "fl/function.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/str.h"
#include "fl/mutex.h"

namespace fl {

FASTLED_SMART_PTR(jsUiInternal);

class jsUiInternal : public fl::Referent {
  public:
    using UpdateFunction =
        fl::function<void(const FLArduinoJson::JsonVariantConst &)>;
    using ToJsonFunction = fl::function<void(FLArduinoJson::JsonObject &)>;

    jsUiInternal(const fl::string &name, UpdateFunction updateFunc,
                 ToJsonFunction toJsonFunc, const fl::string &group = "");
    ~jsUiInternal() {
        const bool functions_exist = mUpdateFunc || mtoJsonFunc;
        if (functions_exist) {
            clearFunctions();
            printf("Warning: %s: The owner of the jsUiInternal should clear "
                   "the functions, not this destructor.\n",
                   mName.c_str());
        }
    }

    const fl::string &name() const;
    void update(const FLArduinoJson::JsonVariantConst &json);
    void toJson(FLArduinoJson::JsonObject &json) const;
    int id() const;

    const fl::string &group() const;
    void setGroup(const fl::string &group);

    bool clearFunctions();

  private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
    fl::string mName;
    fl::string mGroup;
    UpdateFunction mUpdateFunc;
    ToJsonFunction mtoJsonFunc;
    mutable fl::mutex mMutex;
};

} // namespace fl

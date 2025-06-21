#pragma once



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
                 ToJsonFunction toJsonFunc);
    ~jsUiInternal();

    const fl::string &name() const;
    void update(const FLArduinoJson::JsonVariantConst &json);
    void toJson(FLArduinoJson::JsonObject &json) const;
    int id() const;

    bool clearFunctions();

  private:
    static int nextId();
    fl::string mName;
    UpdateFunction mUpdateFunc;
    ToJsonFunction mtoJsonFunc;
    int mId;
    mutable fl::mutex mMutex;
};

} // namespace fl

#pragma once



#include "fl/function.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/str.h"
#include "fl/mutex.h"


namespace fl {

FASTLED_SMART_PTR(JsonUiInternal);

class JsonUiInternal : public fl::Referent {
  public:
    using UpdateFunction =
        fl::function<void(const FLArduinoJson::JsonVariantConst &)>;
    using ToJsonFunction = fl::function<void(FLArduinoJson::JsonObject &)>;

    JsonUiInternal(const fl::string &name, UpdateFunction updateFunc,
                 ToJsonFunction toJsonFunc);
    ~JsonUiInternal();

    const fl::string &name() const;
    void update(const FLArduinoJson::JsonVariantConst &json);
    void toJson(FLArduinoJson::JsonObject &json) const;
    int id() const;

    // Group functionality
    void setGroup(const fl::string &groupName);
    const fl::string &groupName() const;

    // Change tracking for polling (eliminates need for manual notifications)
    bool hasChanged() const;
    void markChanged();
    void clearChanged();

    bool clearFunctions();

  private:
    static int nextId();
    fl::string mName;
    UpdateFunction mUpdateFunc;
    ToJsonFunction mtoJsonFunc;
    int mId;
    fl::string mGroup;
    mutable fl::mutex mMutex;
    mutable bool mHasChanged = false; // Track if component has changed since last poll
};

} // namespace fl

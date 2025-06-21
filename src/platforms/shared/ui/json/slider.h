#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"


namespace fl {

class JsonSliderImpl {
  public:
    JsonSliderImpl(const fl::string &name, float value = 128.0f, float min = 0.0f,
                 float max = 255.0f, float step = -1.f);
    ~JsonSliderImpl();
    JsonSliderImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    float value() const;
    float value_normalized() const;
    float getMax() const { return mMax; }
    float getMin() const { return mMin; }
    void setValue(float value);
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const { return static_cast<int>(mValue); }

    JsonSliderImpl &operator=(float value) {
        setValue(value);
        return *this;
    }
    JsonSliderImpl &operator=(int value) {
        setValue(static_cast<float>(value));
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    JsonUiInternalPtr mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

} // namespace fl

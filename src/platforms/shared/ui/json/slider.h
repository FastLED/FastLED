#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"


namespace fl {

class JsonSliderImpl {
  public:
    JsonSliderImpl(const fl::string &name, float value, float min,
                 float max, float step);
    ~JsonSliderImpl();
    JsonSliderImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    float value() const;
    float value_normalized() const;
    float getMax() const;
    float getMin() const;
    void setValue(float value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const;

    JsonSliderImpl &operator=(float value);
    JsonSliderImpl &operator=(int value);

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);
    void setValueInternal(float value);  // Internal method for UI updates - no change notification

    JsonUiInternalPtr mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
};

} // namespace fl

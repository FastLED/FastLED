#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/math_macros.h"
#include "fl/json.h"



namespace fl {

class JsonNumberFieldImpl {
  public:
    JsonNumberFieldImpl(const fl::string &name, double value, double min, double max);
    ~JsonNumberFieldImpl();
    JsonNumberFieldImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    double value() const;
    void setValue(double value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    JsonNumberFieldImpl &operator=(double value);
    JsonNumberFieldImpl &operator=(int value);
    // Use ALMOST_EQUAL_FLOAT for floating-point comparison
    bool operator==(double v) const;
    bool operator==(int v) const;
    bool operator!=(double v) const;
    bool operator!=(int v) const;

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);
    void setValueInternal(double value);  // Internal method for UI updates - no change notification

    JsonUiInternalPtr mInternal;
    double mValue;
    double mMin;
    double mMax;
};

} // namespace fl

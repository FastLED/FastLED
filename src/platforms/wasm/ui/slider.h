#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsSliderImpl {
  public:
    jsSliderImpl(const fl::string &name, double *value, double min, double max);
    ~jsSliderImpl();
    jsSliderImpl &Group(const fl::string &name) {
        mInternal->setGroup(name);
        return *this;
    }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    double value() const;
    void setValue(double value);
    double min() const;
    double max() const;
    const fl::string &groupName() const { return mInternal->groupName(); }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::string &groupName) { mInternal->setGroup(groupName); }

    jsSliderImpl &operator=(double value) {
        setValue(value);
        return *this;
    }
    jsSliderImpl &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    double *mValue;
    double mMin;
    double mMax;
};

} // namespace fl

#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsSliderImpl {
  public:
    jsSliderImpl(const fl::Str &name, float value = 128.0f, float min = 0.0f,
                 float max = 255.0f, float step = -1.f);
    ~jsSliderImpl();
    jsSliderImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    }

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    float value() const;
    float value_normalized() const;
    float getMax() const { return mMax; }
    float getMin() const { return mMin; }
    void setValue(float value);
    const fl::Str &groupName() const { return mGroup; }

    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const { return static_cast<int>(mValue); }

    jsSliderImpl &operator=(float value) {
        setValue(value);
        return *this;
    }
    jsSliderImpl &operator=(int value) {
        setValue(static_cast<float>(value));
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    fl::Str mGroup;
};

} // namespace fl

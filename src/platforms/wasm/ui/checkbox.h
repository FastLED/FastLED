#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsCheckboxImpl {
  public:
    jsCheckboxImpl(const fl::Str &, bool value);
    ~jsCheckboxImpl();
    jsCheckboxImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    };

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    bool value() const;
    void setValue(bool value);
    const fl::Str &groupName() const { return mGroup; }

    jsCheckboxImpl &operator=(bool value) {
        setValue(value);
        return *this;
    }
    jsCheckboxImpl &operator=(int value) {
        setValue(value != 0);
        return *this;
    }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    bool mValue;
    fl::Str mGroup;
};

} // namespace fl

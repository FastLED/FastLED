#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsDescriptionImpl {
  public:
    jsDescriptionImpl(const fl::Str &text);
    ~jsDescriptionImpl();
    jsDescriptionImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    }

    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::Str &groupName() const { return mGroup; }
    const fl::Str &text() const { return mText; }

  private:
    jsUiInternalPtr mInternal;
    fl::Str mGroup;
    fl::Str mText;
};

} // namespace fl

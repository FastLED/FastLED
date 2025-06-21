#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsDescriptionImpl {
  public:
    jsDescriptionImpl(const fl::string &name);
    ~jsDescriptionImpl();

    void setGroupIternal(const fl::string& name);

    const fl::string &text() const { return mText; }

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->group(); }

  private:
    jsUiInternalPtr mInternal;
    fl::string mText;
};

} // namespace fl

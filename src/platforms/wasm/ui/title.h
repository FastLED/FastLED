#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsTitleImpl {
  public:
    jsTitleImpl(const fl::string &text);
    ~jsTitleImpl();


    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    const fl::string &groupName() const { return mInternal->group(); }
    const fl::string &text() const { return mText; }

  private:
    jsUiInternalPtr mInternal;
    fl::string mText;
};

} // namespace fl

#pragma once

#include <string>

#include "fl/singleton.h"
#include "platforms/wasm/engine_listener.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"

#include "fl/json.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/ui/ui_manager.h"

namespace fl {


class JsonUiManager : public UiManager {
  public:
    // Called from the JS engine.
    static void jsUpdateUiComponents(const std::string &jsonStr) {
        instance().updateUiComponents(jsonStr.c_str());
    }
    static JsonUiManager &instance();

  private:
    friend class fl::Singleton<JsonUiManager>;
    JsonUiManager(): UiManager(fl::updateJs) {}
    ~JsonUiManager() = default;
};

} // namespace fl

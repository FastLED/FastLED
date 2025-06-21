#pragma once

#include <string>

#include "fl/singleton.h"
#include "platforms/wasm/engine_listener.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"

#include "fl/json.h"
#include "platforms/wasm/js.h"
#include "platforms/shared/ui/json/ui_manager.h"

namespace fl {


class JsonUiManager : public UiManager {
  public:
    // Called from the JS engine.
    static void jsUpdateUiComponents(const std::string &jsonStr);
    static JsonUiManager &instance();

  private:
    friend class fl::Singleton<JsonUiManager>;
    JsonUiManager();
    ~JsonUiManager() = default;
};

} // namespace fl

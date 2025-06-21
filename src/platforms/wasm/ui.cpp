#if defined(__EMSCRIPTEN__)


#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "platforms/wasm/ui.h"


namespace fl {

JsonUiManager &JsonUiManager::instance() {
    return fl::Singleton<JsonUiManager>::instance();
}



EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &JsonUiManager::jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__

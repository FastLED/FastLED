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

jsUiManager &jsUiManager::instance() {
    return fl::Singleton<jsUiManager>::instance();
}



EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUiManager::jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__

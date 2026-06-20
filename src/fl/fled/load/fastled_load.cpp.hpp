// ok no header - CFastLED method body; declaration lives in src/FastLED.h.
/// @file fastled_load.cpp.hpp
/// @brief CFastLED::load(const fl::Fled&) - top-level bundle-load entry
/// point for the .fled subsystem (#3311 PR5). Composes loadChannels +
/// loadScreenMap and, if the bundle carries a script section, dispatches
/// to the back-end installed via FastLED.enableWasm() or .enableMicroPy()
/// (those setters land in PR6).
///
/// Lives in its own TU so the linker can drop load() when the sketch
/// never calls it (linker tree-shake via --gc-sections). Pulling load()
/// transitively pulls loadChannels + loadScreenMap + the FledDispatcher
/// singleton.

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/fled/fled.h"
#include "fl/fled/fled_dispatch.h"
#include "fl/fled/fled_script.h"
#include "fl/log/log.h"
#include "fl/stl/shared_ptr.h"

void CFastLED::load(const fl::Fled& fled) {
    if (!fled) {
        return;
    }
    loadChannels(fled);
    loadScreenMap(fled);

    fl::shared_ptr<fl::FledScript> script = fled.script();
    if (!script) {
        return;
    }
    fl::detail::FledDispatcher& d = fl::detail::fledDispatcher();
    if (script->kind == fl::ScriptKind::Wasm) {
        if (d.wasmLoader) {
            d.wasmLoader(fled);
        } else {
            FL_ERROR_F("Wasm script detected in .fled bundle but the "
                     "wasm runtime is not linked. Call FastLED.enableWasm() "
                     "in setup() to enable it.");
        }
    } else if (script->kind == fl::ScriptKind::MicroPy) {
        if (d.microPyLoader) {
            d.microPyLoader(fled);
        } else {
            FL_ERROR_F("MicroPython script detected in .fled bundle but the "
                     "VM is not linked. Call FastLED.enableMicroPy() in "
                     "setup() to enable it.");
        }
    }
}

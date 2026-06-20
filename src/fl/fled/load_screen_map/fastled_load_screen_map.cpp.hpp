// ok no header - CFastLED method body; declaration lives in src/FastLED.h.
/// @file fastled_load_screen_map.cpp.hpp
/// @brief CFastLED::loadScreenMap(const fl::Fled&) - bundle-load helper
/// for the .fled subsystem (#3311 PR5). PR5 ships this as an API stub:
/// the issue spec calls for a broadcast via fl::EngineEvents::onCanvasUiSet,
/// but that broadcast takes a specific CLEDController* and the
/// CFastLED::loadScreenMap entry-point has no single controller to bind
/// to. The bind-target policy (all controllers, first/default, or a new
/// no-controller EngineEvents API) lands in a follow-up. The TU is laid
/// down now so callers see a stable surface.
///
/// Lives in its own TU so the linker can drop loadScreenMap when the
/// sketch never calls it (linker tree-shake via --gc-sections).

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/fled/fled.h"
#include "fl/log/log.h"
#include "fl/math/screenmap.h"
#include "fl/stl/shared_ptr.h"

void CFastLED::loadScreenMap(const fl::Fled& fled) {
    fl::shared_ptr<fl::ScreenMap> sm = fled.screenMap();
    if (!sm) {
        return;
    }
    // TODO(#3311): policy decision pending - bind to all registered
    // CLEDControllers, or to the first/default one, or via a new
    // EngineEvents API that doesn't require a specific controller.
    // PR5 ships the API surface; binding semantics land in a follow-up.
    FL_WARN_F("CFastLED::loadScreenMap is a stub - see #3311 PR5 follow-up.");
}

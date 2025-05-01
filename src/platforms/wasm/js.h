#pragma once


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers

#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string>



#include "fl/str.h"
#include "ui/ui_internal.h"
#include "fl/str.h"
#include "fl/engine_events.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/ptr.h"
#include "platforms/wasm/ui/button.h"
#include "platforms/wasm/ui/checkbox.h"
#include "platforms/wasm/ui/slider.h"
#include "platforms/wasm/ui/title.h"
#include "platforms/wasm/ui/number_field.h"
#include "platforms/wasm/ui/description.h"
#include "platforms/wasm/ui/audio.h"
#include "platforms/wasm/active_strip_data.h"

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t millis();

// Replacement for 'micros' in WebAssembly context
EMSCRIPTEN_KEEPALIVE uint32_t micros();

// Replacement for 'delay' in WebAssembly context
EMSCRIPTEN_KEEPALIVE void delay(int ms);
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup();
EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop();


namespace fl {

void jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap& screenmap);
void jsOnFrame(ActiveStripData& active_strips);
void jsOnStripAdded(uintptr_t strip, uint32_t num_leds);
void updateJs(const char* jsonStr);

#define FASTLED_HAS_UI_BUTTON 1
#define FASTLED_HAS_UI_SLIDER 1
#define FASTLED_HAS_UI_CHECKBOX 1
#define FASTLED_HAS_UI_NUMBER_FIELD 1
#define FASTLED_HAS_UI_TITLE 1
#define FASTLED_HAS_UI_DESCRIPTION 1
#define FASTLED_HAS_UI_AUDIO 1

typedef jsNumberFieldImpl UINumberFieldImpl;
typedef jsSliderImpl UISliderImpl;
typedef jsCheckboxImpl UICheckboxImpl;
typedef jsButtonImpl UIButtonImpl;
typedef jsTitleImpl UITitleImpl;
typedef jsDescriptionImpl UIDescriptionImpl;
typedef jsAudioImpl UIAudioImpl;

}  // namespace fl

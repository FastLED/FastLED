#pragma once

#include "fl/stdint.h"

#include "platforms/wasm/js_bindings.h"
#include "platforms/wasm/ui/audio.h"
#include "platforms/wasm/ui/button.h"
#include "platforms/wasm/ui/checkbox.h"
#include "platforms/wasm/ui/description.h"
#include "platforms/wasm/ui/number_field.h"
#include "platforms/wasm/ui/slider.h"
#include "platforms/wasm/ui/title.h"

// Needed or the wasm compiler will strip them out.
// Provide missing functions for WebAssembly build.
extern "C" {

// Replacement for 'millis' in WebAssembly context
uint32_t millis();

// Replacement for 'micros' in WebAssembly context
uint32_t micros();

// Replacement for 'delay' in WebAssembly context
void delay(int ms);
void delayMicroseconds(int micros);
}

//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS
extern "C" int extern_setup();
extern "C" int extern_loop();

namespace fl {

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

} // namespace fl

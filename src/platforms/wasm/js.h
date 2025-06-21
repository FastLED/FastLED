#pragma once

#include "fl/stdint.h"

#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/audio.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/number_field.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/title.h"

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
#define FASTLED_HAS_UI_DROPDOWN 1

typedef JsonNumberFieldImpl UINumberFieldImpl;
typedef JsonSliderImpl UISliderImpl;
typedef JsonCheckboxImpl UICheckboxImpl;
typedef JsonButtonImpl UIButtonImpl;
typedef JsonTitleImpl UITitleImpl;
typedef JsonDescriptionImpl UIDescriptionImpl;
typedef JsonAudioImpl UIAudioImpl;
typedef JsonDropdownImpl UIDropdownImpl;

} // namespace fl

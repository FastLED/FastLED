#pragma once

#ifndef FASTLED_USE_JSON_UI
#ifdef __EMSCRIPTEN__
#define FASTLED_USE_JSON_UI 1
#else
#define FASTLED_USE_JSON_UI 0
#endif  // __EMSCRIPTEN__
#endif  // FASTLED_USE_JSON_UI


#if FASTLED_USE_JSON_UI
#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/audio.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/help.h"
#include "platforms/shared/ui/json/number_field.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/title.h"

namespace fl {

#define FASTLED_HAS_UI_BUTTON 1
#define FASTLED_HAS_UI_SLIDER 1
#define FASTLED_HAS_UI_CHECKBOX 1
#define FASTLED_HAS_UI_NUMBER_FIELD 1
#define FASTLED_HAS_UI_TITLE 1
#define FASTLED_HAS_UI_DESCRIPTION 1
#define FASTLED_HAS_UI_HELP 1
#define FASTLED_HAS_UI_AUDIO 1
#define FASTLED_HAS_UI_DROPDOWN 1

typedef JsonNumberFieldImpl UINumberFieldImpl;
typedef JsonSliderImpl UISliderImpl;
typedef JsonCheckboxImpl UICheckboxImpl;
typedef JsonButtonImpl UIButtonImpl;
typedef JsonTitleImpl UITitleImpl;
typedef JsonDescriptionImpl UIDescriptionImpl;
typedef JsonHelpImpl UIHelpImpl;
typedef JsonAudioImpl UIAudioImpl;
typedef JsonDropdownImpl UIDropdownImpl;

} // namespace fl

#else
// not defined for the system.
#endif // FASTLED_USE_JSON_UI

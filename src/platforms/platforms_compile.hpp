// Hierarchical include file for platforms/ directory
#pragma once

#ifdef FASTLED_ALL_SRC

// PLATFORMS MODULE IMPLEMENTATIONS
#include "platforms/arm/k20/clockless_objectfled.cpp.hpp"
#include "platforms/avr/avr_millis_timer_source.cpp.hpp"
#include "platforms/compile_test.cpp.hpp"
#include "platforms/esp/32/clockless_i2s_esp32s3.cpp.hpp"
#include "platforms/esp/32/i2s/i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/rmt_4/idf4_rmt.cpp.hpp"
#include "platforms/esp/32/rmt_4/idf4_rmt_impl.cpp.hpp"
#include "platforms/esp/32/rmt_5/idf5_rmt.cpp.hpp"
#include "platforms/esp/32/rmt_5/strip_rmt.cpp.hpp"
#include "platforms/esp/32/spi_ws2812/strip_spi.cpp.hpp"
#include "platforms/shared/ui/json/audio.cpp.hpp"
#include "platforms/shared/ui/json/button.cpp.hpp"
#include "platforms/shared/ui/json/checkbox.cpp.hpp"
#include "platforms/shared/ui/json/description.cpp.hpp"
#include "platforms/shared/ui/json/dropdown.cpp.hpp"
#include "platforms/shared/ui/json/help.cpp.hpp"
#include "platforms/shared/ui/json/number_field.cpp.hpp"
#include "platforms/shared/ui/json/slider.cpp.hpp"
#include "platforms/shared/ui/json/title.cpp.hpp"
#include "platforms/shared/ui/json/ui.cpp.hpp"
#include "platforms/shared/ui/json/ui_internal.cpp.hpp"
#include "platforms/shared/ui/json/ui_manager.cpp.hpp"
#include "platforms/stub/led_sysdefs_stub.cpp.hpp"
#include "platforms/wasm/active_strip_data.cpp.hpp"
#include "platforms/wasm/compiler/Arduino.cpp.hpp"
#include "platforms/wasm/engine_listener.cpp.hpp"
#include "platforms/wasm/fastspi_wasm.cpp.hpp"
#include "platforms/wasm/fs_wasm.cpp.hpp"
#include "platforms/wasm/js.cpp.hpp"
#include "platforms/wasm/js_bindings.cpp.hpp"
#include "platforms/wasm/timer.cpp.hpp"
#include "platforms/wasm/ui.cpp.hpp"

#endif // FASTLED_ALL_SRC

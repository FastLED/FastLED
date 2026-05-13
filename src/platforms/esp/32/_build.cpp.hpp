// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)

// begin current directory includes
#include "platforms/esp/32/condition_variable_esp32.cpp.hpp"
#include "platforms/esp/32/init_esp32.cpp.hpp"
#include "platforms/esp/32/io_esp.cpp.hpp"
#include "platforms/esp/32/lwip_hooks.cpp.hpp"
#include "platforms/esp/32/mutex_esp32.cpp.hpp"
#include "platforms/esp/32/platform_time_esp32.cpp.hpp"
#include "platforms/esp/32/semaphore_esp32.cpp.hpp"
#include "platforms/esp/32/watchdog_esp32.cpp.hpp"

// begin sub directory includes
#include "platforms/esp/32/audio/_build.cpp.hpp"
#include "platforms/esp/32/codec/_build.cpp.hpp"
#include "platforms/esp/32/core/_build.cpp.hpp"
#include "platforms/esp/32/drivers/_build.cpp.hpp"
#include "platforms/esp/32/interrupts/_build.cpp.hpp"
#include "platforms/esp/32/ota/_build.cpp.hpp"

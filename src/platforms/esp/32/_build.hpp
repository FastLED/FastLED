/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)
#include "platforms/esp/32/condition_variable_esp32.cpp.hpp"
#include "platforms/esp/32/core/cpu_frequency.cpp.hpp"
#include "platforms/esp/32/init_esp32.cpp.hpp"
#include "platforms/esp/32/io_esp.cpp.hpp"
#include "platforms/esp/32/mutex_esp32.cpp.hpp"
#include "platforms/esp/32/semaphore_esp32.cpp.hpp"
#include "platforms/esp/32/task_coroutine_esp32.cpp.hpp"
#include "platforms/esp/32/watchdog_esp32.cpp.hpp"

// Subdirectory implementations (alphabetical order)
#include "platforms/esp/32/audio/_build.hpp"
#include "platforms/esp/32/core/_build.hpp"
#include "platforms/esp/32/drivers/_build.hpp"
#include "platforms/esp/32/interrupts/_build.hpp"
#include "platforms/esp/32/ota/_build.hpp"

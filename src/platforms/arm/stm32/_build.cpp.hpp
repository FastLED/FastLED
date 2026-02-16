// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\arm\stm32/ directory
/// Includes all implementation files in alphabetical order
///
/// Note: SPI hardware drivers are now in drivers/_build.hpp (included separately in platforms/_build.cpp)

#include "platforms/arm/stm32/init_channel_engine_stm32.cpp.hpp"
#include "platforms/arm/stm32/init_stm32.cpp.hpp"
#include "platforms/arm/stm32/io_stm32.cpp.hpp"
#include "platforms/arm/stm32/mutex_stm32.cpp.hpp"
#include "platforms/arm/stm32/semaphore_stm32.cpp.hpp"
#include "platforms/arm/stm32/stm32_gpio_timer_helpers.cpp.hpp"

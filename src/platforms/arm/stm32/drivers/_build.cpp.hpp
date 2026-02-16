// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\arm\stm32\drivers/ directory
/// Includes all implementation files in alphabetical order

#include "platforms/arm/stm32/is_stm32.h"

#if defined(FL_IS_STM32)

#include "platforms/arm/stm32/stm32_capabilities.h"

// Include SPI implementation files based on platform capabilities
#ifdef FL_STM32_HAS_SPI_HW_2
    #include "platforms/arm/stm32/drivers/spi_hw_2_stm32.cpp.hpp"
#endif

#ifdef FL_STM32_HAS_SPI_HW_4
    #include "platforms/arm/stm32/drivers/spi_hw_4_stm32.cpp.hpp"
#endif

#ifdef FL_STM32_HAS_SPI_HW_8
    #include "platforms/arm/stm32/drivers/spi_hw_8_stm32.cpp.hpp"
#endif

#include "platforms/arm/stm32/drivers/spi_hw_manager_stm32.cpp.hpp"

#endif  // FL_IS_STM32

/// @file _build.hpp
/// @brief Unity build header for platforms\shared/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)
#include "platforms/shared/spi_bus_manager.cpp.hpp"
#include "platforms/shared/spi_hw_1.cpp.hpp"
#include "platforms/shared/spi_hw_16.cpp.hpp"
#include "platforms/shared/spi_hw_2.cpp.hpp"
#include "platforms/shared/spi_hw_4.cpp.hpp"
#include "platforms/shared/spi_hw_8.cpp.hpp"
#include "platforms/shared/spi_transposer.cpp.hpp"
#include "platforms/shared/spi_types.cpp.hpp"
#include "platforms/shared/task_coroutine_null.cpp.hpp"

// Subdirectory implementations (alphabetical order)
#include "platforms/shared/active_strip_data/_build.hpp"
#include "platforms/shared/mock/_build.hpp"
#include "platforms/shared/spi_bitbang/_build.hpp"
#include "platforms/shared/ui/_build.hpp"

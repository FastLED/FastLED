// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428 (binary-size fix for #2420):
///
/// Each LED-output driver subdirectory's `_build.cpp.hpp` self-gates against
/// `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY`. When the macro is enabled (opt-in
/// mode), every non-platform-default driver TU becomes empty -- so only the
/// platform's default driver remains in the build. The auxiliary subdirs
/// (`ble`, `gpio_isr_rx`, `rmt_rx`) are not channel drivers and stay
/// unconditional.
///
/// Unconditional aggregator structure (one include per subdir, alphabetical)
/// is preserved to satisfy the unity-build linter.

#include "fl/stl/compiler_control.h"
FL_NO_UNWIND_BEGIN

// Root directory implementations (alphabetical order)

// begin current directory includes
#include "platforms/esp/32/drivers/channel_manager_esp32.cpp.hpp"
#include "platforms/esp/32/drivers/cled.cpp.hpp"
#include "platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp"

// begin sub directory includes
#include "platforms/esp/32/drivers/ble/_build.cpp.hpp"
#include "platforms/esp/32/drivers/gpio_isr_rx/_build.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/_build.cpp.hpp"
#include "platforms/esp/32/drivers/i2s_spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/_build.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt_rx/_build.cpp.hpp"
#include "platforms/esp/32/drivers/spi/_build.cpp.hpp"
#include "platforms/esp/32/drivers/uart/_build.cpp.hpp"

FL_NO_UNWIND_END

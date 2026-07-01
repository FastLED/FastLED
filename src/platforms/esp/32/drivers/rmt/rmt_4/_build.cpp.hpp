// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\rmt\rmt_4/ directory
/// Includes all implementation files in alphabetical order

#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_4/network_state_tracker_4.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.cpp.hpp"

// BusTraits<Bus::RMT> specialization (RMT4 path, mutually exclusive with RMT5).
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h"

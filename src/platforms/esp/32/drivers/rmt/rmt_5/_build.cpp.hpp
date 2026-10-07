// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\rmt\rmt_5/ directory
/// Includes all implementation files in alphabetical order

#if defined(FASTLED_STUB_IMPL) && defined(FL_RMT_MEMORY_MANAGER_HOST_TU)
// The isolated native unit exercises the real allocator without compiling
// the channel driver's independent mock implementation into the same TU.
#include "platforms/shared/mock/esp/32/drivers/rmt_memory_manager_host.h"
#else
#include "platforms/esp/32/drivers/rmt/rmt_5/buffer_pool.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/channel_driver_rmt.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/network_detector.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/network_state_tracker.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/rmt5_controller_lowlevel.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/rmt5_peripheral_esp.cpp.hpp"
#endif
#include "platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.cpp.hpp"

// BusTraits<Bus::RMT> specialization (header-only, included for parse + ODR-keep).
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"

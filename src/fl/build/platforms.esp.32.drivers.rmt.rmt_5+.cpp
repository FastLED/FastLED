/// @brief Isolated production RMT allocator unity entry for native tests.
#include "platforms/new.h"
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep
#if defined(FASTLED_STUB_IMPL) && defined(FL_RMT_MEMORY_MANAGER_HOST_TU)
#include "platforms/esp/32/drivers/rmt/rmt_5/_build.cpp.hpp"
#endif

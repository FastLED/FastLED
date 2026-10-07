/// @file fl/stl/isr/handler.cpp.hpp
/// @brief ISR handler API implementation (delegates to platforms/isr.h)

#include "fl/stl/isr/handler.h"
#include "platforms/isr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace isr {

int attach_timer_handler(const config& cfg, handle* out_handle) FL_NO_EXCEPT {
    return isr::platforms::attach_timer_handler(cfg, out_handle);
}

int attach_external_handler(u8 pin, const config& cfg, handle* out_handle) FL_NO_EXCEPT {
    return isr::platforms::attach_external_handler(pin, cfg, out_handle);
}

int detach_handler(handle& h) FL_NO_EXCEPT {
    return isr::platforms::detach_handler(h);
}

int enable_handler(handle& h) FL_NO_EXCEPT {
    return isr::platforms::enable_handler(h);
}

int disable_handler(handle& h) FL_NO_EXCEPT {
    return isr::platforms::disable_handler(h);
}

bool is_handler_enabled(const handle& h) FL_NO_EXCEPT {
    return isr::platforms::is_handler_enabled(h);
}

const char* get_error_string(int error_code) FL_NO_EXCEPT {
    return isr::platforms::get_error_string(error_code);
}

const char* get_platform_name() FL_NO_EXCEPT {
    return isr::platforms::get_platform_name();
}

u32 get_max_timer_frequency() FL_NO_EXCEPT {
    return isr::platforms::get_max_timer_frequency();
}

u32 get_min_timer_frequency() FL_NO_EXCEPT {
    return isr::platforms::get_min_timer_frequency();
}

u8 get_max_priority() FL_NO_EXCEPT {
    return isr::platforms::get_max_priority();
}

bool requires_assembly_handler(u8 priority) FL_NO_EXCEPT {
    return isr::platforms::requires_assembly_handler(priority);
}

} // namespace isr
} // namespace fl

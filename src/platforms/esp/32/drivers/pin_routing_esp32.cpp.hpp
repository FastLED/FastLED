// IWYU pragma: private

/// @file pin_routing_esp32.cpp.hpp
/// @brief GPIO routing snapshots for drivers that keep a pin bound while idle

#include "fl/stl/compiler_control.h"
#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/drivers/pin_routing_esp32.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "esp_idf_version.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace esp32 {

u32 pinRoutingSnapshot(int pin) FL_NO_EXCEPT {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    gpio_io_config_t io = {};
    if (gpio_get_io_config(static_cast<gpio_num_t>(pin), &io) != ESP_OK) {
        return kPinRoutingUnknown;
    }
    return (io.sig_out & 0xFFFFu) | (io.oe ? (1u << 16) : 0u) |
           ((io.fun_sel & 0xFFu) << 20);
#else
    (void)pin;
    return kPinRoutingUnknown;
#endif
}

} // namespace esp32
} // namespace fl

#endif // FL_IS_ESP32

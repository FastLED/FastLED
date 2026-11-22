// IWYU pragma: private

#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32
#include "platforms/esp/32/core/pin_routing.h"
#include "platforms/esp/esp_version.h"
#include "fl/stl/compiler_control.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#include "driver/gpio.h" // IWYU pragma: keep
#include "soc/gpio_reg.h" // IWYU pragma: keep
#include "soc/gpio_periph.h" // IWYU pragma: keep
#include "soc/io_mux_reg.h" // IWYU pragma: keep
#include "soc/soc_caps.h" // IWYU pragma: keep
#endif

namespace fl {
namespace detail {

FL_NO_INLINE u32 readPinRouting(int pin) FL_NO_EXCEPT {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    if (!GPIO_IS_VALID_GPIO(pin) || !GPIO_PIN_MUX_REG[pin]) {
        return 0xFFFFFFFFu;
    }
    // Read only the three routing fields, rather than the full GPIO config.
    // A hardware token also observes raw SDK users without notification hooks.
    const u32 signal = REG_GET_FIELD(GPIO_FUNC0_OUT_SEL_CFG_REG + pin * 4, GPIO_FUNC0_OUT_SEL);
    u32 enableRegister = GPIO_ENABLE_REG;
#if SOC_GPIO_PIN_COUNT > 32
    if (pin >= 32) {
        enableRegister = GPIO_ENABLE1_REG;
    }
#endif
    const u32 enabled = (REG_READ(enableRegister) >> (pin & 31)) & 1u;
    const u32 function = REG_GET_FIELD(GPIO_PIN_MUX_REG[pin], MCU_SEL);
    return (signal & 0xFFFFu) | (enabled << 16) | ((function & 0xFFu) << 20);
#else
    (void)pin;
    return 0xFFFFFFFFu;
#endif
}

} // namespace detail
} // namespace fl
#endif // FL_IS_ESP32

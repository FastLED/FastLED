// IWYU pragma: private

/// @file rmt4_peripheral_esp.cpp.hpp
/// @brief `Rmt4PeripheralESP` — `IRMT4Peripheral` implementation that
///        forwards to ESP-IDF v4.x `driver/rmt.h`.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if !FASTLED_ESP32_HAS_RMT
// No RMT hardware available
#elif defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C5) ||                    \
    defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32H2)
// Skip for RMT5-only chips
#elif !FASTLED_RMT5 // Only compile for the RMT4 path

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "driver/rmt.h"
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR > 3
#include "esp_intr_alloc.h"
#else
#include "esp_intr.h"
#endif
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

bool Rmt4PeripheralESP::configureChannel(const detail::Rmt4ChannelConfig &cfg)
    FL_NO_EXCEPT {
    rmt_config_t rmt_tx = {};
    rmt_tx.channel = static_cast<rmt_channel_t>(cfg.mChannel);
    rmt_tx.rmt_mode = RMT_MODE_TX;
    rmt_tx.gpio_num = static_cast<gpio_num_t>(cfg.mGpioPin);
    rmt_tx.mem_block_num = cfg.mMemBlocks;
    rmt_tx.clk_div = cfg.mClockDivider;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    rmt_tx.tx_config.carrier_en = false;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmt_tx.tx_config.idle_output_en = true;

    esp_err_t err = rmt_config(&rmt_tx);
    if (err != ESP_OK) {
        FL_WARN_F(
            "Rmt4PeripheralESP: rmt_config failed on channel %s, error=%s",
            cfg.mChannel, err);
        return false;
    }
    return true;
}

bool Rmt4PeripheralESP::installDriver(int channel) FL_NO_EXCEPT {
    esp_err_t err =
        rmt_driver_install(static_cast<rmt_channel_t>(channel), 0, 0);
    if (err != ESP_OK) {
        FL_WARN_F("Rmt4PeripheralESP: rmt_driver_install failed on channel %s, "
                  "error=%s",
                  channel, err);
        return false;
    }
    return true;
}

bool Rmt4PeripheralESP::uninstallDriver(int channel) FL_NO_EXCEPT {
    esp_err_t err = rmt_driver_uninstall(static_cast<rmt_channel_t>(channel));
    return err == ESP_OK;
}

bool Rmt4PeripheralESP::setTxThresholdIntrEnable(int channel, bool enable,
                                                 u16 threshold) FL_NO_EXCEPT {
    esp_err_t err = rmt_set_tx_thr_intr_en(static_cast<rmt_channel_t>(channel),
                                           enable, threshold);
    if (err != ESP_OK) {
        FL_WARN_F("Rmt4PeripheralESP: rmt_set_tx_thr_intr_en failed on channel "
                  "%s, error=%s",
                  channel, err);
        return false;
    }
    return true;
}

bool Rmt4PeripheralESP::setTxIntrEnable(int channel, bool enable) FL_NO_EXCEPT {
    esp_err_t err =
        rmt_set_tx_intr_en(static_cast<rmt_channel_t>(channel), enable);
    if (err != ESP_OK) {
        FL_WARN_F("Rmt4PeripheralESP: rmt_set_tx_intr_en failed on channel %s, "
                  "error=%s",
                  channel, err);
        return false;
    }
    return true;
}

bool Rmt4PeripheralESP::setGpio(int channel, int gpio_pin,
                                bool invert) FL_NO_EXCEPT {
    esp_err_t err;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    err = rmt_set_gpio(static_cast<rmt_channel_t>(channel), RMT_MODE_TX,
                       static_cast<gpio_num_t>(gpio_pin), invert);
#else
    (void)invert;
    err = rmt_set_pin(static_cast<rmt_channel_t>(channel), RMT_MODE_TX,
                      static_cast<gpio_num_t>(gpio_pin));
#endif
    if (err != ESP_OK) {
        FL_WARN_F("Rmt4PeripheralESP: rmt_set_gpio/rmt_set_pin failed on "
                  "channel %s pin %s, error=%s",
                  channel, gpio_pin, err);
        return false;
    }
    return true;
}

bool Rmt4PeripheralESP::installIsr(detail::Rmt4IsrHandler handler, void *arg,
                                   void **out_handle) FL_NO_EXCEPT {
    if (out_handle == nullptr) {
        return false;
    }
    intr_handle_t handle = nullptr;
    esp_err_t err = esp_intr_alloc(ETS_RMT_INTR_SOURCE,
                                   ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                                   handler, arg, &handle);
    if (err != ESP_OK) {
        FL_WARN_F("Rmt4PeripheralESP: esp_intr_alloc failed, error=%s", err);
        *out_handle = nullptr;
        return false;
    }
    *out_handle = static_cast<void *>(handle);
    return true;
}

void Rmt4PeripheralESP::freeIsr(void *handle) FL_NO_EXCEPT {
    if (handle != nullptr) {
        esp_intr_free(static_cast<intr_handle_t>(handle));
    }
}

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip
#endif // FL_IS_ESP32

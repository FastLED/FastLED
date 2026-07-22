// IWYU pragma: private

/// @file platforms/esp/32/drivers/cled.cpp
/// ESP32 Custom LED (CLED) driver implementation

#include "platforms/esp/32/drivers/cled.h"

#include "platforms/is_platform.h"
#include "fl/stl/noexcept.h"
#if defined(FL_IS_ESP32)

#include "fl/stl/has_include.h"

#if FL_HAS_INCLUDE("driver/ledc.h")
    // IWYU pragma: begin_keep
    #include "driver/ledc.h"
    // IWYU pragma: end_keep
    #define FL_CLED_HAS_LEDC 1
#else
    // No driver/ledc.h on this toolchain (very old Arduino-ESP32 core) —
    // fall back to the Arduino LEDC wrapper API.
    // IWYU pragma: begin_keep
    #include "fl/system/arduino.h"
    // IWYU pragma: end_keep
#endif

#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/esp/esp_version.h"
#include "fl/log/log.h"

namespace fl {
namespace esp32 {

CLED::CLED() FL_NO_EXCEPT
    : mMaxDuty(0)
    , mInitialized(false) {
}

CLED::~CLED() {
    end();
}

bool CLED::begin(const CLEDConfig& config) FL_NO_EXCEPT {
    // Clean up any previous initialization
    if (mInitialized) {
        end();
    }

    mConfig = config;
    mMaxDuty = (1 << config.resolution_bits) - 1;

    // Validate parameters
    if (config.resolution_bits > 20) {
        // ESP32 LEDC maximum is 20 bits
        FL_WARN_F("CLED: resolution_bits > 20 not supported (requested: %s)", config.resolution_bits);
        return false;
    }

#ifdef FL_CLED_HAS_LEDC
    // Native ESP-IDF LEDC setup — identical under Arduino-ESP32 (which
    // bundles driver/ledc.h) and bare ESP-IDF builds. CLED models its
    // channel space the same way write16() below does: group = channel/8
    // selects the speed mode (low-speed only on chips without
    // SOC_LEDC_SUPPORT_HS_MODE), and each group's 8 channels share 4
    // timers, 2 channels per timer.
    u8 group = config.channel / 8;
    u8 channel_in_group = config.channel % 8;
    ledc_mode_t mode = ledc_mode_t(group);
    ledc_timer_t timer = ledc_timer_t(channel_in_group / 2);

    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = mode;
    timer_cfg.timer_num = timer;
    timer_cfg.duty_resolution = static_cast<ledc_timer_bit_t>(config.resolution_bits);
    timer_cfg.freq_hz = config.frequency;
#if ESP_IDF_VERSION_4_OR_HIGHER
    // clk_cfg field and LEDC_AUTO_CLK added in IDF 4.0
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
#endif

    if (ledc_timer_config(&timer_cfg) != ESP_OK) {
        FL_WARN_F("CLED: LEDC timer config failed for channel %s", config.channel);
        return false;
    }

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.speed_mode = mode;
    ch_cfg.channel = ledc_channel_t(channel_in_group);
    ch_cfg.timer_sel = timer;
    ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ch_cfg.gpio_num = config.pin;
    ch_cfg.duty = 0;
    ch_cfg.hpoint = 0;

    if (ledc_channel_config(&ch_cfg) != ESP_OK) {
        FL_WARN_F("CLED: LEDC channel config failed for channel %s", config.channel);
        return false;
    }

    FL_DBG_F("CLED: Initialized channel %s at %s Hz, %s bits", config.channel, config.frequency, config.resolution_bits);
#else
    // No driver/ledc.h on this toolchain — fall back to the Arduino LEDC
    // wrapper API, which differs between Arduino Core 2.x and 3.x.
#ifndef ESP_ARDUINO_VERSION_MAJOR
    #define ESP_ARDUINO_VERSION_MAJOR 2  // Assume old API if not defined
#endif

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    // New API (Arduino Core 3.x): ledcAttach auto-assigns channel
    u8 assigned_channel = ledcAttach(config.pin, config.frequency, config.resolution_bits);
    if (assigned_channel == 0) {
        FL_WARN_F("CLED: LEDC attach failed for pin %s", config.pin);
        return false;
    }

    // Update internal state with auto-assigned channel
    mConfig.channel = assigned_channel;

    FL_DBG_F("CLED: Initialized pin %s with auto-assigned channel %s at %s Hz, %s bits", config.pin, static_cast<int>(assigned_channel), config.frequency, config.resolution_bits);
#else
    // Old API (Arduino Core 2.x): ledcSetup + ledcAttachPin with explicit channel
    ledcAttachPin(config.pin, config.channel);
    u32 freq = ledcSetup(config.channel, config.frequency, config.resolution_bits);

    if (freq == 0) {
        FL_WARN_F("CLED: LEDC setup failed for channel %s", config.channel);
        return false;
    }

    FL_DBG_F("CLED: Initialized channel %s at %s Hz, %s bits", config.channel, freq, config.resolution_bits);
#endif
#endif  // FL_CLED_HAS_LEDC

    mInitialized = true;

    // Initialize to off
    write16(0);

    return true;
}

void CLED::end() FL_NO_EXCEPT {
    if (!mInitialized) {
        return;
    }

    // TODO: Add LEDC cleanup (ledcDetach?) when available in Arduino core

    mInitialized = false;
}

void CLED::write16(u16 value) FL_NO_EXCEPT {
    // Accept 16-bit input (0-65535), scale to configured resolution
    // Users apply gamma correction upstream
    u32 duty = mapToDutyCycle(value);

    // Invert for sink configuration
    if (mConfig.is_sink) {
        duty = mMaxDuty - duty;
    }

    // Clamp to maximum
    if (duty > mMaxDuty) {
        duty = mMaxDuty;
    }

#ifdef FL_CLED_HAS_LEDC
    // For full-on condition with LEDC, add 1 to duty to get true 100%
    // This is an ESP32 LEDC quirk where max_duty gives 99.998% duty cycle
    if (duty == mMaxDuty && mMaxDuty != 1) {
        duty = mMaxDuty + 1;
    }

    u8 group = mConfig.channel / 8;
    u8 channel = mConfig.channel % 8;

    ledc_set_duty(ledc_mode_t(group), ledc_channel_t(channel), duty);
    ledc_update_duty(ledc_mode_t(group), ledc_channel_t(channel));
#else
    // Fallback to Arduino API
    ledcWrite(mConfig.channel, duty);
#endif
}

u32 CLED::getMaxDuty() const FL_NO_EXCEPT {
    return mMaxDuty;
}

u8 CLED::getResolutionBits() const FL_NO_EXCEPT {
    return mConfig.resolution_bits;
}

u32 CLED::mapToDutyCycle(u16 val16) const FL_NO_EXCEPT {
    // Map 16-bit input (0-65535) to current resolution
    // Formula: (val16 * maxDuty + 32767) / 65535 (with rounding)
    // Use 32-bit math to avoid overflow
    // Adding 32767 (half of 65535) provides proper rounding to nearest integer
    return ((u32(val16) * mMaxDuty) + 32767) / 65535;
}

}  // namespace esp32
}  // namespace fl

#endif  // FL_IS_ESP32

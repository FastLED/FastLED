/// @file platforms/esp/32/drivers/cled.cpp
/// ESP32 Custom LED (CLED) driver implementation

#include "platforms/esp/32/drivers/cled.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <Arduino.h>

#ifdef __has_include
    #if __has_include("driver/ledc.h")
        #include "driver/ledc.h"
        #define FL_CLED_HAS_LEDC 1
    #endif
#endif

#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/dbg.h"

namespace fl {
namespace esp32 {

CLED::CLED()
    : mMaxDuty(0)
    , mInitialized(false) {
}

CLED::~CLED() {
    end();
}

bool CLED::begin(const CLEDConfig& config) {
    // Clean up any previous initialization
    if (mInitialized) {
        end();
    }

    mConfig = config;
    mMaxDuty = (1 << config.resolution_bits) - 1;

    // Validate parameters
    if (config.resolution_bits > 20) {
        // ESP32 LEDC maximum is 20 bits
        FL_WARN("CLED: resolution_bits > 20 not supported (requested: " << config.resolution_bits << ")");
        return false;
    }

    // Arduino LEDC setup - API differs between Arduino Core 2.x and 3.x
#ifndef ESP_ARDUINO_VERSION_MAJOR
    #define ESP_ARDUINO_VERSION_MAJOR 2  // Assume old API if not defined
#endif

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    // New API (Arduino Core 3.x): ledcAttach auto-assigns channel
    uint8_t assigned_channel = ledcAttach(config.pin, config.frequency, config.resolution_bits);
    if (assigned_channel == 0) {
        FL_WARN("CLED: LEDC attach failed for pin " << config.pin);
        return false;
    }

    // Update internal state with auto-assigned channel
    mConfig.channel = assigned_channel;

    FL_DBG("CLED: Initialized pin " << config.pin << " with auto-assigned channel "
           << static_cast<int>(assigned_channel) << " at " << config.frequency
           << " Hz, " << config.resolution_bits << " bits");
#else
    // Old API (Arduino Core 2.x): ledcSetup + ledcAttachPin with explicit channel
    ledcAttachPin(config.pin, config.channel);
    uint32_t freq = ledcSetup(config.channel, config.frequency, config.resolution_bits);

    if (freq == 0) {
        FL_WARN("CLED: LEDC setup failed for channel " << config.channel);
        return false;
    }

    FL_DBG("CLED: Initialized channel " << config.channel << " at " << freq
           << " Hz, " << config.resolution_bits << " bits");
#endif

    mInitialized = true;

    // Initialize to off
    write16(0);

    return true;
}

void CLED::end() {
    if (!mInitialized) {
        return;
    }

    // TODO: Add LEDC cleanup (ledcDetach?) when available in Arduino core

    mInitialized = false;
}

void CLED::write16(uint16_t value) {
    // Accept 16-bit input (0-65535), scale to configured resolution
    // Users apply gamma correction upstream
    uint32_t duty = mapToDutyCycle(value);

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

    uint8_t group = mConfig.channel / 8;
    uint8_t channel = mConfig.channel % 8;

    ledc_set_duty(ledc_mode_t(group), ledc_channel_t(channel), duty);
    ledc_update_duty(ledc_mode_t(group), ledc_channel_t(channel));
#else
    // Fallback to Arduino API
    ledcWrite(mConfig.channel, duty);
#endif
}

uint32_t CLED::getMaxDuty() const {
    return mMaxDuty;
}

uint8_t CLED::getResolutionBits() const {
    return mConfig.resolution_bits;
}

uint32_t CLED::mapToDutyCycle(uint16_t val16) const {
    // Map 16-bit input (0-65535) to current resolution
    // Formula: (val16 * maxDuty + 32767) / 65535 (with rounding)
    // Use 32-bit math to avoid overflow
    // Adding 32767 (half of 65535) provides proper rounding to nearest integer
    return ((uint32_t(val16) * mMaxDuty) + 32767) / 65535;
}

}  // namespace esp32
}  // namespace fl

#endif  // ESP32

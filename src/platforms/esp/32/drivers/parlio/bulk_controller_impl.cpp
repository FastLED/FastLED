/// @file bulk_controller_impl.cpp
/// @brief Implementation of BulkControllerImpl for ESP32-P4 PARLIO peripheral
///
/// This file contains all the business logic for managing multiple LED strips:
/// - Strip management (add/remove/get)
/// - Pin validation
/// - Settings management
/// - Multi-strip pixel iteration and transmission

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "bulk_controller_impl.h"
#include "parlio_transmitter.h"
#include "pixel_iterator.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/engine_events.h"
#include "fl/clockless/base.h"

namespace fl {

BulkControllerImpl::BulkControllerImpl(IParlioTransmitter& transmitter,
                                       const BulkStrip::Settings& defaultSettings)
    : mTransmitter(transmitter)
    , mDefaultSettings(defaultSettings) {
}

BulkStrip* BulkControllerImpl::add(int pin, CRGB* buffer, int count,
                                   const ScreenMap& screenmap) {
    // 1. Validate pin
    if (!validatePin(pin)) {
        FL_WARN("BulkControllerImpl: Invalid pin " << pin);
        return nullptr;
    }

    // 2. Check duplicate
    if (mSubControllers.find(pin) != mSubControllers.end()) {
        FL_WARN("BulkControllerImpl: Pin " << pin << " already in use");
        return nullptr;
    }

    // 3. Check max strips
    if (static_cast<int>(mSubControllers.size()) >= MAX_STRIPS) {
        FL_WARN("BulkControllerImpl: Max strips (" << MAX_STRIPS << ") exceeded");
        return nullptr;
    }

    // 4. Create sub-controller with settings
    BulkStrip sub(pin, buffer, count, screenmap);
    sub.settings = mDefaultSettings;
    mSubControllers[pin] = sub;

    // 5. Register with transmitter (will be configured on first flush)
    bool is_rgbw = sub.settings.rgbw.active();
    mTransmitter.addStrip(static_cast<uint8_t>(pin), static_cast<uint16_t>(count), is_rgbw);

    FL_DBG("BulkControllerImpl: Added strip on pin " << pin << " (" << count << " LEDs, "
           << (is_rgbw ? "RGBW" : "RGB") << ")");

    return &mSubControllers[pin];
}

bool BulkControllerImpl::remove(int pin) {
    auto it = mSubControllers.find(pin);
    if (it == mSubControllers.end()) {
        return false;
    }

    // Note: IParlioTransmitter doesn't currently have removeStrip() method
    // Strips are re-registered on each flush based on mSubControllers state
    mSubControllers.erase(it);

    FL_DBG("BulkControllerImpl: Removed strip on pin " << pin);
    return true;
}

const BulkStrip* BulkControllerImpl::get(int pin) const {
    auto it = mSubControllers.find(pin);
    return (it != mSubControllers.end()) ? &it->second : nullptr;
}

BulkStrip* BulkControllerImpl::get(int pin) {
    auto it = mSubControllers.find(pin);
    return (it != mSubControllers.end()) ? &it->second : nullptr;
}

int BulkControllerImpl::size() const {
    int total = 0;
    for (const auto& pair : mSubControllers) {
        total += pair.second.getCount();
    }
    return total;
}

int BulkControllerImpl::stripCount() const {
    return static_cast<int>(mSubControllers.size());
}

bool BulkControllerImpl::has(int pin) const {
    return mSubControllers.find(pin) != mSubControllers.end();
}

void BulkControllerImpl::setDefaultSettings(const BulkStrip::Settings& settings) {
    mDefaultSettings = settings;
}

void BulkControllerImpl::setCorrection(CRGB correction) {
    mDefaultSettings.correction = correction;
}

void BulkControllerImpl::setTemperature(CRGB temperature) {
    mDefaultSettings.temperature = temperature;
}

void BulkControllerImpl::setDither(u8 ditherMode) {
    mDefaultSettings.ditherMode = ditherMode;
}

void BulkControllerImpl::setRgbw(const Rgbw& rgbw) {
    mDefaultSettings.rgbw = rgbw;
}

void BulkControllerImpl::showPixels(u8 brightness) {
    if (mSubControllers.empty()) {
        FL_DBG("BulkControllerImpl: No strips to show");
        return;
    }

    // Start frame queuing
    mTransmitter.onQueuingStart();

    // Write pixel data for each strip
    for (auto& [pin, strip] : mSubControllers) {
        CRGB* buffer = strip.getBuffer();
        if (!buffer) {
            FL_WARN("BulkControllerImpl: Strip on pin " << pin << " has null buffer, skipping");
            continue;
        }

        int count = strip.getCount();

        // Create PixelIterator with per-strip settings
        // PixelIterator automatically handles brightness, correction, temperature, dither
        ColorAdjustment adj = BulkClocklessHelper::computeAdjustment(brightness, strip.settings);
        PixelIterator pixels(buffer, count, adj, strip.settings.ditherMode, strip.settings.rgbw);

        // Write to transmitter
        mTransmitter.writePixels(static_cast<uint8_t>(pin), pixels);
    }

    // Notify queuing complete
    mTransmitter.onQueuingDone();

    // Flush to hardware (initiates DMA transmission)
    mTransmitter.flush();

    FL_DBG("BulkControllerImpl: Transmitted " << mSubControllers.size() << " strips");
}

vector<int> BulkControllerImpl::getAllPins() const {
    vector<int> pins;
    pins.reserve(mSubControllers.size());
    for (const auto& pair : mSubControllers) {
        pins.push_back(pair.first);
    }
    return pins;
}

int BulkControllerImpl::removeAll() {
    int count = static_cast<int>(mSubControllers.size());
    mSubControllers.clear();
    return count;
}

bool BulkControllerImpl::validatePin(int pin) const {
    // ESP32-P4 GPIO validation
    if (pin < 0 || pin > 54) {
        FL_WARN("BulkControllerImpl: GPIO pin must be in range 0-54 for ESP32-P4");
        return false;
    }

    // Reject strapping pins (GPIO34-38)
    if (pin >= 34 && pin <= 38) {
        FL_WARN("BulkControllerImpl: GPIO34-38 are strapping pins and CANNOT be used");
        return false;
    }

    // Reject USB-JTAG pins (GPIO24-25)
    if (pin == 24 || pin == 25) {
        FL_WARN("BulkControllerImpl: GPIO24-25 are reserved for USB-JTAG");
        return false;
    }

    return true;
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4

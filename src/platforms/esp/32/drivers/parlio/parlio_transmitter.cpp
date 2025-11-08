/// @file parlio_transmitter.cpp
/// @brief Implementation of PARLIO transmitter class

#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_transmitter.h"
#include "parlio_hub.h"          // ParlioHub
#include "fl/pixel_iterator.h"        // fl::PixelIterator
#include "fl/chipsets/led_timing.h"   // TIMING_WS2812_800KHZ, WS2812ChipsetTiming
#include "fl/singleton.h"             // fl::Singleton
#include "fl/map.h"                   // fl::map for chipset registry

// ParlioTransmitterBase is defined in clockless_parlio_esp32p4.cpp (anonymous namespace)
// Forward declaration of factory function
namespace fl {
class ParlioTransmitterBase;
ParlioTransmitterBase* createParlioTransmitterBase(const ChipsetTimingConfig& timing);
}

namespace fl {

// Static registry mapping timing configs to IParlioTransmitter instances
static fl::map<uint32_t, IParlioTransmitter*>& getTransmitterRegistry() {
    static fl::map<uint32_t, IParlioTransmitter*> registry;
    return registry;
}

// Hash function for ChipsetTimingConfig
static uint32_t hashTiming(const ChipsetTimingConfig& timing) {
    // Simple hash: XOR all timing values
    return timing.t1_ns ^ timing.t2_ns ^ timing.t3_ns ^ timing.reset_us;
}

// Constructor
IParlioTransmitter::IParlioTransmitter(const ChipsetTimingConfig& timing)
    : mBase(createParlioTransmitterBase(timing))
    , mTiming(timing)
{
    // Register with ParlioHub for cross-chipset coordination
    ParlioHub::getInstance().registerTransmitter(this, &flushCallback);
}

// Destructor
IParlioTransmitter::~IParlioTransmitter() {
    // Unregister from ParlioHub
    ParlioHub::getInstance().unregisterTransmitter(this);
}

// Runtime factory function - directly uses timing config
IParlioTransmitter& IParlioTransmitter::getOrCreate(const ChipsetTimingConfig& timing) {
    // Hash the timing to find existing instance
    uint32_t hash = hashTiming(timing);

    auto& registry = getTransmitterRegistry();
    auto it = registry.find(hash);

    if (it != registry.end()) {
        return *it->second;
    }

    // Create new instance
    IParlioTransmitter* transmitter = new IParlioTransmitter(timing);
    registry[hash] = transmitter;

    return *transmitter;
}

// Templated factory function - converts compile-time CHIPSET to runtime config
template <typename CHIPSET>
IParlioTransmitter& IParlioTransmitter::getOrCreate() {
    // Convert compile-time CHIPSET to runtime config and delegate to runtime version
    return getOrCreate(makeTimingConfig<CHIPSET>());
}

// Method implementations
void IParlioTransmitter::onQueuingStart() {
    mBase->onQueuingStart();
}

bool IParlioTransmitter::isQueuing() const {
    return mBase->isQueuing();
}

void IParlioTransmitter::onQueuingDone() {
    mBase->onQueuingDone();
}

void IParlioTransmitter::addStrip(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
    mBase->addObject(pin, numLeds, is_rgbw);
}

void IParlioTransmitter::writePixels(uint8_t data_pin, fl::PixelIterator& pixel_iterator) {
    mBase->writePixels(data_pin, pixel_iterator);
}

void IParlioTransmitter::flush() {
    mBase->showPixelsOnceThisFrame();
}

}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32

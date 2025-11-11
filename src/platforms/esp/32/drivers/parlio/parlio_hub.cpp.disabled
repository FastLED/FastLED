/// @file parlio_hub.cpp
/// @brief Implementation of PARLIO transmitter coordination hub

#if defined(ESP32)
#include "sdkconfig.h"

// PARLIO driver is only available on ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_hub.h"
#include "fl/singleton.h"
#include "fl/log.h"

namespace fl {

ParlioHub& ParlioHub::getInstance() {
    return fl::Singleton<ParlioHub>::instance();
}

void ParlioHub::registerTransmitter(void* transmitterPtr, void (*flushFunc)(void*)) {
    TransmitterEntry entry{transmitterPtr, flushFunc};

    // Only add if not already registered
    if (!contains(entry)) {
        mTransmitters.push_back(entry);
        FL_DBG("PARLIO Hub: Registered transmitter " << transmitterPtr);
    }
}

void ParlioHub::unregisterTransmitter(void* transmitterPtr) {
    // Find and remove the transmitter from the hub
    for (auto it = mTransmitters.begin(); it != mTransmitters.end(); ++it) {
        if (it->transmitterPtr == transmitterPtr) {
            mTransmitters.erase(it);
            FL_DBG("PARLIO Hub: Unregistered transmitter " << transmitterPtr);
            return;
        }
    }
}

void ParlioHub::flushAll() {
    FL_DBG("PARLIO Hub: Flushing all " << mTransmitters.size() << " transmitters");
    for (auto& entry : mTransmitters) {
        if (entry.flushFunc != nullptr) {
            entry.flushFunc(entry.transmitterPtr);
        }
    }
}

void ParlioHub::flushAllExcept(void* exceptPtr) {
    FL_DBG("PARLIO Hub: Flushing all transmitters except " << exceptPtr);
    for (auto& entry : mTransmitters) {
        if (entry.transmitterPtr != exceptPtr && entry.flushFunc != nullptr) {
            FL_DBG("PARLIO Hub: Flushing transmitter " << entry.transmitterPtr);
            entry.flushFunc(entry.transmitterPtr);
        }
    }
}

bool ParlioHub::contains(const TransmitterEntry& entry) {
    for (const auto& existing : mTransmitters) {
        if (existing == entry) {
            return true;
        }
    }
    return false;
}

} // namespace fl

#endif  // CONFIG_IDF_TARGET_ESP32P4
#endif  // ESP32

/// @file lcd_i80_registry.cpp
/// @brief Implementation of LCD I80 chipset group registry

#if defined(ESP32)
#include "sdkconfig.h"

// I80 LCD driver is only available on ESP32-S3 and ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)

#include "lcd_i80_registry.h"

namespace fl {

void LCDI80Esp32Registry::registerGroup(void* groupPtr, void (*flushFunc)(void*)) {
    GroupEntry entry{groupPtr, flushFunc};

    // Only add if not already registered
    if (!contains(entry)) {
        mGroups.push_back(entry);
    }
}

void LCDI80Esp32Registry::flushAll() {
    for (auto& entry : mGroups) {
        if (entry.flushFunc != nullptr) {
            entry.flushFunc(entry.groupPtr);
        }
    }
}

void LCDI80Esp32Registry::flushAllExcept(void* exceptPtr) {
    for (auto& entry : mGroups) {
        if (entry.groupPtr != exceptPtr && entry.flushFunc != nullptr) {
            entry.flushFunc(entry.groupPtr);
        }
    }
}

bool LCDI80Esp32Registry::contains(const GroupEntry& entry) {
    for (const auto& existing : mGroups) {
        if (existing == entry) {
            return true;
        }
    }
    return false;
}

} // namespace fl

#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#endif  // ESP32

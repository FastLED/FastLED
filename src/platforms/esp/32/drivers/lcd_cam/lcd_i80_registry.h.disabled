/// @file lcd_i80_registry.h
/// @brief Registry for tracking multiple LCD I80 chipset groups
///
/// Enables multi-chipset support by managing per-chipset singleton groups.
/// When chipset timing changes mid-frame, the registry flushes pending groups
/// to prevent timing conflicts.
///
/// Pattern inspired by ObjectFLED registry on Teensy 4.x

#pragma once

#include "fl/vector.h"
#include "fl/singleton.h"

namespace fl {

/// @brief Global registry for tracking all active LCD I80 chipset groups
///
/// This registry enables multiple chipset timings to coexist by:
/// 1. Tracking all per-chipset singleton groups
/// 2. Flushing pending groups when chipset changes
/// 3. Preventing timing conflicts between different chipsets
///
/// Example usage:
/// @code
/// // User creates two different chipset controllers
/// FastLED.addLeds<WS2812, 8>(leds1, 100);  // Creates LCDI80Esp32Group<WS2812>
/// FastLED.addLeds<SK6812, 9>(leds2, 100);  // Creates LCDI80Esp32Group<SK6812>
///
/// // When switching from WS2812 to SK6812:
/// // - SK6812's beginShowLeds() calls registry.flushAllExcept(SK6812_group)
/// // - Registry flushes WS2812 group before SK6812 starts queuing
/// // - Prevents timing conflicts
/// @endcode
class LCDI80Esp32Registry {
public:
    /// Get singleton instance
    static LCDI80Esp32Registry& getInstance() {
        return fl::Singleton<LCDI80Esp32Registry>::instance();
    }

    /// Register a chipset group for tracking
    /// @param groupPtr Opaque pointer to the group (void* for type erasure)
    /// @param flushFunc Function pointer to call group's flush() method
    void registerGroup(void* groupPtr, void (*flushFunc)(void*));

    /// Flush all registered groups
    /// Called at end of frame or when explicit flush needed
    void flushAll();

    /// Flush all groups except the specified one
    /// Called when switching chipsets mid-frame
    /// @param exceptPtr Pointer to group that should NOT be flushed
    void flushAllExcept(void* exceptPtr);

private:
    /// Group entry in registry
    struct GroupEntry {
        void* groupPtr;              ///< Opaque pointer to group
        void (*flushFunc)(void*);    ///< Function to flush this group

        bool operator==(const GroupEntry& other) const {
            return groupPtr == other.groupPtr;
        }
    };

    /// List of registered groups
    fl::vector<GroupEntry> mGroups;

    /// Check if entry already exists in registry
    /// @param entry Entry to search for
    /// @returns true if entry is already registered
    bool contains(const GroupEntry& entry);
};

} // namespace fl

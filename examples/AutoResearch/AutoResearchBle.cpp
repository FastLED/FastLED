// examples/AutoResearch/AutoResearchBle.cpp
//
// BLE autoresearch state accessor.
// BLE transport creation/destruction is handled by
// AutoResearchRemoteControl::startBleRemote()/stopBleRemote() in AutoResearchRemote.cpp.

// Gate the entire file out under low-memory mode -- the LowMemory bring-up
// surface (AutoResearchLowMemory.h) doesn't use BLE state and the full
// fl::net BLE stack does not fit on parts like LPC845-BRK (64 KB flash).
// Matches the conditional structure in AutoResearch.ino itself.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchBle.h"

// Global BLE state
static AutoResearchBleState s_ble_state;

AutoResearchBleState& getBleState() {
    return s_ble_state;
}

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY

// examples/AutoResearch/AutoResearchBle.cpp
//
// BLE autoresearch state accessor.
// BLE transport creation/destruction is handled by
// AutoResearchRemoteControl::startBleRemote()/stopBleRemote() in AutoResearchRemote.cpp.

#include "AutoResearchBle.h"

// Global BLE state
static AutoResearchBleState s_ble_state;

AutoResearchBleState& getBleState() {
    return s_ble_state;
}

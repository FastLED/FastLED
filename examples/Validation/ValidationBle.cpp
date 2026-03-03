// examples/Validation/ValidationBle.cpp
//
// BLE validation state accessor.
// BLE transport creation/destruction is handled by
// ValidationRemoteControl::startBleRemote()/stopBleRemote() in ValidationRemote.cpp.

#include "ValidationBle.h"

// Global BLE state
static ValidationBleState s_ble_state;

ValidationBleState& getBleState() {
    return s_ble_state;
}

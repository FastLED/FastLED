// examples/Validation/ValidationBle.h
//
// BLE validation for ESP32: GATT server for JSON-RPC over BLE.
// Used by `bash validate --ble`.
//
// The ESP32 starts a BLE GATT server with a custom service.
// The host Python script connects via Bleak and validates
// bidirectional JSON-RPC communication (ping/pong).
//
// NOTE: BLE transport creation/destruction is handled by
// ValidationRemoteControl::startBleRemote()/stopBleRemote().
// This file provides state accessors only.

#pragma once

#include "fl/stl/stdint.h"

// BLE configuration constants
#define VALIDATION_BLE_DEVICE_NAME "FastLED-C6"

/// @brief State for BLE validation
struct ValidationBleState {
    bool ble_server_active = false;
};

/// @brief Get current BLE validation state.
/// @return Reference to the global BLE state
ValidationBleState& getBleState();

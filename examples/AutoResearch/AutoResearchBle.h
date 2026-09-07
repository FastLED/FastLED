// examples/AutoResearch/AutoResearchBle.h
//
// BLE autoresearch for ESP32: GATT server for JSON-RPC over BLE.
// Used by `bash autoresearch --ble`.
//
// The ESP32 starts a BLE GATT server with a custom service.
// The host Python script connects via Bleak and tests
// bidirectional JSON-RPC communication (ping/pong).
//
// NOTE: BLE transport creation/destruction is handled by
// AutoResearchRemoteControl::startBleRemote()/stopBleRemote().
// This file provides state accessors only.

#pragma once

#include "fl/stl/stdint.h"

// FL_IS_RP2040 / FL_IS_RP2350 for the advertising-name default below.
#include "platforms/is_platform.h"

// BLE configuration constants
//
// The advertising name is what a host scan matches on, so it has to name the
// board that actually answered. It was hardcoded "FastLED-C6", which meant an
// RP2350W advertised under a C6 name and the scan log reported the wrong chip.
#ifndef AUTORESEARCH_BLE_DEVICE_NAME
#if defined(FL_IS_RP2350)
#define AUTORESEARCH_BLE_DEVICE_NAME "FastLED-RP2350"
#elif defined(FL_IS_RP2040)
#define AUTORESEARCH_BLE_DEVICE_NAME "FastLED-RP2040"
#else
#define AUTORESEARCH_BLE_DEVICE_NAME "FastLED-C6"
#endif
#endif

/// @brief State for BLE autoresearch
struct AutoResearchBleState {
    bool ble_server_active = false;
};

/// @brief Get current BLE autoresearch state.
/// @return Reference to the global BLE state
AutoResearchBleState& getBleState();

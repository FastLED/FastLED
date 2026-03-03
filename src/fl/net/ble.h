// src/fl/net/ble.h
// BLE GATT transport layer for JSON-RPC
// Provides factory functions for creating RequestSource and ResponseSink
// callbacks that communicate over BLE instead of serial.
//
// Platform-neutral header: all ESP32 BLE implementation lives in ble_esp32.cpp.hpp.
//
// Requires: JSON enabled + ESP32 + IDF 5+ + NimBLE headers present.
// If any requirement is missing, FL_BLE_AVAILABLE is 0 and BLE compiles out.

#pragma once

#include "fl/json.h"
#include "fl/has_include.h"

// BLE requires: JSON enabled + ESP32 + IDF 5+ + NimBLE headers present
#if FASTLED_ENABLE_JSON && defined(FL_IS_ESP32) && defined(FL_IS_IDF_5_OR_HIGHER) \
    && FL_HAS_INCLUDE(<nimble/nimble_port.h>)
#define FL_BLE_AVAILABLE 1
#else
#define FL_BLE_AVAILABLE 0
#endif

#if FL_BLE_AVAILABLE

#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/pair.h"

namespace fl {

// GATT UUIDs for FastLED BLE service (string form, used by validation/diagnostics)
#define FL_BLE_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define FL_BLE_CHAR_RX_UUID "12345678-1234-1234-1234-123456789ab0"
#define FL_BLE_CHAR_TX_UUID "12345678-1234-1234-1234-123456789ab1"

// NimBLE BLE_UUID128_INIT takes bytes in LITTLE-ENDIAN order.
// UUID "12345678-1234-1234-1234-123456789abc" in BE byte order is:
//   12 34 56 78  12 34  12 34  12 34  12 34 56 78 9a bc
// Reversed (LE) for BLE_UUID128_INIT:
//   bc 9a 78 56 34 12 34 12 34 12 34 12 78 56 34 12
#define FL_BLE_SERVICE_UUID_INIT \
    0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, \
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12

#define FL_BLE_CHAR_RX_UUID_INIT \
    0xb0, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, \
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12

#define FL_BLE_CHAR_TX_UUID_INIT \
    0xb1, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, \
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12

/// @brief Platform-neutral BLE diagnostics (returned by queryBleStatus)
struct BleStatusInfo {
    bool connected = false;
    int connectedCount = 0;
    bool txCharExists = false;
    int txValueLen = 0;
    int ringHead = 0;
    int ringTail = 0;
};

/// @brief Opaque BLE transport state — defined in ble_esp32.cpp.hpp
struct BleTransportState;

/// @brief Create BLE GATT server, heap-allocate transport state
/// @param deviceName BLE advertising name (e.g., "FastLED-C6")
/// @return Heap-allocated state pointer (caller owns, free with destroyBleTransport)
BleTransportState* createBleTransport(const char* deviceName);

/// @brief Deinitialize BLE stack and free heap state
/// @param state Pointer returned by createBleTransport (safe to call with nullptr)
void destroyBleTransport(BleTransportState* state);

/// @brief Query BLE connection/subscription diagnostics
/// @param state Pointer returned by createBleTransport
/// @return Platform-neutral status snapshot
BleStatusInfo queryBleStatus(const BleTransportState* state);

/// @brief Get RequestSource and ResponseSink lambdas for fl::Remote
/// @param state Pointer returned by createBleTransport (must outlive returned lambdas)
/// @return Pair of {RequestSource, ResponseSink} ready for fl::Remote constructor
fl::pair<fl::function<fl::optional<fl::Json>()>, fl::function<void(const fl::Json&)>>
getBleTransportCallbacks(BleTransportState* state);

} // namespace fl

#endif // FL_BLE_AVAILABLE

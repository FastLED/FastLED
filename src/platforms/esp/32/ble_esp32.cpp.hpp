// platforms/esp/32/ble_esp32.cpp.hpp
// BLE GATT transport layer — ESP32 NimBLE implementation
// Included via platforms/esp/32/_build.cpp.hpp (unity build). Do NOT compile separately.

#pragma once

#include "fl/net/ble.h"
#include "platforms/esp/is_esp.h"

#if FASTLED_ENABLE_JSON && defined(FL_IS_ESP32)

#include "fl/memory.h" // for fl::make_unique
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/cctype.h"
#include "fl/string_view.h"
#include "fl/warn.h"
#include "fl/remote/transport/serial.h" // for formatJsonResponse

// IWYU pragma: begin_keep
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
// IWYU pragma: end_keep

namespace fl {

// Ring buffer size for incoming BLE writes (must be power of 2)
static constexpr int kBleRingBufferSize = 8;

/// @brief State shared between BLE callbacks and the main loop
struct BleTransportState {
    // Ring buffer for incoming JSON-RPC requests (written by NimBLE task, read by main loop)
    fl::string ringBuffer[kBleRingBufferSize];
    volatile int head = 0;  // Written by NimBLE task (producer)
    volatile int tail = 0;  // Written by main loop (consumer)

    // TX characteristic for sending responses via NOTIFY
    BLECharacteristic* txChar = nullptr;

    // Connection state
    volatile bool connected = false;

    // Server handles for cleanup
    BLEServer* server = nullptr;
    BLEService* service = nullptr;
};

/// @brief BLE RX characteristic callback — enqueues incoming writes into ring buffer
///
/// NOTE: On ESP32-C6, BLEServer::onConnect() never fires due to a bug in
/// Arduino ESP32's BLEServer.cpp where ble_gap_conn_find() fails and causes
/// an early return before callbacks are dispatched. This means the server's
/// m_connectedCount stays 0 and notify() never delivers. As a workaround,
/// we track connection state here via onWrite (which DOES fire correctly).
class BleRxCallback : public BLECharacteristicCallbacks {
public:
    explicit BleRxCallback(BleTransportState* state) : mState(state) {}

    // NimBLE dispatches to the 2-param onWrite (with ble_gap_conn_desc*),
    // NOT the 1-param version. We must override this signature.
    void onWrite(BLECharacteristic* pChar, ble_gap_conn_desc* desc) override {
        (void)desc;
        // Track connection via write activity (onConnect bug workaround)
        mState->connected = true;

        // getValue() returns Arduino String — convert to fl::string immediately
        auto arduinoStr = pChar->getValue();
        if (arduinoStr.length() == 0) return;
        fl::string val(arduinoStr.c_str(), arduinoStr.length());

        FL_WARN("[BLE RX] onWrite len=" << val.size() << ": " << val.c_str());
        int nextHead = (mState->head + 1) % kBleRingBufferSize;
        if (nextHead == mState->tail) {
            FL_WARN("[BLE] Ring buffer full, dropping message");
            return;
        }
        mState->ringBuffer[mState->head] = val;
        mState->head = nextHead;
    }

private:
    BleTransportState* mState;
};

/// @brief BLE TX characteristic callback — logs subscription events for NOTIFY debugging
class BleTxCallback : public BLECharacteristicCallbacks {
public:
    void onSubscribe(BLECharacteristic* pChar, ble_gap_conn_desc* desc,
                     u16 subValue) override {
        (void)pChar;
        (void)desc;
        FL_WARN("[BLE TX] onSubscribe subValue=" << subValue
                << (subValue & 1 ? " NOTIFY" : "")
                << (subValue & 2 ? " INDICATE" : "")
                << (subValue == 0 ? " UNSUBSCRIBED" : ""));
    }

    void onStatus(BLECharacteristic* pChar, Status s, u32 code) override {
        (void)pChar;
        (void)code;
        const char* statusStr = "UNKNOWN";
        switch (s) {
        case SUCCESS_NOTIFY: statusStr = "SUCCESS_NOTIFY"; break;
        case SUCCESS_INDICATE: statusStr = "SUCCESS_INDICATE"; break;
        case ERROR_INDICATE_DISABLED: statusStr = "ERROR_INDICATE_DISABLED"; break;
        case ERROR_NOTIFY_DISABLED: statusStr = "ERROR_NOTIFY_DISABLED"; break;
        case ERROR_GATT: statusStr = "ERROR_GATT"; break;
        case ERROR_NO_CLIENT: statusStr = "ERROR_NO_CLIENT"; break;
        case ERROR_INDICATE_TIMEOUT: statusStr = "ERROR_INDICATE_TIMEOUT"; break;
        case ERROR_INDICATE_FAILURE: statusStr = "ERROR_INDICATE_FAILURE"; break;
        case ERROR_NO_SUBSCRIBER: statusStr = "ERROR_NO_SUBSCRIBER"; break;
        }
        FL_WARN("[BLE TX] onStatus: " << statusStr << " code=" << code);
    }
};

/// @brief BLE server callbacks — tracks connection state
class BleServerCallbacks : public BLEServerCallbacks {
public:
    explicit BleServerCallbacks(BleTransportState* state) : mState(state) {}

    void onConnect(BLEServer* pServer) override {
        mState->connected = true;
        FL_WARN("[BLE] Client connected (1-param)");
    }

    // NimBLE also dispatches the 2-param version
    void onConnect(BLEServer* pServer, ble_gap_conn_desc* desc) override {
        (void)desc;
        mState->connected = true;
        FL_WARN("[BLE] Client connected (2-param)");
    }

    void onDisconnect(BLEServer* pServer) override {
        mState->connected = false;
        FL_WARN("[BLE] Client disconnected — restarting advertising");
        pServer->getAdvertising()->start();
    }

    // NimBLE also dispatches the 2-param version
    void onDisconnect(BLEServer* pServer, ble_gap_conn_desc* desc) override {
        (void)desc;
        mState->connected = false;
        FL_WARN("[BLE] Client disconnected (2-param) — restarting advertising");
        pServer->getAdvertising()->start();
    }

private:
    BleTransportState* mState;
};

BleTransportState* createBleTransport(const char* deviceName) {
    auto uptr = fl::make_unique<BleTransportState>();
    auto* state = uptr.get();

    // Initialize BLE stack
    BLEDevice::init(deviceName);

    // Create GATT server
    state->server = BLEDevice::createServer();
    state->server->setCallbacks(new BleServerCallbacks(state)); // ok bare allocation

    // Create service
    state->service = state->server->createService(FL_BLE_SERVICE_UUID);

    // RX characteristic: host writes JSON-RPC requests here
    // Support both WRITE (with response) and WRITE_NR (without response)
    // because some BLE stacks (e.g. Windows) may prefer one over the other
    BLECharacteristic* rxChar = state->service->createCharacteristic(
        FL_BLE_CHAR_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    rxChar->setCallbacks(new BleRxCallback(state)); // ok bare allocation

    // TX characteristic: device sends JSON-RPC responses via NOTIFY
    state->txChar = state->service->createCharacteristic(
        FL_BLE_CHAR_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    // NimBLE automatically adds BLE2902 (CCCD) descriptor for NOTIFY characteristics
    state->txChar->setCallbacks(new BleTxCallback()); // ok bare allocation

    // Start service and advertising
    state->service->start();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(FL_BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->start();

    FL_WARN("[BLE] GATT server started: name=" << deviceName);
    uptr.release(); // caller owns the state now
    return state;
}

void destroyBleTransport(BleTransportState* state) {
    if (!state) return;
    fl::unique_ptr<BleTransportState> guard(state); // ensure delete on scope exit
    if (state->service) {
        state->service->stop();
        state->service = nullptr;
    }
    if (state->server) {
        state->server->getAdvertising()->stop();
    }
    BLEDevice::deinit(true);
    FL_WARN("[BLE] GATT server stopped");
}

BleStatusInfo queryBleStatus(const BleTransportState* state) {
    BleStatusInfo info;
    if (!state) return info;
    info.connected = state->connected;
    if (state->server) {
        info.connectedCount = static_cast<int>(state->server->getConnectedCount());
    }
    info.txCharExists = (state->txChar != nullptr);
    if (state->txChar) {
        auto val = state->txChar->getValue();
        info.txValueLen = static_cast<int>(val.length());
    }
    info.ringHead = state->head;
    info.ringTail = state->tail;
    return info;
}

fl::pair<fl::function<fl::optional<fl::Json>()>, fl::function<void(const fl::Json&)>>
getBleTransportCallbacks(BleTransportState* state) {
    // RequestSource: polls ring buffer for incoming JSON-RPC
    auto requestSource = [state]() -> fl::optional<fl::Json> {
        if (state->tail == state->head) {
            return fl::nullopt; // Empty
        }

        fl::string& raw = state->ringBuffer[state->tail];
        state->tail = (state->tail + 1) % kBleRingBufferSize;

        FL_WARN("[BLE SRC] raw: " << raw.c_str());

        // Use string_view for zero-copy prefix stripping and trimming
        fl::string_view view = raw;

        // Strip "REMOTE: " prefix if present
        if (view.starts_with("REMOTE: ")) {
            view.remove_prefix(8);
        }

        // Trim whitespace
        while (!view.empty() && fl::isspace(view.front())) {
            view.remove_prefix(1);
        }
        while (!view.empty() && fl::isspace(view.back())) {
            view.remove_suffix(1);
        }

        if (view.empty() || view[0] != '{') {
            FL_WARN("[BLE SRC] rejected (not JSON): " << raw.c_str());
            return fl::nullopt;
        }

        fl::string input(view);
        auto parsed = fl::Json::parse(input);
        if (parsed.has_value()) {
            FL_WARN("[BLE SRC] parsed OK");
        } else {
            FL_WARN("[BLE SRC] parse FAILED");
        }
        return parsed;
    };

    // ResponseSink: sets TX characteristic value and attempts NOTIFY.
    // Due to ESP32-C6 onConnect bug, notify() may silently fail (ERROR_NO_SUBSCRIBER).
    // The host should READ the TX characteristic as a fallback after sending a request.
    auto responseSink = [state](const fl::Json& response) {
        if (!state->txChar) {
            FL_WARN("[BLE TX] no txChar!");
            return;
        }
        fl::string formatted = formatJsonResponse(response, "REMOTE: ");
        FL_WARN("[BLE TX] sending (" << formatted.size() << " bytes): " << formatted.c_str());
        state->txChar->setValue((const u8*)formatted.c_str(), formatted.size()); // ok reinterpret cast
        state->txChar->notify(); // May fail silently on ESP32-C6 (see onConnect bug note)
    };

    return {requestSource, responseSink};
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON && FL_IS_ESP32

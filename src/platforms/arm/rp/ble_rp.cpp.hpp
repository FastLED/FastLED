// IWYU pragma: private

/// @file ble_rp.cpp.hpp
/// @brief Arduino-Pico BTstack BLE GATT transport for RP2350W.

#pragma once

#include "fl/net/ble.h"
#include "platforms/arm/rp/is_rp.h"

#if FL_BLE_AVAILABLE && defined(FL_IS_RP2350)

#include "fl/log/log.h"
#include "fl/remote/transport/serial.h"
#include "fl/stl/cctype.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"
#include "fl/stl/string.h"
#include "fl/stl/string_view.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"

// IWYU pragma: begin_keep
#include <BTstackLib.h>
#include <ble/att_server.h>
// IWYU pragma: end_keep

namespace fl {
namespace net {
namespace ble {

struct TransportState {
    fl::string ring_buffer[8];
    volatile int head = 0;
    volatile int tail = 0;
    u16 tx_handle = 0;
    u16 rx_handle = 0;
    hci_con_handle_t connection = HCI_CON_HANDLE_INVALID;
    bool connected = false;
    fl::string last_tx_value;
};

struct RpBleRuntime {
    TransportState* active = nullptr;
};

RpBleRuntime& rpBleRuntime() FL_NO_EXCEPT {
    return fl::Singleton<RpBleRuntime>::instance();
}

static int nextRingIndex(int index) FL_NO_EXCEPT {
    return (index + 1) % 8;
}

static int onGattWrite(u16, u8* data, u16 size) { // ok no noexcept
    TransportState* state = rpBleRuntime().active;
    if (state == nullptr || data == nullptr || size == 0) {
        return 0;
    }

    const int next_head = nextRingIndex(state->head);
    if (next_head == state->tail) {
        FL_WARN_F("[BLE RP] RX queue full, dropping message");
        return 0;
    }
    fl::string incoming;
    incoming.reserve(size);
    for (u16 i = 0; i < size; ++i) {
        incoming.push_back(static_cast<char>(data[i]));
    }
    state->ring_buffer[state->head] = incoming;
    state->head = next_head;
    return 0;
}

static u16 onGattRead(u16, u8* buffer, u16 buffer_size) { // ok no noexcept
    TransportState* state = rpBleRuntime().active;
    if (state == nullptr || buffer == nullptr) {
        return 0;
    }
    const u16 copy_size = static_cast<u16>(
        state->last_tx_value.size() < buffer_size ? state->last_tx_value.size() : buffer_size);
    for (u16 i = 0; i < copy_size; ++i) {
        buffer[i] = static_cast<u8>(state->last_tx_value[i]);
    }
    return copy_size;
}

static void onConnected(BLEStatus status, BLEDevice* device) { // ok no noexcept
    TransportState* state = rpBleRuntime().active;
    if (state == nullptr || status != BLE_STATUS_OK || device == nullptr) {
        return;
    }
    state->connection = device->getHandle();
    state->connected = true;
}

static void onDisconnected(BLEDevice*) { // ok no noexcept
    TransportState* state = rpBleRuntime().active;
    if (state == nullptr) {
        return;
    }
    state->connection = HCI_CON_HANDLE_INVALID;
    state->connected = false;
}

TransportState* createTransport(const char* device_name) FL_NO_EXCEPT {
    RpBleRuntime& runtime = rpBleRuntime();
    if (runtime.active != nullptr || device_name == nullptr || *device_name == '\0') {
        return nullptr;
    }

    auto holder = fl::make_unique<TransportState>();
    TransportState* state = holder.get();
    UUID service_uuid(FL_BLE_SERVICE_UUID);
    UUID rx_uuid(FL_BLE_CHAR_RX_UUID);
    UUID tx_uuid(FL_BLE_CHAR_TX_UUID);

    BTstack.addGATTService(&service_uuid);
    state->rx_handle = BTstack.addGATTCharacteristicDynamic(
        &rx_uuid, ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE, 1);
    state->tx_handle = BTstack.addGATTCharacteristicDynamic(
        &tx_uuid, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, 2);
    BTstack.setGATTCharacteristicWrite(onGattWrite);
    BTstack.setGATTCharacteristicRead(onGattRead);
    BTstack.setBLEDeviceConnectedCallback(onConnected);
    BTstack.setBLEDeviceDisconnectedCallback(onDisconnected);

    runtime.active = state;
    BTstack.setup(device_name);
    FL_WARN_F("[BLE RP] GATT server started: %s", device_name);
    return holder.release();
}

void destroyTransport(TransportState* state) FL_NO_EXCEPT {
    if (state == nullptr) {
        return;
    }
    if (rpBleRuntime().active == state) {
        rpBleRuntime().active = nullptr;
    }
    fl::unique_ptr<TransportState> holder(state);
}

StatusInfo queryStatus(const TransportState* state) FL_NO_EXCEPT {
    StatusInfo info;
    if (state == nullptr) {
        return info;
    }
    info.connected = state->connected;
    info.connectedCount = state->connected ? 1 : 0;
    info.txCharExists = state->tx_handle != 0;
    info.txValueLen = static_cast<int>(state->last_tx_value.size());
    info.ringHead = state->head;
    info.ringTail = state->tail;
    return info;
}

fl::pair<fl::function<fl::optional<fl::json>()>, fl::function<void(const fl::json&)>>
getTransportCallbacks(TransportState* state) FL_NO_EXCEPT {
    auto request_source = [state]() -> fl::optional<fl::json> {
        if (state == nullptr || state->tail == state->head) {
            return fl::nullopt;
        }
        fl::string raw = state->ring_buffer[state->tail];
        state->tail = nextRingIndex(state->tail);
        fl::string_view view = raw;
        if (view.starts_with("REMOTE: ")) {
            view.remove_prefix(8);
        }
        while (!view.empty() && fl::isspace(view.front())) {
            view.remove_prefix(1);
        }
        while (!view.empty() && fl::isspace(view.back())) {
            view.remove_suffix(1);
        }
        if (view.empty()) {
            return fl::nullopt;
        }
        return fl::json::parse(fl::string(view));
    };

    auto response_sink = [state](const fl::json& response) {
        if (state == nullptr) {
            return;
        }
        state->last_tx_value = formatJsonResponse(response, "REMOTE: ");
        if (!state->connected || state->tx_handle == 0) {
            return;
        }
        fl::vector<u8> payload;
        payload.reserve(state->last_tx_value.size());
        for (char value : state->last_tx_value) {
            payload.push_back(static_cast<u8>(value));
        }
        const u8 result = att_server_notify(
            state->connection, state->tx_handle, payload.data(),
            static_cast<u16>(payload.size()));
        if (result != ERROR_CODE_SUCCESS) {
            FL_WARN_F("[BLE RP] notify failed: %s", result);
        }
    };

    return {request_source, response_sink};
}

} // namespace ble
} // namespace net
} // namespace fl

#endif // FL_BLE_AVAILABLE && FL_IS_RP2350

// platforms/esp/32/drivers/ble/ble_esp32.cpp.hpp
// BLE GATT transport layer — ESP32 NimBLE C API implementation (IDF 5+)
// Included via platforms/esp/32/drivers/ble/_build.cpp.hpp (unity build). Do NOT compile separately.

#pragma once

#include "fl/stl/asio/ble.h"

#if FL_BLE_AVAILABLE

#include "fl/stl/int.h"
#include "fl/system/heap.h"
#include "fl/stl/cstring.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/cctype.h"
#include "fl/stl/string_view.h"
#include "fl/system/log.h"
#include "fl/remote/transport/serial.h" // for formatJsonResponse

// NimBLE C headers (all ESP-IDF, no Arduino dependency)
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

// Ring buffer size for incoming BLE writes (must be power of 2)
static constexpr int kBleRingBufferSize = 8;

/// @brief State shared between BLE callbacks and the main loop
struct BleTransportState {
    // Ring buffer for incoming JSON-RPC requests (written by NimBLE task, read by main loop)
    fl::string ringBuffer[kBleRingBufferSize];
    volatile int head = 0;  // Written by NimBLE task (producer)
    volatile int tail = 0;  // Written by main loop (consumer)

    // NimBLE attribute handles (set during GATT registration)
    u16 txAttrHandle = 0;
    u16 rxAttrHandle = 0;
    u16 connHandle = BLE_HS_CONN_HANDLE_NONE;

    // Connection state
    volatile bool connected = false;

    // Last TX value for READ fallback
    fl::string lastTxValue;
};

// ---------------------------------------------------------------------------
// Forward declarations (C-linkage callbacks must be declared before GATT table)
// ---------------------------------------------------------------------------

static int fl_ble_gatt_access_cb(u16 conn_handle, u16 attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int fl_ble_gap_event_cb(struct ble_gap_event *event, void *arg);
static void fl_ble_on_sync(void);
static void fl_ble_host_task(void *param);

// ---------------------------------------------------------------------------
// Static state pointer — set in createBleTransport, used by on_sync callback
// which has no void* arg parameter.
// ---------------------------------------------------------------------------
static BleTransportState *s_bleState = nullptr;

// ---------------------------------------------------------------------------
// 128-bit UUIDs (NimBLE ble_uuid128_t)
// ---------------------------------------------------------------------------

static const ble_uuid128_t fl_ble_svc_uuid =
    BLE_UUID128_INIT(FL_BLE_SERVICE_UUID_INIT);

static const ble_uuid128_t fl_ble_chr_rx_uuid =
    BLE_UUID128_INIT(FL_BLE_CHAR_RX_UUID_INIT);

static const ble_uuid128_t fl_ble_chr_tx_uuid =
    BLE_UUID128_INIT(FL_BLE_CHAR_TX_UUID_INIT);

// ---------------------------------------------------------------------------
// GATT service definition (static table)
// ---------------------------------------------------------------------------

static const struct ble_gatt_svc_def fl_gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &fl_ble_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX characteristic: host writes JSON-RPC requests here
                .uuid = &fl_ble_chr_rx_uuid.u,
                .access_cb = fl_ble_gatt_access_cb,
                .arg = nullptr, // patched to BleTransportState* at init
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = nullptr, // patched at init
            },
            {
                // TX characteristic: device sends JSON-RPC responses via NOTIFY
                .uuid = &fl_ble_chr_tx_uuid.u,
                .access_cb = fl_ble_gatt_access_cb,
                .arg = nullptr, // patched to BleTransportState* at init
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = nullptr, // patched at init
            },
            { 0 } // terminator
        },
    },
    { 0 } // terminator
};

// Mutable copy for patching arg/val_handle pointers before registration.
// NimBLE reads the table at ble_gatts_count_cfg/add_svcs time, so we copy
// the const template into a mutable array, patch it, then register.
static struct ble_gatt_svc_def fl_gatt_svr_svcs_mut[2];
static struct ble_gatt_chr_def fl_gatt_chr_defs_mut[3];

static void fl_ble_patch_gatt_table(BleTransportState *state) {
    // Copy service table
    fl::memcpy(fl_gatt_svr_svcs_mut, fl_gatt_svr_svcs, sizeof(fl_gatt_svr_svcs));
    // Copy characteristic table
    fl::memcpy(fl_gatt_chr_defs_mut, fl_gatt_svr_svcs[0].characteristics,
               sizeof(fl_gatt_chr_defs_mut));

    // Patch arg pointers to carry state
    fl_gatt_chr_defs_mut[0].arg = state;
    fl_gatt_chr_defs_mut[1].arg = state;

    // Patch val_handle pointers so NimBLE writes back the attribute handles
    fl_gatt_chr_defs_mut[0].val_handle = &state->rxAttrHandle;
    fl_gatt_chr_defs_mut[1].val_handle = &state->txAttrHandle;

    // Point mutable service to mutable characteristics
    fl_gatt_svr_svcs_mut[0].characteristics = fl_gatt_chr_defs_mut;
}

// ---------------------------------------------------------------------------
// GATT access callback — handles RX writes and TX reads
// ---------------------------------------------------------------------------

static int fl_ble_gatt_access_cb(u16 conn_handle, u16 attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    auto *state = static_cast<BleTransportState *>(arg);
    if (!state) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // RX write — extract data from mbuf chain
        // Track connection via write activity (onConnect bug workaround for ESP32-C6)
        state->connected = true;

        u16 om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len == 0) {
            return 0;
        }

        // Flatten mbuf into a contiguous buffer
        fl::string val;
        val.resize(om_len);
        u16 out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, &val[0], om_len, &out_len);
        if (rc != 0) {
            FL_WARN("[BLE RX] mbuf_to_flat failed rc=" << rc);
            return BLE_ATT_ERR_UNLIKELY;
        }
        val.resize(out_len);

        FL_WARN("[BLE RX] onWrite len=" << val.size() << ": " << val.c_str());
        int nextHead = (state->head + 1) % kBleRingBufferSize;
        if (nextHead == state->tail) {
            FL_WARN("[BLE] Ring buffer full, dropping message");
            return 0;
        }
        state->ringBuffer[state->head] = val;
        state->head = nextHead;
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // TX read — return last response value
        if (!state->lastTxValue.empty()) {
            int rc = os_mbuf_append(ctxt->om, state->lastTxValue.c_str(),
                                    state->lastTxValue.size());
            if (rc != 0) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ---------------------------------------------------------------------------
// GAP event callback — handles connect, disconnect, adv complete
// ---------------------------------------------------------------------------

static void fl_ble_start_advertise(BleTransportState *state);

static int fl_ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
    auto *state = static_cast<BleTransportState *>(arg);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            state->connHandle = event->connect.conn_handle;
            state->connected = true;
            FL_WARN("[BLE] Client connected, conn_handle=" << event->connect.conn_handle);
        } else {
            FL_WARN("[BLE] Connection failed, status=" << event->connect.status
                    << " — restarting advertising");
            fl_ble_start_advertise(state);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        state->connected = false;
        state->connHandle = BLE_HS_CONN_HANDLE_NONE;
        FL_WARN("[BLE] Client disconnected, reason=" << event->disconnect.reason
                << " — restarting advertising");
        fl_ble_start_advertise(state);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        // Advertising timed out without connection — restart
        FL_WARN("[BLE] Advertising complete — restarting");
        fl_ble_start_advertise(state);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        FL_WARN("[BLE TX] subscribe event: cur_notify="
                << (int)event->subscribe.cur_notify
                << " cur_indicate=" << (int)event->subscribe.cur_indicate);
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        if (event->notify_tx.status != 0
            && event->notify_tx.status != BLE_HS_EDONE) {
            FL_WARN("[BLE TX] notify failed, status=" << event->notify_tx.status);
        }
        break;

    default:
        break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Advertising
// ---------------------------------------------------------------------------

static void fl_ble_start_advertise(BleTransportState *state) {
    const char *name = ble_svc_gap_device_name();

    // Advertising packet: flags + device name (required for scanner discovery)
    // 128-bit UUID goes in scan response since name+UUID exceed 31-byte ad limit.
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const u8 *)name;
    fields.name_len = (u8)strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        FL_WARN("[BLE] adv_set_fields failed rc=" << rc);
        return;
    }

    // Scan response: 128-bit service UUID (clients discover service after connecting)
    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.uuids128 = &fl_ble_svc_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        FL_WARN("[BLE] adv_rsp_set_fields failed rc=" << rc);
    }

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    u8 addr_type;
    rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        FL_WARN("[BLE] id_infer_auto failed rc=" << rc);
        return;
    }

    rc = ble_gap_adv_start(addr_type, nullptr, BLE_HS_FOREVER, &adv_params,
                           fl_ble_gap_event_cb, state);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        FL_WARN("[BLE] adv_start failed rc=" << rc);
    }
}

// ---------------------------------------------------------------------------
// NimBLE host sync callback — called when NimBLE stack is ready
// ---------------------------------------------------------------------------

static void fl_ble_on_sync(void) {
    // Use best available address
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        FL_WARN("[BLE] ensure_addr failed rc=" << rc);
        return;
    }

    if (s_bleState) {
        fl_ble_start_advertise(s_bleState);
    }
}

// ---------------------------------------------------------------------------
// NimBLE host task — runs the NimBLE event loop on a FreeRTOS task
// ---------------------------------------------------------------------------

static void fl_ble_host_task(void *param) {
    (void)param;
    nimble_port_run(); // Blocks until nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

BleTransportState* createBleTransport(const char* deviceName) {
    auto uptr = fl::make_unique<BleTransportState>();
    auto* state = uptr.get();

    // Set static state pointer for on_sync callback (which has no void* arg)
    s_bleState = state;

    // Initialize NimBLE stack (IDF 5+ API)
    int rc = nimble_port_init();
    if (rc != 0) {
        FL_WARN("[BLE] nimble_port_init failed rc=" << rc);
        s_bleState = nullptr;
        return nullptr;
    }

    // Configure NimBLE host
    ble_hs_cfg.sync_cb = fl_ble_on_sync;

    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Patch GATT table with state pointers and register
    fl_ble_patch_gatt_table(state);
    rc = ble_gatts_count_cfg(fl_gatt_svr_svcs_mut);
    if (rc != 0) {
        FL_WARN("[BLE] gatts_count_cfg failed rc=" << rc);
        nimble_port_deinit();
        s_bleState = nullptr;
        return nullptr;
    }
    rc = ble_gatts_add_svcs(fl_gatt_svr_svcs_mut);
    if (rc != 0) {
        FL_WARN("[BLE] gatts_add_svcs failed rc=" << rc);
        nimble_port_deinit();
        s_bleState = nullptr;
        return nullptr;
    }

    // Set device name
    rc = ble_svc_gap_device_name_set(deviceName);
    if (rc != 0) {
        FL_WARN("[BLE] device_name_set failed rc=" << rc);
    }

    // Start NimBLE host task on FreeRTOS
    nimble_port_freertos_init(fl_ble_host_task);

    FL_WARN("[BLE] GATT server started: name=" << deviceName);
    uptr.release(); // caller owns the state now
    return state;
}

void destroyBleTransport(BleTransportState* state) {
    if (!state) return;
    fl::unique_ptr<BleTransportState> guard(state);

    // Stop advertising
    ble_gap_adv_stop();

    // Signal NimBLE host task to stop
    int rc = nimble_port_stop();
    if (rc != 0) {
        FL_WARN("[BLE] nimble_port_stop failed rc=" << rc);
    }

    // Deinitialize NimBLE stack
    rc = nimble_port_deinit();
    if (rc != 0) {
        FL_WARN("[BLE] nimble_port_deinit failed rc=" << rc);
    }

    s_bleState = nullptr;
    FL_WARN("[BLE] GATT server stopped");
}

BleStatusInfo queryBleStatus(const BleTransportState* state) {
    BleStatusInfo info;
    if (!state) return info;
    info.connected = state->connected;
    info.connectedCount = state->connected ? 1 : 0;
    info.txCharExists = (state->txAttrHandle != 0);
    info.txValueLen = static_cast<int>(state->lastTxValue.size());
    info.ringHead = state->head;
    info.ringTail = state->tail;
    return info;
}

fl::pair<fl::function<fl::optional<fl::json>()>, fl::function<void(const fl::json&)>>
getBleTransportCallbacks(BleTransportState* state) {
    // RequestSource: polls ring buffer for incoming JSON-RPC
    auto requestSource = [state]() -> fl::optional<fl::json> {
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
        auto parsed = fl::json::parse(input);
        if (parsed.has_value()) {
            FL_WARN("[BLE SRC] parsed OK");
        } else {
            FL_WARN("[BLE SRC] parse FAILED");
        }
        return parsed;
    };

    // ResponseSink: sends TX notification and stores value for READ fallback.
    // Due to ESP32-C6 onConnect bug, notify may silently fail.
    // The host should READ the TX characteristic as a fallback.
    auto responseSink = [state](const fl::json& response) {
        fl::string formatted = formatJsonResponse(response, "REMOTE: ");
        FL_WARN("[BLE TX] sending (" << formatted.size() << " bytes): " << formatted.c_str());

        // Store for READ fallback
        state->lastTxValue = formatted;

        if (!state->connected || state->connHandle == BLE_HS_CONN_HANDLE_NONE) {
            FL_WARN("[BLE TX] not connected, skipping notify");
            return;
        }

        // Build mbuf and send notification
        struct os_mbuf *om = ble_hs_mbuf_from_flat(
            formatted.c_str(), static_cast<u16>(formatted.size()));
        if (!om) {
            FL_WARN("[BLE TX] mbuf alloc failed");
            return;
        }

        int rc = ble_gatts_notify_custom(state->connHandle, state->txAttrHandle, om);
        if (rc != 0) {
            FL_WARN("[BLE TX] notify_custom failed rc=" << rc);
        }
    };

    return {requestSource, responseSink};
}

} // namespace fl

#endif // FL_BLE_AVAILABLE

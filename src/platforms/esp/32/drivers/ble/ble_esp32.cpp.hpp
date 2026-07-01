// platforms/esp/32/drivers/ble/ble_esp32.cpp.hpp
// BLE GATT transport layer — ESP32 NimBLE C API implementation (IDF 5+)
// Included via platforms/esp/32/drivers/ble/_build.cpp.hpp (unity build). Do NOT compile separately.
// FL_LINT_ALLOW_GLOBAL_FILE(BLE mutable GATT service/characteristic table copies (fl_gatt_svr_svcs_mut / fl_gatt_chr_defs_mut) are patched once at BLE startup and handed to NimBLE by pointer. Migrating to `fl::Singleton<BleGattState>` is straightforward but deferred to a driver-focused PR so this linter-landing PR stays minimal — tracked under FastLED#3481.)

#pragma once

#include "fl/net/ble.h"

#if FL_BLE_AVAILABLE

#include "fl/stl/int.h"
#include "fl/system/heap.h"
#include "fl/stl/cstring.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/cctype.h"
#include "fl/stl/string_view.h"
#include "fl/log/log.h"
#include "fl/remote/transport/serial.h" // for formatJsonResponse
#include "fl/stl/noexcept.h"

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
namespace net {
namespace ble {

// Ring buffer size for incoming BLE writes (must be power of 2)
static constexpr int kBleRingBufferSize = 8;

/// @brief State shared between BLE callbacks and the main loop
struct TransportState {
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
                                  struct ble_gatt_access_ctxt *ctxt, void *arg); // ok no noexcept
static int fl_ble_gap_event_cb(struct ble_gap_event *event, void *arg); // ok no noexcept
static void fl_ble_on_sync(void); // ok no noexcept
static void fl_ble_host_task(void *param); // ok no noexcept

// ---------------------------------------------------------------------------
// Static state pointer — set in createTransport, used by on_sync callback
// which has no void* arg parameter.
// ---------------------------------------------------------------------------
static TransportState *s_bleState = nullptr; // okay static in header

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
                .arg = nullptr, // patched to TransportState* at init
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = nullptr, // patched at init
            },
            {
                // TX characteristic: device sends JSON-RPC responses via NOTIFY
                .uuid = &fl_ble_chr_tx_uuid.u,
                .access_cb = fl_ble_gatt_access_cb,
                .arg = nullptr, // patched to TransportState* at init
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

static void fl_ble_patch_gatt_table(TransportState *state) FL_NO_EXCEPT {
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

static int fl_ble_gatt_access_cb(u16 conn_handle, u16 attr_handle, // ok no noexcept
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    auto *state = static_cast<TransportState *>(arg);
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
            FL_WARN_F("[BLE RX] mbuf_to_flat failed rc=%s", rc);
            return BLE_ATT_ERR_UNLIKELY;
        }
        val.resize(out_len);

        FL_WARN_F("[BLE RX] onWrite len=%s: %s", val.size(), val.c_str());
        int nextHead = (state->head + 1) % kBleRingBufferSize;
        if (nextHead == state->tail) {
            FL_WARN_F("[BLE] Ring buffer full, dropping message");
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

static void fl_ble_start_advertise(TransportState *state) FL_NO_EXCEPT;

static int fl_ble_gap_event_cb(struct ble_gap_event *event, void *arg) { // ok no noexcept
    auto *state = static_cast<TransportState *>(arg);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            state->connHandle = event->connect.conn_handle;
            state->connected = true;
            FL_WARN_F("[BLE] Client connected, conn_handle=%s", event->connect.conn_handle);
        } else {
            FL_WARN_F("[BLE] Connection failed, status=%s — restarting advertising", event->connect.status);
            fl_ble_start_advertise(state);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        state->connected = false;
        state->connHandle = BLE_HS_CONN_HANDLE_NONE;
        FL_WARN_F("[BLE] Client disconnected, reason=%s — restarting advertising", event->disconnect.reason);
        fl_ble_start_advertise(state);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        // Advertising timed out without connection — restart
        FL_WARN_F("[BLE] Advertising complete — restarting");
        fl_ble_start_advertise(state);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        FL_WARN_F("[BLE TX] subscribe event: cur_notify=%s cur_indicate=%s", (int)event->subscribe.cur_notify, (int)event->subscribe.cur_indicate);
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        if (event->notify_tx.status != 0
            && event->notify_tx.status != BLE_HS_EDONE) {
            FL_WARN_F("[BLE TX] notify failed, status=%s", event->notify_tx.status);
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

static void fl_ble_start_advertise(TransportState *state) FL_NO_EXCEPT {
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
        FL_WARN_F("[BLE] adv_set_fields failed rc=%s", rc);
        return;
    }

    // Scan response: 128-bit service UUID (clients discover service after connecting)
    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.uuids128 = &fl_ble_svc_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        FL_WARN_F("[BLE] adv_rsp_set_fields failed rc=%s", rc);
    }

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    u8 addr_type;
    rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        FL_WARN_F("[BLE] id_infer_auto failed rc=%s", rc);
        return;
    }

    rc = ble_gap_adv_start(addr_type, nullptr, BLE_HS_FOREVER, &adv_params,
                           fl_ble_gap_event_cb, state);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        FL_WARN_F("[BLE] adv_start failed rc=%s", rc);
    }
}

// ---------------------------------------------------------------------------
// NimBLE host sync callback — called when NimBLE stack is ready
// ---------------------------------------------------------------------------

static void fl_ble_on_sync(void) { // ok no noexcept
    // Use best available address
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        FL_WARN_F("[BLE] ensure_addr failed rc=%s", rc);
        return;
    }

    if (s_bleState) {
        fl_ble_start_advertise(s_bleState);
    }
}

// ---------------------------------------------------------------------------
// NimBLE host task — runs the NimBLE event loop on a FreeRTOS task
// ---------------------------------------------------------------------------

static void fl_ble_host_task(void *param) { // ok no noexcept
    (void)param;
    nimble_port_run(); // Blocks until nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

TransportState* createTransport(const char* deviceName) FL_NO_EXCEPT {
    auto uptr = fl::make_unique<TransportState>();
    auto* state = uptr.get();

    // Set static state pointer for on_sync callback (which has no void* arg)
    s_bleState = state;

    // Initialize NimBLE stack (IDF 5+ API)
    int rc = nimble_port_init();
    if (rc != 0) {
        FL_WARN_F("[BLE] nimble_port_init failed rc=%s", rc);
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
        FL_WARN_F("[BLE] gatts_count_cfg failed rc=%s", rc);
        nimble_port_deinit();
        s_bleState = nullptr;
        return nullptr;
    }
    rc = ble_gatts_add_svcs(fl_gatt_svr_svcs_mut);
    if (rc != 0) {
        FL_WARN_F("[BLE] gatts_add_svcs failed rc=%s", rc);
        nimble_port_deinit();
        s_bleState = nullptr;
        return nullptr;
    }

    // Set device name
    rc = ble_svc_gap_device_name_set(deviceName);
    if (rc != 0) {
        FL_WARN_F("[BLE] device_name_set failed rc=%s", rc);
    }

    // Start NimBLE host task on FreeRTOS
    nimble_port_freertos_init(fl_ble_host_task);

    FL_WARN_F("[BLE] GATT server started: name=%s", deviceName);
    uptr.release(); // caller owns the state now
    return state;
}

void destroyTransport(TransportState* state) FL_NO_EXCEPT {
    if (!state) return;
    fl::unique_ptr<TransportState> guard(state); // ok no noexcept

    // Stop advertising
    ble_gap_adv_stop();

    // Signal NimBLE host task to stop
    int rc = nimble_port_stop();
    if (rc != 0) {
        FL_WARN_F("[BLE] nimble_port_stop failed rc=%s", rc);
    }

    // Deinitialize NimBLE stack
    rc = nimble_port_deinit();
    if (rc != 0) {
        FL_WARN_F("[BLE] nimble_port_deinit failed rc=%s", rc);
    }

    s_bleState = nullptr;
    FL_WARN_F("[BLE] GATT server stopped");
}

StatusInfo queryStatus(const TransportState* state) FL_NO_EXCEPT {
    StatusInfo info;
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
getTransportCallbacks(TransportState* state) FL_NO_EXCEPT {
    // RequestSource: polls ring buffer for incoming JSON-RPC
    auto requestSource = [state]() -> fl::optional<fl::json> {
        if (state->tail == state->head) {
            return fl::nullopt; // Empty
        }

        fl::string& raw = state->ringBuffer[state->tail];
        state->tail = (state->tail + 1) % kBleRingBufferSize;

        FL_WARN_F("[BLE SRC] raw: %s", raw.c_str());

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
            FL_WARN_F("[BLE SRC] rejected (not JSON): %s", raw.c_str());
            return fl::nullopt;
        }

        fl::string input(view); // ok no noexcept
        auto parsed = fl::json::parse(input);
        if (parsed.has_value()) {
            FL_WARN_F("[BLE SRC] parsed OK");
        } else {
            FL_WARN_F("[BLE SRC] parse FAILED");
        }
        return parsed;
    };

    // ResponseSink: sends TX notification and stores value for READ fallback.
    // Due to ESP32-C6 onConnect bug, notify may silently fail.
    // The host should READ the TX characteristic as a fallback.
    auto responseSink = [state](const fl::json& response) {
        fl::string formatted = formatJsonResponse(response, "REMOTE: ");
        FL_WARN_F("[BLE TX] sending (%s bytes): %s", formatted.size(), formatted.c_str());

        // Store for READ fallback
        state->lastTxValue = formatted;

        if (!state->connected || state->connHandle == BLE_HS_CONN_HANDLE_NONE) {
            FL_WARN_F("[BLE TX] not connected, skipping notify");
            return;
        }

        // Build mbuf and send notification
        struct os_mbuf *om = ble_hs_mbuf_from_flat(
            formatted.c_str(), static_cast<u16>(formatted.size()));
        if (!om) {
            FL_WARN_F("[BLE TX] mbuf alloc failed");
            return;
        }

        int rc = ble_gatts_notify_custom(state->connHandle, state->txAttrHandle, om);
        if (rc != 0) {
            FL_WARN_F("[BLE TX] notify_custom failed rc=%s", rc);
        }
    };

    return {requestSource, responseSink};
}

} // namespace ble
} // namespace net
} // namespace fl

#endif // FL_BLE_AVAILABLE

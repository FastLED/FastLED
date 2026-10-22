#pragma once

// IWYU pragma: private
// Implementation fragment composed into the isolated embedded-storage object.

#include "fl/stl/shared_ptr.h"
#include "fl/stl/memory.h"
#include "fl/stl/type_traits.h"
// IWYU pragma: begin_keep
#include "esp_littlefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace detail {

// Component versions vary independently of IDF: older configs lack this bit.
template<class Config, class = decltype(fl::declval<Config>().grow_on_mount)>
void enableEmbeddedGrowth(Config& config, int) FL_NO_EXCEPT {
    config.grow_on_mount = true;
}
template<class Config>
void enableEmbeddedGrowth(Config&, ...) FL_NO_EXCEPT {}

inline bool mountNativeEmbedded(bool format) FL_NO_EXCEPT {
    esp_vfs_littlefs_conf_t config = {};
    config.base_path = "/littlefs";
    config.partition_label = "spiffs";
    config.format_if_mount_failed = format;
    enableEmbeddedGrowth(config, 0);
    return esp_vfs_littlefs_register(&config) == ESP_OK;
}
inline void unmountNativeEmbedded() FL_NO_EXCEPT {
    (void)esp_vfs_littlefs_unregister("spiffs");
}


// Count lease objects under one SDK lock, including leases retained by files.
// A weak_ptr alone cannot serialize last-owner unmount with a new mount.
struct EmbeddedMountRegistry {
    SemaphoreHandle_t mutex;
    u32 users;
    bool owned;

    static EmbeddedMountRegistry& instance() FL_NO_EXCEPT;

    bool ensureMutex() FL_NO_EXCEPT {
        static portMUX_TYPE initLock = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&initLock);
        auto existing = mutex;
        portEXIT_CRITICAL(&initLock);
        if (existing) return true;
        auto candidate = xSemaphoreCreateMutex();
        if (!candidate) return false;
        portENTER_CRITICAL(&initLock);
        if (!mutex) {
            mutex = candidate;
            candidate = nullptr;
        }
        portEXIT_CRITICAL(&initLock);
        if (candidate) vSemaphoreDelete(candidate);
        return true;
    }

    bool acquire(bool format) FL_NO_EXCEPT {
        if (!ensureMutex() || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return false;
        bool ready = false;
        if (users != 0xffffffffu) {
            if (users) {
                ready = esp_littlefs_mounted("spiffs");
            } else {
                owned = false;
                if (esp_littlefs_mounted("spiffs")) {
                    ready = true; // Borrow; never unmount another owner's volume.
                } else if (mountNativeEmbedded(format)) {
                    ready = esp_littlefs_mounted("spiffs");
                    owned = ready;
                }
            }
            if (ready) ++users;
        }
        xSemaphoreGive(mutex);
        return ready;
    }

    void release() FL_NO_EXCEPT {
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
        if (users && --users == 0) {
            if (owned) unmountNativeEmbedded();
            owned = false;
        }
        xSemaphoreGive(mutex);
    }
};

class EmbeddedMountLease {
    bool mActive = false;
public:
    ~EmbeddedMountLease() FL_NO_EXCEPT {
        if (mActive) EmbeddedMountRegistry::instance().release();
    }
    bool begin(bool format) FL_NO_EXCEPT {
        if (!mActive) mActive = EmbeddedMountRegistry::instance().acquire(format);
        return mActive;
    }
};
using EmbeddedMountPtr = fl::shared_ptr<EmbeddedMountLease>;

inline EmbeddedMountPtr acquireEmbeddedMount(bool format) FL_NO_EXCEPT {
    auto lease = fl::make_shared<EmbeddedMountLease>();
    if (!lease || !lease->begin(format)) return EmbeddedMountPtr();
    return lease;
}

} // namespace detail
} // namespace platforms
} // namespace fl

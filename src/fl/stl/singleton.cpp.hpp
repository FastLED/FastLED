#include "fl/stl/singleton.h"
#include "fl/stl/cstring.h"
#include "fl/stl/thread_local.h"
#if FASTLED_USE_THREAD_LOCAL
#include "fl/log/log.h"
#include "fl/stl/atomic.h"
#endif
#include "platforms/is_platform.h"  // IWYU pragma: keep
#ifdef FL_IS_ESP8266
#include "fastled_progmem.h"
#endif

namespace fl {
namespace detail {

namespace {
    struct RegistryEntry {
        const char* key;
        void* value;
    };
    static constexpr int REGISTRY_MAX = 128;
    // FL_LINT_ALLOW_GLOBAL(singleton registry backing — cannot itself live inside a Singleton<T>, which would be self-referential)
    static RegistryEntry registry[REGISTRY_MAX];
    // FL_LINT_ALLOW_GLOBAL(singleton registry backing — see above)
    static int registry_count = 0;

    // Keys are FL_PRETTY_FUNCTION strings. The ESP8266 core's linker script
    // places __PRETTY_FUNCTION__ in flash (.irom0.text), which only allows
    // aligned 32-bit loads, so a byte-wise strcmp() faults with
    // LoadStoreError. pgm_read_byte() is safe for both RAM and flash there.
    bool keys_equal(const char* a, const char* b) FL_NO_EXCEPT {
        if (a == b) {
            return true;
        }
#ifdef FL_IS_ESP8266
        for (;; ++a, ++b) {
            const char ca = static_cast<char>(FL_PGM_READ_BYTE_NEAR(a));
            const char cb = static_cast<char>(FL_PGM_READ_BYTE_NEAR(b));
            if (ca != cb) {
                return false;
            }
            if (ca == '\0') {
                return true;
            }
        }
#else
        return fl::strcmp(a, b) == 0;
#endif
    }
} // anonymous namespace

void* singleton_registry_get(const char* key) FL_NO_EXCEPT {
    for (int i = 0; i < registry_count; i++) {
        if (keys_equal(registry[i].key, key)) {
            return registry[i].value;
        }
    }
    return nullptr;
}

void singleton_registry_set(const char* key, void* value) FL_NO_EXCEPT {
    // Check if already registered (update)
    for (int i = 0; i < registry_count; i++) {
        if (keys_equal(registry[i].key, key)) {
            registry[i].value = value;
            return;
        }
    }
    // Add new entry
    if (registry_count < REGISTRY_MAX) {
        registry[registry_count++] = {key, value};
    }
}

#if FASTLED_USE_THREAD_LOCAL
// Declared in thread_local.h. Lives here (with SingletonThreadLocal, the main
// ThreadLocal user) so no new unity TU is needed.
void thread_local_fallback_warn() FL_NO_EXCEPT {
    // Atomic once-flag: several ThreadLocal objects may degrade concurrently.
    static fl::atomic<bool> warned(false);
    if (!warned.exchange(true, fl::memory_order_acq_rel)) {
        FL_WARN("ThreadLocal: pthread_key_create/pthread_setspecific failed; "
                "affected threads share one fallback value");
    }
}
#endif // FASTLED_USE_THREAD_LOCAL

} // namespace detail
} // namespace fl

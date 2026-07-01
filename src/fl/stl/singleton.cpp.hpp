#include "fl/stl/singleton.h"
#include "fl/stl/cstring.h"

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
} // anonymous namespace

void* singleton_registry_get(const char* key) {
    for (int i = 0; i < registry_count; i++) {
        if (fl::strcmp(registry[i].key, key) == 0) {
            return registry[i].value;
        }
    }
    return nullptr;
}

void singleton_registry_set(const char* key, void* value) {
    // Check if already registered (update)
    for (int i = 0; i < registry_count; i++) {
        if (fl::strcmp(registry[i].key, key) == 0) {
            registry[i].value = value;
            return;
        }
    }
    // Add new entry
    if (registry_count < REGISTRY_MAX) {
        registry[registry_count++] = {key, value};
    }
}

} // namespace detail
} // namespace fl

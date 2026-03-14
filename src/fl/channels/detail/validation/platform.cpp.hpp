// src/fl/channels/detail/validation/platform.cpp.hpp
//
// Platform-specific validation implementation
// Queries ChannelManager for registered drivers instead of maintaining a
// hardcoded list.

#include "fl/channels/detail/validation/platform.h"
#include "fl/channels/manager.h"
#include "fl/system/log.h"

namespace fl {
namespace validation {

inline bool validateExpectedEngines() {
    auto infos = channelManager().getDriverInfos();
    return !infos.empty();
}

inline void printEngineValidation() {
    auto infos = channelManager().getDriverInfos();

    if (infos.empty()) {
        FL_ERROR("[VALIDATION] No drivers registered with ChannelManager!");
        return;
    }

    FL_WARN("\n[VALIDATION] Registered drivers: " << infos.size());
    for (fl::size i = 0; i < infos.size(); i++) {
        const auto& info = infos[i];
        FL_WARN("  - " << info.name.c_str()
                        << " (priority=" << info.priority
                        << ", enabled=" << (info.enabled ? "true" : "false")
                        << ")");
    }
    FL_WARN("[VALIDATION] Driver registration OK");
}

}  // namespace validation
}  // namespace fl

#pragma once

/// @file fl/wdt/boot_guard_record.h
/// @brief Platform-neutral record format and policy for the early-boot loop
/// guard (see fl/wdt/boot_guard.h). Everything is always-inline: platforms
/// call it from startup hooks that run before normal code memory is ready.

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"  // FL_ALWAYS_INLINE

namespace fl {

/// @brief Reset-persistent boot-guard record. Platforms place it in memory
/// that survives warm resets (watchdog, software reset) but not power-on.
struct BootGuardRecord {
    fl::u32 magic;
    fl::u32 boots;     ///< consecutive boots that never called markBootHealthy()
    fl::u32 check;     ///< magic ^ ~boots, rejects power-on garbage
    fl::u32 reserved;
};

constexpr fl::u32 kBootGuardMagic = 0xFA57B007u;

/// @brief Boot count stored in `r`, or 0 if the record is not valid.
FL_ALWAYS_INLINE fl::u32 bootGuardDecode(const volatile BootGuardRecord& r) {
    const fl::u32 boots = r.boots;
    if (r.magic != kBootGuardMagic || r.check != (kBootGuardMagic ^ ~boots)) {
        return 0;
    }
    return boots;
}

/// @brief Store `boots` in `r` as a valid record.
FL_ALWAYS_INLINE void bootGuardEncode(volatile BootGuardRecord& r, fl::u32 boots) {
    r.magic = kBootGuardMagic;
    r.boots = boots;
    r.check = kBootGuardMagic ^ ~boots;
    r.reserved = 0;
}

/// @brief What the early-boot hook does on this boot.
struct BootGuardDecision {
    fl::u32 boots;   ///< count to store before continuing
    bool    escape;  ///< reboot into the bootloader now
};

/// @brief Boot-guard policy. `stored_boots` is the count read from the
/// record; `escape_boots` is the threshold (0 disables escaping). On escape
/// the count is zeroed, so the firmware flashed next starts fresh.
FL_ALWAYS_INLINE BootGuardDecision bootGuardOnBoot(fl::u32 stored_boots,
                                                   fl::u32 escape_boots) {
    BootGuardDecision d;
    const fl::u32 boots = stored_boots + 1;
    d.escape = escape_boots != 0 && boots >= escape_boots;
    d.boots = d.escape ? 0 : boots;
    return d;
}

}  // namespace fl

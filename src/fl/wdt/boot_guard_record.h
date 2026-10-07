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
///
/// The record is keyed to the firmware image: a reflash is a warm reset, so
/// the counts of the previous firmware would otherwise carry over and the
/// newly flashed firmware could escape to the bootloader on its first boot.
/// A record written by a different image reads as all-zero counts.
struct BootGuardRecord {
    fl::u32 magic;
    fl::u32 image;     ///< key of the firmware image that wrote the record
    fl::u32 boots;     ///< consecutive boots that never called markBootHealthy()
    fl::u32 crashes;   ///< consecutive watchdog resets (Watchdog::consecutiveCrashCount())
    fl::u32 check;     ///< checksum of the fields above, rejects power-on garbage
    fl::u32 reserved[3];  // pad to one 32-byte cache line
};

constexpr fl::u32 kBootGuardMagic = 0xFA57B008u;

FL_ALWAYS_INLINE fl::u32 bootGuardCheck(fl::u32 image, fl::u32 boots, fl::u32 crashes) {
    return kBootGuardMagic ^ image ^ ~boots ^ ((crashes << 16) | (crashes >> 16)) ^ 0x5A5A5A5Au;
}

/// @brief True when `r` holds a valid record written by firmware `image`.
FL_ALWAYS_INLINE bool bootGuardValid(const volatile BootGuardRecord& r, fl::u32 image) {
    return r.magic == kBootGuardMagic && r.image == image &&
           r.check == bootGuardCheck(image, r.boots, r.crashes);
}

/// @brief Boot count stored in `r`, or 0 if `r` is not a valid record of `image`.
FL_ALWAYS_INLINE fl::u32 bootGuardDecode(const volatile BootGuardRecord& r, fl::u32 image) {
    return bootGuardValid(r, image) ? r.boots : 0;
}

/// @brief Crash count stored in `r`, or 0 if `r` is not a valid record of `image`.
FL_ALWAYS_INLINE fl::u32 bootGuardDecodeCrashes(const volatile BootGuardRecord& r,
                                                fl::u32 image) {
    return bootGuardValid(r, image) ? r.crashes : 0;
}

/// @brief Store both counts in `r` as a valid record of `image`.
FL_ALWAYS_INLINE void bootGuardStore(volatile BootGuardRecord& r, fl::u32 image,
                                     fl::u32 boots, fl::u32 crashes) {
    r.magic = kBootGuardMagic;
    r.image = image;
    r.boots = boots;
    r.crashes = crashes;
    r.check = bootGuardCheck(image, boots, crashes);
    r.reserved[0] = 0;
    r.reserved[1] = 0;
    r.reserved[2] = 0;
}

/// @brief Store `boots`, keeping the crash count (0 if the record is invalid).
/// Always writes: every platform's `watchdogBootGuardWrite()` uses this.
FL_ALWAYS_INLINE void bootGuardEncode(volatile BootGuardRecord& r, fl::u32 image,
                                      fl::u32 boots) {
    bootGuardStore(r, image, boots, bootGuardDecodeCrashes(r, image));
}

/// @brief Store `crashes`, keeping the boot count (0 if the record is invalid).
FL_ALWAYS_INLINE void bootGuardEncodeCrashes(volatile BootGuardRecord& r, fl::u32 image,
                                             fl::u32 crashes) {
    bootGuardStore(r, image, bootGuardDecode(r, image), crashes);
}

/// @brief FNV-1a over every `stride`-th of `count` 32-bit words; platforms
/// hash their firmware image with it to get the record key.
FL_ALWAYS_INLINE fl::u32 bootGuardHashWords(const volatile fl::u32* words, fl::u32 count,
                                            fl::u32 stride = 1) {
    fl::u32 h = 0x811C9DC5u;
    for (fl::u32 i = 0; i < count; i += stride) {
        h = (h ^ words[i]) * 0x01000193u;
    }
    return h;
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

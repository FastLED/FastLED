#pragma once

// IWYU pragma: private, include "fl/wdt/boot_guard.h"

/// @file platforms/arm/mxrt1062/watchdog_boot_guard_mxrt1062.h
/// @brief Teensy 4.x early-boot loop guard (see fl/wdt/boot_guard.h).
///
/// `FL_WATCHDOG_BOOT_GUARD` expands to Teensyduino's weak
/// `startup_early_hook`, which runs before `usb_init()`, static constructors
/// and `setup()`. A hang or fault before `usb_init()` never enumerates: the
/// core's fault handler serves `usb_isr()` for 8 s and reboots, which does
/// nothing before USB is up, so the board loops with no USB device. The hook:
///   - counts boots in an OCRAM record that survives warm resets;
///   - arms WDOG3 (RTWDOG) instead of leaving it off;
///   - on the escape boot, zeroes the count and calls `_reboot_Teensyduino_()`,
///     so the MKL02 starts HalfKay (16c0:0478) and the host can flash it.
///
/// Everything here must be FLASHMEM or always-inline: ITCM, where ordinary
/// code lives, is not initialized yet when the hook runs. The D-cache is not
/// configured yet either, so the record accesses go straight to memory.

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
#include "fl/wdt/boot_guard_record.h"

// IWYU pragma: begin_keep
#include <imxrt.h>         // ok include - WDOG3 / CCM register defines
#include <avr/pgmspace.h>  // ok include - FLASHMEM
// IWYU pragma: end_keep

#define FL_WATCHDOG_HAS_BOOT_GUARD

extern "C" void _reboot_Teensyduino_(void);  // Teensyduino cores/teensy4/usb.c

namespace fl {
namespace platforms {

// The core keeps CrashReport in the top 128 bytes of OCRAM (0x2027FF80, with
// breadcrumbs at 0x2027FFC0; see imxrt.h). The record sits in the 32-byte
// cache line just below it, so flushing it never touches CrashReport's data.
// Like CrashReport, it relies on the heap not reaching the last 160 bytes of
// OCRAM (_heap_end is the end of OCRAM).
constexpr fl::uptr kMxrt1062BootGuardAddr = 0x2027FF60u;

FL_ALWAYS_INLINE volatile BootGuardRecord& mxrt1062BootGuardRecord() {
    return *reinterpret_cast<volatile BootGuardRecord*>(kMxrt1062BootGuardAddr);  // ok reinterpret cast - fixed OCRAM address
}

// Reconfigure WDOG3 before the Arduino runtime is up. `enable` false leaves
// it off (TOVAL max, EN clear). UPDATE stays set so Watchdog::begin() can
// reconfigure it later.
FL_ALWAYS_INLINE void mxrt1062BootGuardConfigureWdog3(bool enable, fl::u32 timeout_ms) {
    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    WDOG3_CNT = 0xD928C520u;  // unlock key
    for (volatile fl::u32 spin = 0; !(WDOG3_CS & WDOG_CS_ULK) && spin < 200000u; ++spin) {
    }

    // LPO with the /256 prescaler: 8 ms per count.
    fl::u32 toval = enable ? timeout_ms / 8u : 0xFFFFu;
    if (toval == 0) toval = 1;
    if (toval > 0xFFFFu) toval = 0xFFFFu;
    WDOG3_TOVAL = toval;
    WDOG3_WIN = 0;
    fl::u32 cs = WDOG_CS_CMD32EN | WDOG_CS_UPDATE | WDOG_CS_CLK(1);
    if (enable) {
        cs |= WDOG_CS_EN | WDOG_CS_PRES;
    }
    WDOG3_CS = cs;
    for (volatile fl::u32 spin = 0; !(WDOG3_CS & WDOG_CS_RCS) && spin < 200000u; ++spin) {
    }
}

FL_ALWAYS_INLINE void mxrt1062BootGuardEarly(fl::u32 escape_boots, fl::u32 timeout_ms) {
    volatile BootGuardRecord& rec = mxrt1062BootGuardRecord();
    const BootGuardDecision d = bootGuardOnBoot(bootGuardDecode(rec), escape_boots);
    bootGuardEncode(rec, d.boots);
    if (d.escape) {
        mxrt1062BootGuardConfigureWdog3(false, 0);  // don't let WDOG3 reset HalfKay
        _reboot_Teensyduino_();                     // bkpt #251: enter HalfKay
    }
    mxrt1062BootGuardConfigureWdog3(true, timeout_ms);
#ifdef FL_WATCHDOG_DEBUG_EARLY_HANG
    // Hardware test: hang before usb_init(), every boot.
    while (true) {
    }
#endif
}

}  // namespace platforms
}  // namespace fl

#define FL_WATCHDOG_BOOT_GUARD_IMPL(escape_boots, early_timeout_ms)          \
    extern "C" FLASHMEM void startup_early_hook(void) {                      \
        ::fl::platforms::mxrt1062BootGuardEarly((escape_boots),              \
                                                (early_timeout_ms));         \
    }                                                                        \
    FL_STATIC_ASSERT(true, "")

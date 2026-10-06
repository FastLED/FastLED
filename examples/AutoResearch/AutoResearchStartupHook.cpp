#include "fl/system/sketch_macros.h"

#if defined(FL_IS_TEENSY_4X) && defined(__IMXRT1062__)

#include <Arduino.h>

// Teensy 4.x boot guard: the bench recovers itself when a firmware hangs or
// crash-loops, including before USB comes up.
//
// startup_early_hook runs before usb_init() (Teensyduino startup.c). A hang
// or fault before then never enumerates: the core's fault handler serves
// usb_isr() for 8 s and then reboots, but before usb_init() that does nothing,
// so the board loops forever without a USB device. The host cannot reach it.
// Recovery therefore has to happen on the chip:
//
//   1. Every boot increments a counter kept in OCRAM, which survives warm
//      resets (watchdog, SYSRESETREQ from the fault handler).
//   2. The RTWDOG is armed here, before anything that can hang, so a pre-USB
//      hang becomes a reset instead of a permanent wedge.
//   3. When the counter reaches kArEscapeBoots, the hook zeroes it and enters
//      the HalfKay bootloader via _reboot_Teensyduino_(). The board then
//      enumerates as 16c0:0478 and the host can flash it.
//   4. The sketch calls autoResearchBootGuardMarkHealthy() once loop() runs,
//      which zeroes the counter. Watchdog resets after that (e.g. the
//      deliberate --watchdog-soak test) never accumulate.
//
// The counter is zeroed before escaping, so the firmware HalfKay flashes next
// always gets a fresh kArEscapeBoots attempts.
//
// Keep the extern "C" hook in this .cpp: Arduino prototype generation can give
// .ino prototypes C++ linkage, which conflicts with the C-linkage weak symbol.
// Everything called from the hook must be FLASHMEM: ITCM is not initialized
// yet when it runs.

extern "C" void _reboot_Teensyduino_(void);  // Teensyduino cores/teensy4/usb.c

namespace {

// The core keeps CrashReport in the top 128 bytes of OCRAM (0x2027FF80, with
// breadcrumbs at 0x2027FFC0; see imxrt.h). The record sits in the 32-byte
// cache line just below it, so flushing it never touches CrashReport's data.
// Like CrashReport, it relies on the heap not reaching the last 160 bytes of
// OCRAM (_heap_end is the end of OCRAM).
struct ArBootRecord {
    uint32_t magic;
    uint32_t boots;   // consecutive boots that never reached loop()
    uint32_t check;   // magic ^ ~boots, rejects power-on garbage
    uint32_t reserved;
};
constexpr uintptr_t kArBootRecordAddr = 0x2027FF60u;
constexpr uint32_t kArBootRecordMagic = 0xFA57B007u;
constexpr uint32_t kArEscapeBoots = 3;
// RTWDOG with LPO /256 prescaler: 8 ms per count. 30 s covers USB bring-up,
// static constructors and setup(), which re-arms with its own timeout.
constexpr uint32_t kArEarlyWatchdogToval = 30000u / 8u;

inline volatile ArBootRecord* arBootRecord() {
    return reinterpret_cast<volatile ArBootRecord*>(kArBootRecordAddr);  // ok reinterpret cast - fixed OCRAM address
}

FLASHMEM void arWriteBootRecord(uint32_t boots) {
    volatile ArBootRecord* r = arBootRecord();
    r->magic = kArBootRecordMagic;
    r->boots = boots;
    r->check = kArBootRecordMagic ^ ~boots;
    r->reserved = 0;
}

FLASHMEM uint32_t arReadBootRecord() {
    volatile ArBootRecord* r = arBootRecord();
    if (r->magic != kArBootRecordMagic || r->check != (kArBootRecordMagic ^ ~r->boots)) {
        return 0;
    }
    return r->boots;
}

// Reconfigure WDOG3. `enable` false leaves it off (TOVAL max, EN clear).
FLASHMEM void arConfigureWdog3(bool enable, uint32_t toval) {
    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    WDOG3_CNT = 0xD928C520u;  // unlock key
    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_ULK)) {
            if (++spin > 200000u) {
                break;
            }
        }
    }

    WDOG3_TOVAL = toval;
    WDOG3_WIN = 0;
    // UPDATE stays set so Watchdog::begin() in setup() can reconfigure it.
    uint32_t cs = WDOG_CS_CMD32EN | WDOG_CS_UPDATE | WDOG_CS_CLK(1);
    if (enable) {
        cs |= WDOG_CS_EN | WDOG_CS_PRES;
    }
    WDOG3_CS = cs;
    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_RCS)) {
            if (++spin > 200000u) {
                break;
            }
        }
    }
}

}  // namespace

extern "C" FLASHMEM void startup_early_hook(void) {
    // The D-cache is not configured yet, so these OCRAM accesses go straight
    // to memory.
    const uint32_t boots = arReadBootRecord() + 1;
    if (boots >= kArEscapeBoots) {
        arWriteBootRecord(0);
        arConfigureWdog3(false, 0xFFFFu);  // don't let the WDT reset HalfKay
        _reboot_Teensyduino_();          // bkpt #251: enter HalfKay
    }
    arWriteBootRecord(boots);
    arConfigureWdog3(true, kArEarlyWatchdogToval);
#ifdef AUTORESEARCH_DEBUG_EARLY_HANG
    // Hardware test for this guard: hang before usb_init(), every boot.
    // The board must come back as 16c0:0478 without a button press.
    while (true) {
    }
#endif
}

// Called from loop(): this boot got USB up and ran setup() to completion.
void autoResearchBootGuardMarkHealthy() {
    arWriteBootRecord(0);
    // OCRAM is write-back cached after configure_cache(); push the record out
    // so a reset right after this still sees it.
    arm_dcache_flush_delete(reinterpret_cast<void*>(kArBootRecordAddr), 32);  // ok reinterpret cast - fixed OCRAM address
}

#else

void autoResearchBootGuardMarkHealthy() {}

#endif

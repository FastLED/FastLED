#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC) && !defined(ARDUINO)

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/led_sysdefs_arm_lpc.h"
#include "platforms/time_platform.h"

namespace fl {
namespace platforms {
namespace lpc_time {

static volatile u32 g_millis = 0;
static bool g_systick_started = false;

static u32 ticks_per_ms() FL_NOEXCEPT {
    return F_CPU / 1000UL;
}

static u32 ticks_per_us() FL_NOEXCEPT {
    return F_CPU / 1000000UL;
}

static void init_systick() FL_NOEXCEPT {
    if (g_systick_started) {
        return;
    }

    const u32 ticks = ticks_per_ms();
    if (ticks == 0 || ticks > 0x1000000UL) {
        return;
    }

    SysTick->LOAD = ticks - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    g_systick_started = true;
}

static u32 current_ms_fraction_us() FL_NOEXCEPT {
    const u32 per_us = ticks_per_us();
    if (!g_systick_started || per_us == 0) {
        return 0;
    }

    const u32 period_ticks = SysTick->LOAD + 1;
    const u32 current_ticks = SysTick->VAL;
    const u32 elapsed_ticks = period_ticks - current_ticks;
    const u32 fraction_us = elapsed_ticks / per_us;
    return fraction_us < 1000UL ? fraction_us : 999UL;
}

}  // namespace lpc_time
}  // namespace platforms
}  // namespace fl

extern "C" void SysTick_Handler(void) {
    ++fl::platforms::lpc_time::g_millis;
}

namespace fl {
namespace platforms {

void delay(u32 ms) FL_NOEXCEPT {
    const u32 start = millis();
    while ((millis() - start) < ms) {
    }
}

void delayMicroseconds(u32 us) FL_NOEXCEPT {
    const u32 start = micros();
    while ((micros() - start) < us) {
    }
}

u32 millis() FL_NOEXCEPT {
    lpc_time::init_systick();
    return lpc_time::g_millis;
}

u32 micros() FL_NOEXCEPT {
    lpc_time::init_systick();

    u32 before;
    u32 fraction;
    u32 after;
    do {
        before = lpc_time::g_millis;
        fraction = lpc_time::current_ms_fraction_us();
        after = lpc_time::g_millis;
    } while (before != after);

    return before * 1000UL + fraction;
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC && !ARDUINO

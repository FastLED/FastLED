#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

// ------------------------------------------------------------------
// SCT match-register slot defaults. The legacy template driver
// (`clockless_arm_lpc_pwm_dma.h`) references these as project-supplied
// `-D` flags without providing fallbacks. The runtime form provides
// defaults that match the documented layout in the header comment block
// (MATCH0 = RISE, MATCH1 = T0_FALL, MATCH2 = T1_FALL, MATCH3 = END) so
// downstream callers don't have to define them.
// ------------------------------------------------------------------
#ifndef FASTLED_LPC_PWM_DMA_BASECH
#define FASTLED_LPC_PWM_DMA_BASECH 0u
#endif
#ifndef FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE 0u
#endif
#ifndef FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL 1u
#endif
#ifndef FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL 2u
#endif
#ifndef FASTLED_LPC_PWM_DMA_SCT_MATCH_END
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_END 3u
#endif

namespace fl {

LpcSctDmaTransmitter::LpcSctDmaTransmitter() FL_NO_EXCEPT {
    // Zero-init the buffer at construction so the first transmit() sees
    // clean state. (Not strictly needed — encode pass overwrites every
    // word — but cheap and defensive.)
    fl::memset(mChannelBuf, 0, sizeof(mChannelBuf));
}

#if defined(FL_IS_ARM_LPC_845)

void LpcSctDmaTransmitter::init() FL_NO_EXCEPT {
    if (mInitialized) {
        return;
    }

    // ----- Power up SCT + DMA in SYSAHBCLKCTRL0 -----
    // Inherits the legacy template's bit assignments verbatim:
    //   bit  8 = SCT clock
    //   bit 29 = DMA clock
    // TODO(2842): verify SYSCON->SYSAHBCLKCTRL0 bit assignments against
    // UM11029 §4.6.13.
    SYSCON->SYSAHBCLKCTRL0 |= (1UL << 8) | (1UL << 29);

    // ----- SCT skeleton -----
    // CONFIG: UNIFY (bit 0) — one 32-bit counter; CLKMODE = SYS clock.
    SCT0->CONFIG = 0x00000001UL;        // UNIFY
    SCT0->CTRL   = 0x00000004UL;        // HALT_L = 1 (paused while configuring)

    // Event bindings (MATCH0..MATCH3 → EV0..EV3, all active in STATE 0).
    // EVx_CTRL bits 0..3 = MATCHSEL, bit 12 = COMBMODE (MATCH only).
    // TODO(2842): verify EVx_CTRL bit layout vs UM11029 §16.6.21.
    for (u32 ev = 0; ev < 4; ++ev) {
        SCT0->EV[ev].STATE = 0x00000001UL;          // active in STATE 0
        SCT0->EV[ev].CTRL  = (ev & 0xF) |
                              (0x1UL << 12);        // COMBMODE = MATCH
    }
    SCT0->LIMIT = (1UL << 3);           // EV3 (T_END) reloads counter

    // SCT events 0/1/2 → DMA0 trigger lines.
    // TODO(2842): verify SCT->DMAREQ0/1 vs UM11029 §16.6.18.
    SCT0->DMAREQ0 = (1UL << 0) | (1UL << 1) | (1UL << 2);
    SCT0->EVEN    = 0xFUL;              // enable EV0..EV3

    // ----- DMA controller -----
    DMA0->CTRL = 1UL;                   // enable controller
    // TODO(2842): allocate aligned descriptor block and wire SRAMBASE.
    // The legacy driver punts to the SDK clock_config.h conventional
    // address; the runtime form inherits the same assumption.

    const u32 base = FASTLED_LPC_PWM_DMA_BASECH;
    for (u32 i = 0; i < 3; ++i) {
        // CFG: PERIPHREQEN + HWTRIGEN, edge-triggered, normal polarity.
        // (@phatpaul caught the missing PERIPHREQEN in FastLED #3349.)
        DMA0->CHANNEL[base + i].CFG =
            (1UL << 0) |                // PERIPHREQEN
            (1UL << 1) |                // HWTRIGEN
            (0UL << 4) |                // TRIGPOL = 0
            (0UL << 5);                 // TRIGTYPE = 0 (edge)
        // TODO(2842): set up DMA_ITRIG_INMUX[i] = SCT_DMA0_inmux_source.
    }
    DMA0->COMMON[0].ENABLESET |= (7UL << base);     // 3 contiguous channels

    mInitialized = true;
}

void LpcSctDmaTransmitter::configureForChannel(
        u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT {
    init();

    // GPIO port-0 mask. Per the legacy driver, the SCT is not routed to
    // the data pin via the SCTOUT mux — the DMA writes the configured
    // bitmask to LPC_GPIO->SET[0] / CLR[0] at SCT match time, so any
    // PIO0_n pin works.
    mPinMask = (1UL << pin);

    // Compute SCT tick counts at the active F_CPU.
    mT0Rise = 0;
    mT0Fall = lpc_sct_ticks(t1_ns);
    mT1Fall = lpc_sct_ticks(t1_ns + t2_ns);
    mBitEnd = lpc_sct_ticks(t1_ns + t2_ns + t3_ns);

    // Latch into SCT MATCH + MATCHREL.
    SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]      = mT0Rise;
    SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL]    = mT0Fall;
    SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL]    = mT1Fall;
    SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]       = mBitEnd;
    SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]   = mT0Rise;
    SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL] = mT0Fall;
    SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL] = mT1Fall;
    SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]    = mBitEnd;
}

namespace {

// Encode one chunk of WS2812 bits into the three stream slices.
//   rise_stream[i]    = pin_mask  for every bit  (line goes HIGH at T0_RISE)
//   t0fall_stream[i]  = pin_mask if the source bit is '0', else 0
//   t1fall_stream[i]  = pin_mask if the source bit is '1', else 0
// A DMA write of 0 to LPC_GPIO->CLR[0] is a documented no-op
// (UM11029 §12.4.2.6), so the "skip this bit" entries are zero-cost
// stream filler.
void encodeChunkFromBytes(const u8* bytes, u32 bytes_avail,
                          u32 chunk_bits, u32 pin_mask,
                          u32* rise_stream,
                          u32* t0fall_stream,
                          u32* t1fall_stream) FL_NO_EXCEPT {
    for (u32 i = 0; i < chunk_bits; ++i) {
        rise_stream[i] = pin_mask;
    }

    u32 bit_idx = 0;
    u32 byte_idx = 0;
    while (bit_idx < chunk_bits && byte_idx < bytes_avail) {
        const u8 byte_value = bytes[byte_idx++];
        // MSB first (WS2812 standard).
        for (u8 b = 0; b < 8; ++b) {
            const u32 dst = bit_idx + b;
            if (dst >= chunk_bits) break;
            const bool bit = (byte_value & (0x80u >> b)) != 0;
            t0fall_stream[dst] = bit ? 0u : pin_mask;
            t1fall_stream[dst] = bit ? pin_mask : 0u;
        }
        bit_idx += 8;
    }

    // Pad tail of chunk with no-op CLR words.
    for (; bit_idx < chunk_bits; ++bit_idx) {
        t0fall_stream[bit_idx] = 0u;
        t1fall_stream[bit_idx] = 0u;
    }
}

}  // namespace

void LpcSctDmaTransmitter::transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT {
    if (bytes == nullptr || len == 0u) {
        return;
    }
    (void)is_rgbw;  // bytes are already chipset-encoded by ChannelData; the
                    // is_rgbw flag is reserved for future per-pixel padding
                    // logic (RGBW-XTRA0). For now the byte stream is consumed
                    // verbatim.

    constexpr u32 chunk_words = FASTLED_LPC_PWM_DMA_CHUNK_BITS;
    constexpr u32 xfercfg_base =
        (1UL << 0) |                            // CFGVALID
        (2UL << 8) |                            // WIDTH = 32-bit
        (1UL << 12) |                           // SRCINC = +1 word
        (0UL << 14);                            // DSTINC = no
    const u32 base = FASTLED_LPC_PWM_DMA_BASECH;

    u32* rise_stream   = &mChannelBuf[0];
    u32* t0fall_stream = &mChannelBuf[chunk_words];
    u32* t1fall_stream = &mChannelBuf[2u * chunk_words];

    u32 byte_offset = 0;
    while (byte_offset < len) {
        const u32 bytes_remaining = len - byte_offset;
        const u32 bits_remaining = bytes_remaining * 8u;
        const u32 chunk_bits = bits_remaining > chunk_words ? chunk_words : bits_remaining;

        encodeChunkFromBytes(bytes + byte_offset, bytes_remaining,
                              chunk_bits, mPinMask,
                              rise_stream, t0fall_stream, t1fall_stream);

        // Arm the 3 DMA channels for this chunk.
        const u32 count_field = ((chunk_bits - 1u) & 0x3FFu) << 16;
        for (u32 i = 0; i < 3; ++i) {
            DMA0->CHANNEL[base + i].XFERCFG = xfercfg_base | count_field;
        }
        DMA0->COMMON[0].SETVALID = (7UL << base);

        // Release HALT — SCT runs, match events fire, DMA channels stream.
        SCT0->CTRL &= ~0x00000004UL;

        // Block until the 3 channels complete this chunk. TODO(2842):
        // wire SCT MATCH_END → NVIC IRQn so this becomes a WFI rather
        // than a busy-wait — same TODO the legacy template carries.
        const u32 mask = (7UL << base);
        while ((DMA0->COMMON[0].ACTIVE & mask) != 0u) {
            // busy
        }
        SCT0->CTRL |= 0x00000004UL;     // halt for next chunk

        byte_offset += chunk_bits / 8u;
    }
}

bool LpcSctDmaTransmitter::isDone() const FL_NO_EXCEPT {
    const u32 mask = (7UL << FASTLED_LPC_PWM_DMA_BASECH);
    return (DMA0->COMMON[0].ACTIVE & mask) == 0u;
}

#else  // host / stub build

// Host-mode stubs — methods are no-ops. The channels-API engine can
// hold an instance and call into these without any peripheral access,
// preserving the state-machine behavior verified by
// `tests/fl/channels/lpc_sct_dma_engine.cpp`.

void LpcSctDmaTransmitter::init() FL_NO_EXCEPT {
    mInitialized = true;
}

void LpcSctDmaTransmitter::configureForChannel(
        u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT {
    init();
    mPinMask = (1UL << pin);
    mT0Rise = 0;
    mT0Fall = lpc_sct_ticks(t1_ns);
    mT1Fall = lpc_sct_ticks(t1_ns + t2_ns);
    mBitEnd = lpc_sct_ticks(t1_ns + t2_ns + t3_ns);
}

void LpcSctDmaTransmitter::transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT {
    (void)bytes; (void)len; (void)is_rgbw;
    // No peripheral on host. The channels-API engine's poll() flips
    // back to READY on the next call — same shape the scaffold had.
}

bool LpcSctDmaTransmitter::isDone() const FL_NO_EXCEPT {
    return true;  // host has no real DMA stream to wait on.
}

#endif  // FL_IS_ARM_LPC_845

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL

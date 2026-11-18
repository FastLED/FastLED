#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"
#include "platforms/arm/lpc/lpc_dma_descriptor_table.h"
#include "platforms/arm/lpc/lpc_dma_isr.h"   // LPC845-gated inside
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

namespace fl {

LpcSctDmaTransmitter::LpcSctDmaTransmitter() FL_NO_EXCEPT {
    fl::memset(mChunkBuf, 0, sizeof(mChunkBuf));
}

u32 LpcSctDmaTransmitter::encodeChunk(u32 buf) FL_NO_EXCEPT {
    const u32 left = mTotalBits - mBitsEncoded;
    const u32 n = left > FASTLED_LPC_PWM_DMA_CHUNK_BITS
                      ? FASTLED_LPC_PWM_DMA_CHUNK_BITS
                      : left;
    lpcSctDmaEncodeClrMasks(mCurrentBytes, mBitsEncoded, n, mChunkBuf[buf]);
    mBitsEncoded += n;
    return n;
}

#if defined(FL_IS_ARM_LPC_845)

// The unity build puts this file in one TU with the SCT RX capture and the
// CTIMER capture, so these names carry a kTx / lpcSctTx prefix.

constexpr u32 kTxDmaCh = FASTLED_LPC_PWM_DMA_BASECH;
FL_STATIC_ASSERT(kTxDmaCh < 25u, "FASTLED_LPC_PWM_DMA_BASECH must be a DMA0 channel (0..24)");

// SCT events (UM11029 ch. 21). EVn uses MATCHn; all live in state 0.
constexpr u32 kTxEvRise = 0u;     // MATCH0: output SET
constexpr u32 kTxEvT0h  = 1u;     // MATCH1: '0' bit falls
constexpr u32 kTxEvT1h  = 2u;     // MATCH2: every bit has fallen; DMA trigger
constexpr u32 kTxEvEnd  = 3u;     // MATCH3: counter limit (bit period)
constexpr u32 kTxEvCtrlMatchOnly = (1u << 12);   // COMBMODE = MATCH
constexpr u32 kTxSctOut = 0u;                    // SCT output 0 -> data pin

// INPUTMUX DMA_ITRIG_INMUX source SCT_DMA0 (UM11029 Table 288).
constexpr u32 kTxItrigSctDma0 = 0x2u;

// DMA channel CFG: hardware trigger on the rising edge of SCT_DMA0, one
// transfer per trigger (TRIGBURST, BURSTPOWER = 0). No PERIPHREQEN: the
// channel's fixed peripheral request is unrelated, and with it set the
// channel waits on that peripheral forever.
constexpr u32 kTxDmaCfg = (1u << 1)     // HWTRIGEN
                        | (1u << 4)     // TRIGPOL = rising
                        | (1u << 6);    // TRIGBURST

// XFERCFG fields (UM11029 Table 321).
constexpr u32 kTxXferValid  = (1u << 0);
constexpr u32 kTxXferReload = (1u << 1);
constexpr u32 kTxXferSetIntA = (1u << 4);
constexpr u32 kTxXferWidth32 = (2u << 8);
constexpr u32 kTxXferSrcInc1 = (1u << 12);
constexpr u32 kTxXferCount(u32 n) { return ((n - 1u) & 0x3FFu) << 16; }

// Written into SCT0->CTRL by the last transfer of each chunk: HALT_L, and
// HALT_H as at reset. PRE = 0 and BIDIR = 0 match init().
constexpr u32 kTxSctCtrlHalt = SCT_CTRL_HALT_L_MASK | SCT_CTRL_HALT_H_MASK;

// Linked descriptors need 16-byte alignment and a fixed address, so they
// are function-local statics (one SCT, one transmitter; unused builds drop
// them with the function).
inline volatile fl::u32& lpcSctTxHaltWord() FL_NO_EXCEPT {
    static volatile fl::u32 word = kTxSctCtrlHalt;
    return word;
}
// The two ping-pong slots: chunk k >= 1 runs from slot k & 1, and each
// slot links to the other.
inline volatile fl::lpc::DmaChannelDescriptor* lpcSctTxSlots() FL_NO_EXCEPT {
    static volatile fl::lpc::DmaChannelDescriptor slots[2];
    return slots;
}

inline fl::u32 lpcSctTxAddr(const volatile void* p) FL_NO_EXCEPT {
    return reinterpret_cast<fl::u32>(p);  // ok reinterpret cast - DMA descriptor addresses
}

inline volatile fl::lpc::DmaChannelDescriptor& lpcSctTxChannelDesc() FL_NO_EXCEPT {
    return reinterpret_cast<volatile fl::lpc::DmaChannelDescriptor*>(  // ok reinterpret cast - DMA descriptor table base
        DMA0->SRAMBASE)[kTxDmaCh];
}

void LpcSctDmaTransmitter::init() FL_NO_EXCEPT {
    if (mInitialized) {
        return;
    }
    SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_SCT_MASK |
                              SYSCON_SYSAHBCLKCTRL0_DMA_MASK |
                              SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
    // Reset only the SCT, which this transmitter owns. DMA0 and SWM are
    // shared with the console, the UART-DMA driver and every movable pin;
    // resetting them breaks those (#4676). The SCT reset also drops any DMA
    // request a previous SCT user left pending.
    SYSCON->PRESETCTRL0 &= ~SYSCON_PRESETCTRL0_SCT_RST_N_MASK;
    SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_SCT_RST_N_MASK;

    DMA0->CTRL = 1u;
    fl::lpc::ensureDmaSramBase();

    // Each chunk's descriptor raises INTA when it runs out. With the DMA ISR
    // hub (on by default with the LPC845 UART-DMA driver) the freed buffer
    // is refilled from the interrupt whether or not anyone calls poll().
    // The transmitter lives inside the engine singleton, so `this` outlives
    // the registration.
    fl::lpc::registerDmaChannelIsr(kTxDmaCh, &LpcSctDmaTransmitter::onDmaIsr, this);

    mInitialized = true;
}

// Every frame re-applies the SCT and DMA channel setup: other code (the SCT
// RX capture, another DMA user) may have used them since the last frame.
inline void lpcSctTxApplySetup() FL_NO_EXCEPT {
    SCT0->CONFIG = SCT_CONFIG_UNIFY_MASK;       // one 32-bit counter, bus clock
    SCT0->CTRL = kTxSctCtrlHalt | SCT_CTRL_CLRCTR_L_MASK;
    // Every slot back to match mode while halted: the SCT RX capture leaves
    // slots 0 and 1 in capture mode, and EV0/EV1 need them as matches.
    SCT0->REGMODE = 0u;
    for (u32 ev = 0; ev < 4u; ++ev) {
        SCT0->EV[ev].STATE = 1u;                  // active in state 0
        SCT0->EV[ev].CTRL = ev | kTxEvCtrlMatchOnly;  // MATCHSEL = ev
    }
    SCT0->LIMIT = (1u << kTxEvEnd);
    SCT0->OUT[kTxSctOut].SET = (1u << kTxEvRise);
    SCT0->OUT[kTxSctOut].CLR = (1u << kTxEvT1h);
// Arduino's OUTPUT macro would rewrite the SCT register name.
#pragma push_macro("OUTPUT")
#undef OUTPUT
    SCT0->OUTPUT &= ~(1u << kTxSctOut);              // idle low
#pragma pop_macro("OUTPUT")
    SCT0->EVEN = 0u;                              // no interrupts
    SCT0->DMAREQ0 = (1u << kTxEvT1h);
    SCT0->DMAREQ1 = 0u;

    DMA0->CTRL = 1u;
    DMA0->COMMON[0].ENABLECLR = (1u << kTxDmaCh);
    INPUTMUX->DMA_ITRIG_INMUX[kTxDmaCh] = kTxItrigSctDma0;
    DMA0->CHANNEL[kTxDmaCh].CFG = kTxDmaCfg;
}

void LpcSctDmaTransmitter::configureForChannel(
        u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT {
    init();
    abort();   // never reconfigure under a running frame
    lpcSctTxApplySetup();
    // Route SCT_OUT0 to the data pin (SWM PINASSIGN7[31:24]) on every frame,
    // in case something else re-muxed it. Only that byte changes; a previous
    // pin goes back to GPIO.
    SWM0->PINASSIGN.PINASSIGN7 =
        (SWM0->PINASSIGN.PINASSIGN7 & ~SWM_PINASSIGN7_SCT_OUT0_O_MASK) |
        SWM_PINASSIGN7_SCT_OUT0_O(pin);
    mPin = pin;
    mT0h = lpc_sct_ticks(t1_ns);
    mT1h = lpc_sct_ticks(t1_ns + t2_ns);
    mPeriod = lpc_sct_ticks(t1_ns + t2_ns + t3_ns);
    // The rise sits one tick into the period so it never coincides with
    // the limit reload. MATCHREL keeps the values across limit events.
    const u32 match[4] = {1u, 1u + mT0h, 1u + mT1h, mPeriod - 1u};
    for (u32 i = 0; i < 4u; ++i) {
        SCT0->MATCH[i] = match[i];
        SCT0->MATCHREL[i] = match[i];
    }
}

// Make `slot` the halt step: one transfer that writes HALT into SCT CTRL
// on the next EV2.
inline void lpcSctTxWriteHaltSlot(volatile fl::lpc::DmaChannelDescriptor& slot) FL_NO_EXCEPT {
    slot.link = 0u;
    slot.dst_end = lpcSctTxAddr(&SCT0->CTRL);
    slot.src_end = lpcSctTxAddr(&lpcSctTxHaltWord());
    slot.reserved = kTxXferValid | kTxXferWidth32 | kTxXferCount(1u);
}

// Only a chunk with a successor needs its INTA (to refill the slot after
// it); the frame's last chunk raises none, which also keeps the DMA ISR off
// the bus while the frame's last bits go out.
constexpr u32 lpcSctTxChunkIntA(bool last) {
    return last ? 0u : kTxXferSetIntA;
}

// Make `slot` play `nbits` masks from `masks`, then go on to `next`.
inline void lpcSctTxWriteChunkSlot(volatile fl::lpc::DmaChannelDescriptor& slot,
                                   const u32* masks, u32 nbits, bool last,
                                   volatile fl::lpc::DmaChannelDescriptor& next) FL_NO_EXCEPT {
    slot.link = lpcSctTxAddr(&next);
    slot.dst_end = lpcSctTxAddr(&SCT0->OUT[kTxSctOut].CLR);
    slot.src_end = lpcSctTxAddr(&masks[nbits - 1u]);
    slot.reserved = kTxXferValid | kTxXferReload | lpcSctTxChunkIntA(last) |
                    kTxXferWidth32 | kTxXferSrcInc1 | kTxXferCount(nbits);
}

// UM11029 16.6.15: disable, wait for the channel's pipeline to drain, then
// abort. Shared with the CTIMER capture.
inline void lpcDmaStopChannel(u32 ch) FL_NO_EXCEPT {
    DMA0->COMMON[0].ENABLECLR = (1u << ch);
    for (u32 i = 0; i < 1000u && (DMA0->COMMON[0].BUSY & (1u << ch)); ++i) {
    }
    DMA0->COMMON[0].ABORT = (1u << ch);
}

void LpcSctDmaTransmitter::abort() FL_NO_EXCEPT {
    if (!mInitialized) {
        return;   // never touched the SCT or the DMA channel
    }
    const u32 primask = __get_PRIMASK();
    __disable_irq();   // the chunk ISR reads the state cleared here
    if (mTransmitInProgress || !isStopped()) {
        SCT0->CTRL = kTxSctCtrlHalt;
        lpcDmaStopChannel(kTxDmaCh);
        DMA0->COMMON[0].INTA = (1u << kTxDmaCh);   // W1C a completion we dropped
// A frame cut mid-bit would leave the line high; drive it low (Arduino's
// OUTPUT macro would rewrite the SCT register name).
#pragma push_macro("OUTPUT")
#undef OUTPUT
        SCT0->OUTPUT &= ~(1u << kTxSctOut);
#pragma pop_macro("OUTPUT")
        mFrameComplete = false;   // cut off before its last bit
    }
    mTransmitInProgress = false;
    mCurrentBytes = nullptr;
    __set_PRIMASK(primask);
}

void LpcSctDmaTransmitter::transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT {
    (void)is_rgbw;  // ChannelData bytes are already in wire order.
    if (!mInitialized) {
        return;
    }
    // A frame still on the wire (the caller gave up waiting) is cut off
    // before its descriptors and buffers are reused.
    abort();
    if (bytes == nullptr || len == 0u) {
        return;
    }
    mCurrentBytes = bytes;
    mTotalBits = len * 8u;
    mBitsEncoded = 0;
    mFrameComplete = false;

    // Chunk 0 runs from the channel's own descriptor; its first mask goes
    // into OUT0.CLR by hand, and EV2 of each bit makes the DMA load the
    // next bit's mask. Chunk 1 is encoded up front into slot 1; slot 0 is
    // the halt step until the ISR refills it. So nothing is encoded with
    // interrupts off while a chunk plays.
    volatile fl::lpc::DmaChannelDescriptor* slots = lpcSctTxSlots();
    const u32 n0 = encodeChunk(0);   // >= 8: whole bytes, CHUNK_BITS >= 8
    const u32 n1 = encodeChunk(1);
    lpcSctTxWriteHaltSlot(slots[0]);
    if (n1 > 0u) {
        lpcSctTxWriteChunkSlot(slots[1], mChunkBuf[1], n1,
                               mBitsEncoded == mTotalBits, slots[0]);
    }
    volatile fl::lpc::DmaChannelDescriptor& d = lpcSctTxChannelDesc();
    d.src_end = lpcSctTxAddr(&mChunkBuf[0][n0 - 1u]);
    d.dst_end = lpcSctTxAddr(&SCT0->OUT[kTxSctOut].CLR);
    d.link = lpcSctTxAddr(n1 > 0u ? &slots[1] : &slots[0]);
    mBitsQueued = n0 + n1;
    mPlayingChunk = 0;
    mLastChunk = (mTotalBits + FASTLED_LPC_PWM_DMA_CHUNK_BITS - 1u) /
                     FASTLED_LPC_PWM_DMA_CHUNK_BITS - 1u;

    const u32 primask = __get_PRIMASK();
    __disable_irq();   // the chunk ISR must see a complete state
    SCT0->CTRL = kTxSctCtrlHalt | SCT_CTRL_CLRCTR_L_MASK;
    SCT0->OUT[kTxSctOut].CLR = mChunkBuf[0][0];
    DMA0->COMMON[0].INTA = (1u << kTxDmaCh);
    (void)fl::lpc::takeDmaChannelError(kTxDmaCh);   // drop a stale error
    DMA0->CHANNEL[kTxDmaCh].XFERCFG = kTxXferValid | kTxXferReload |
                                      lpcSctTxChunkIntA(n1 == 0u) |
                                      kTxXferWidth32 | kTxXferSrcInc1 |
                                      kTxXferCount(n0 - 1u);
    DMA0->COMMON[0].SETVALID = (1u << kTxDmaCh);
    DMA0->COMMON[0].ENABLESET = (1u << kTxDmaCh);
    SCT0->CTRL = SCT_CTRL_HALT_H_MASK;   // release the unified counter
    mTransmitInProgress = true;
    __set_PRIMASK(primask);
}

// Transfers the playing descriptor has left (XFERCOUNT counts down to 0,
// then wraps to 0x3FF when the descriptor runs out).
inline u32 lpcSctTxTransfersLeft() FL_NO_EXCEPT {
    const u32 residual = (DMA0->CHANNEL[kTxDmaCh].XFERCFG >> 16) & 0x3FFu;
    return residual == 0x3FFu ? 0u : residual + 1u;
}

// A slot is rewritten only while the playing chunk has at least this many
// bits to go (5 us at WS2812 rates), so the DMA never reloads it mid-write.
constexpr u32 kTxRefillMarginBits = 4u;

void LpcSctDmaTransmitter::onChunkEnd() FL_NO_EXCEPT {
    if (!mTransmitInProgress || mPlayingChunk >= mLastChunk) {
        return;   // the halt step is running; isStopped() ends the frame
    }
    // Chunk j ran out and chunk j + 1 was loaded from slot (j + 1) & 1; it
    // goes on to slot j & 1. Slot (j + 1) & 1 has been read, so it becomes
    // the halt step at once: if the next refill comes late, the frame ends
    // there instead of replaying an old chunk. Slot j & 1 has been the halt
    // step since the previous call (or transmit()); refill it with chunk
    // j + 2 if chunk j + 1 still has time left, else let the frame end.
    // Each call handles one INTA; if two chunk ends were merged, chunk j + 1
    // has already run into the halt step and the margin check fails.
    const u32 j = mPlayingChunk;
    mPlayingChunk = j + 1u;
    volatile fl::lpc::DmaChannelDescriptor* slots = lpcSctTxSlots();
    const u32 freed = j & 1u;
    lpcSctTxWriteHaltSlot(slots[freed ^ 1u]);
    const u32 n = encodeChunk(freed);   // buffer j & 1 is free either way
    if (n == 0u) {
        return;   // chunk j + 1 is the last; slot j & 1 stays the halt step
    }
    const u32 primask = __get_PRIMASK();
    __disable_irq();
    if ((DMA0->COMMON[0].INTA & (1u << kTxDmaCh)) == 0u &&
        lpcSctTxTransfersLeft() >= kTxRefillMarginBits) {
        lpcSctTxWriteChunkSlot(slots[freed], mChunkBuf[freed], n,
                               mBitsEncoded == mTotalBits, slots[freed ^ 1u]);
        mBitsQueued += n;
    }
    __set_PRIMASK(primask);
}

void LpcSctDmaTransmitter::onDmaIsr(void* ctx) FL_NO_EXCEPT {
    static_cast<LpcSctDmaTransmitter*>(ctx)->onChunkEnd();   // hub cleared INTA
}

bool LpcSctDmaTransmitter::pollAndAdvance() FL_NO_EXCEPT {
    // Builds without the DMA ISR hub refill from here; with it, the ISR
    // usually has already. Interrupts are off, so only one context sees
    // each INTA.
    const u32 primask = __get_PRIMASK();
    __disable_irq();
    if (mTransmitInProgress && fl::lpc::takeDmaChannelError(kTxDmaCh)) {
        // The descriptor chain stopped short of the halt step, so the SCT
        // would never halt: end the frame here. With the ISR hub on, the
        // hub has already cleared ERRINT and latched it for us.
        abort();   // leaves mFrameComplete false
    }
    if (mTransmitInProgress && (DMA0->COMMON[0].INTA & (1u << kTxDmaCh)) != 0u) {
        DMA0->COMMON[0].INTA = (1u << kTxDmaCh);
        onChunkEnd();
    }
    if (mTransmitInProgress && isStopped()) {
        // The halt step ran: normally after the last chunk, or early if a
        // refill came later than one chunk time (frame cut short).
        mFrameComplete = (mBitsQueued == mTotalBits);
        mTransmitInProgress = false;
        mCurrentBytes = nullptr;
    }
    const bool done = !mTransmitInProgress;
    __set_PRIMASK(primask);
    return done;
}

bool LpcSctDmaTransmitter::isStopped() const FL_NO_EXCEPT {
    return (DMA0->COMMON[0].ACTIVE & (1u << kTxDmaCh)) == 0u &&
           (SCT0->CTRL & SCT_CTRL_HALT_L_MASK) != 0u;
}

bool LpcSctDmaTransmitter::isDone() const FL_NO_EXCEPT {
    return !mTransmitInProgress && isStopped();
}

#else  // host / stub build

// Host-mode stubs — no peripheral access. The channels-API engine can
// hold an instance and call into these, preserving the state-machine
// behavior verified by
// `tests/platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.cpp`.

void LpcSctDmaTransmitter::init() FL_NO_EXCEPT {
    mInitialized = true;
}

void LpcSctDmaTransmitter::configureForChannel(
        u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT {
    init();
    mPin = pin;
    mT0h = lpc_sct_ticks(t1_ns);
    mT1h = lpc_sct_ticks(t1_ns + t2_ns);
    mPeriod = lpc_sct_ticks(t1_ns + t2_ns + t3_ns);
}

void LpcSctDmaTransmitter::transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT {
    (void)is_rgbw;
    // No SCT/DMA peripheral on host. Instead, capture the byte stream
    // so the caller can round-trip it through the LPC RX device's
    // decoder (#3468 TX→RX readback contract). The channels-API
    // engine's poll() flips back to READY on the next call.
    //
    // Overwrite semantics: each transmit() replaces the prior capture.
    // Callers that need to accumulate across frames should copy the
    // span out between calls.
    mTxCapture.clear();
    mCurrentBytes = nullptr;
    mTotalBits = 0;
    mBitsEncoded = 0;
    mTransmitInProgress = false;  // host completes synchronously
    mFrameComplete = false;
    if (bytes == nullptr || len == 0u) {
        return;
    }
    mFrameComplete = true;
    mTxCapture.reserve(len);
    for (u32 i = 0; i < len; ++i) {
        mTxCapture.push_back(bytes[i]);
    }
}

bool LpcSctDmaTransmitter::pollAndAdvance() FL_NO_EXCEPT {
    return true;  // host completes transmit() synchronously.
}

void LpcSctDmaTransmitter::abort() FL_NO_EXCEPT {
    mTransmitInProgress = false;
    mCurrentBytes = nullptr;
}

bool LpcSctDmaTransmitter::isStopped() const FL_NO_EXCEPT {
    return true;  // host has no real DMA stream to wait on.
}

bool LpcSctDmaTransmitter::isDone() const FL_NO_EXCEPT {
    return true;
}


#endif  // FL_IS_ARM_LPC_845

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL

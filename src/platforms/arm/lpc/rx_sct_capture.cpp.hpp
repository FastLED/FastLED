/// @file platforms/arm/lpc/rx_sct_capture.cpp.hpp
/// @brief LPC8xx SCT input-capture RX implementation.
///
/// **Status:**
///   * Host / non-LPC builds: edge buffer is RAM-backed; `injectEdges()` +
///     `decode()` round-trip works without hardware (see
///     `tests/fl/channels/rx_sct_capture.cpp`).
///   * LPC845 builds with `-DFASTLED_LPC_RX_SCT=1`: the SCT input-capture
///     path in `begin()` / `wait()` polls SCT EVFLAG and reads CAPTURE[0..1]
///     to populate the edge buffer from real silicon. The polling design is
///     adequate for the Phase-1 pin-toggle test (≤ 100 kHz square waves —
///     see #3021).
///   * LPC845 builds with `-DFASTLED_LPC_RX_SCT_DMA=1`: upgrades the
///     capture path to **SCT → DMA**. Two DMA channels (one per edge
///     direction) stream SCT CAP[0..1] timestamps into RAM rings
///     autonomously. Required for Phase 2 WS2812 byte-match at 800 kHz —
///     the polling loop on M0+ @ 30 MHz can't keep up with WS2812
///     sub-pulses (400–850 ns) but DMA can latch every edge with no CPU
///     involvement until `wait()` drains. Setting this flag implies
///     FASTLED_LPC_RX_SCT.
///   * LPC845 builds *without* `-DFASTLED_LPC_RX_SCT`: same as host —
///     `begin()` is a no-op past clearing the buffer; the flash budget on
///     the LPC845 bring-up sketch is preserved for sketches that don't
///     opt in to the RX driver.

#if defined(FASTLED_LPC_RX_SCT_DMA) && !defined(FASTLED_LPC_RX_SCT)
#define FASTLED_LPC_RX_SCT 1
#endif

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/lpc/rx_sct_capture.h"

#if defined(FL_IS_ARM_LPC)

#include "fl/channels/rx/decode_ws2812.h"
#include "fl/log/log.h"

namespace fl {

#if defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)

// ============================================================================
// LPC845 SCT register access — see embedded comment block below for the
// UM11029 source for every offset and bit position. Layout cross-checked
// against the hardware-validated SYSCON offsets in `watchdog_lpc.impl.hpp`.
// ============================================================================
//
// Memory map (UM11029 §3 Memory Map):
//   SYSCON   0x40048000
//   SWM      0x4000C000
//   INPUTMUX 0x4002C000
//   SCT      0x50004000
//   DMA      0x50008000
//
// SYSCON SYSAHBCLKCTRL0 at +0x080, PRESETCTRL0 at +0x088 (UM11029 §8.6.22/24):
//   bit  7 SWM    bit  8 SCT    bit 17 WWDT (validated by watchdog file)
//   bit 18 IOCON  bit 29 DMA
//
// SWM PINASSIGN (UM11029 §10.5 Tables 186/187 — pin encoding: PIO0_n -> n,
// PIO1_n -> 0x20+n, unassign = 0xFF):
//   PINASSIGN6  +0x018  bits[31:24] = SCT0_GPIO_IN_A_I  (= SCT IOSEL=0)
//   PINASSIGN7  +0x01C  bits[ 7: 0] = SCT0_GPIO_IN_B_I  (= SCT IOSEL=1)
//                       bits[15: 8] = SCT0_GPIO_IN_C_I  (= SCT IOSEL=2)
//                       bits[23:16] = SCT0_GPIO_IN_D_I  (= SCT IOSEL=3)
//
// SCT register offsets (UM11029 §21.6 Table 386, UNIFY=1):
//   0x000 CONFIG     UNIFY=bit0, CLKMODE=[2:1], INSYNC=[12:9]
//   0x004 CTRL       HALT_L=bit2, CLRCTR_L=bit3
//   0x008 LIMIT      bits[7:0] = event mask that limits the unified counter
//   0x040 COUNT      32-bit free-running counter (read/write)
//   0x04C REGMODE    bits[7:0] : 1 = CAPTURE, 0 = MATCH (per index)
//   0x05C DMAREQ0    bit n = event n raises SCT_DMA0 trigger line
//   0x060 DMAREQ1    "                                "
//   0x0F0 EVEN       bits[7:0] : per-event interrupt enable
//   0x0F4 EVFLAG     bits[7:0] : per-event flag (W1C)
//   0x100+4*n MATCH[n] / CAP[n]     (selected by REGMODE bit n)
//   0x200+4*n MATCHREL[n] / CAPCTRL[n] (same — REGMODE selects)
//   0x300+8*n EV[n].STATE  bit m = enabled in STATE m
//   0x304+8*n EV[n].CTRL    layout below
//
// EV[n].CTRL fields (UM11029 §21.6.25 Table 411):
//   [ 3: 0] MATCHSEL    (unused for pure IO events)
//   [    4] HEVENT      must be 0 when UNIFY=1
//   [    5] OUTSEL      0 = input, 1 = output
//   [ 9: 6] IOSEL       SCT input number 0..3 (or output 0..6)
//   [11:10] IOCOND      0=LOW, 1=Rise, 2=Fall, 3=HIGH
//   [13:12] COMBMODE    0=OR, 1=MATCH-only, 2=IO-only, 3=AND
//
// CAPCTRL[n] (UM11029 §21.6.23 Table 409 — when REGMODE bit n = 1):
//   [ 7: 0] CAPCONn_L : bit m = 1 → event m loads CAPn

namespace {

constexpr fl::u32 kSysconBase   = 0x40048000u;
constexpr fl::u32 kSwmBase      = 0x4000C000u;
constexpr fl::u32 kInputMuxBase = 0x4002C000u;
constexpr fl::u32 kSctBase      = 0x50004000u;
constexpr fl::u32 kDmaBase      = 0x50008000u;

constexpr fl::u32 kOffSYSAHBCLKCTRL0 = 0x080u;
constexpr fl::u32 kOffPRESETCTRL0    = 0x088u;
constexpr fl::u32 kClkSWM = (1u <<  7);
constexpr fl::u32 kClkSCT = (1u <<  8);
constexpr fl::u32 kClkDMA = (1u << 29);
constexpr fl::u32 kRstSWM = (1u <<  7);
constexpr fl::u32 kRstSCT = (1u <<  8);
constexpr fl::u32 kRstDMA = (1u << 29);

constexpr fl::u32 kOffPINASSIGN6 = 0x018u;
constexpr fl::u32 kOffPINASSIGN7 = 0x01Cu;
constexpr fl::u8  kSwmUnassign   = 0xFFu;

constexpr fl::u32 kOffCONFIG  = 0x000u;
constexpr fl::u32 kOffCTRL    = 0x004u;
constexpr fl::u32 kOffLIMIT   = 0x008u;
constexpr fl::u32 kOffCOUNT   = 0x040u;
constexpr fl::u32 kOffREGMODE = 0x04Cu;
constexpr fl::u32 kOffEVEN    = 0x0F0u;
constexpr fl::u32 kOffEVFLAG  = 0x0F4u;
constexpr fl::u32 kOffMATCH0  = 0x100u;       // shared with CAP[n]
constexpr fl::u32 kOffMATCHREL0 = 0x200u;     // shared with CAPCTRL[n]
constexpr fl::u32 kOffEV0_STATE = 0x300u;
constexpr fl::u32 kOffEV0_CTRL  = 0x304u;
constexpr fl::u32 kOffDMAREQ0 = 0x05Cu;       // bit n = event n raises SCT_DMA0
constexpr fl::u32 kOffDMAREQ1 = 0x060u;       // bit n = event n raises SCT_DMA1

constexpr fl::u32 kCfgUnify    = (1u << 0);
constexpr fl::u32 kCfgInsync0  = (1u << 9);
constexpr fl::u32 kCtrlHaltL   = (1u << 2);
constexpr fl::u32 kCtrlClrCtrL = (1u << 3);

// INPUTMUX (UM11029 §14 Tables 287/288). DMA_ITRIG_INMUX[k] selects which
// trigger source drives DMA channel k. Source IDs:
//   0x0=ADC_SEQA_IRQ, 0x1=ADC_SEQB_IRQ, 0x2=SCT_DMA0, 0x3=SCT_DMA1,
//   0x4=ACMP_O, 0x5..0x8=PININT4..7, 0x9=T0_DMAREQ_M0, 0xA=T0_DMAREQ_M1,
//   0xB=DMA_INMUX0, 0xC=DMA_INMUX1. Reset = 0x0F (no trigger).
constexpr fl::u32 kOffDmaItrigInmux0 = 0x040u;  // +0x004*k for DMA channel k
constexpr fl::u8  kItrigSctDma0 = 0x2u;
constexpr fl::u8  kItrigSctDma1 = 0x3u;

// DMA controller (UM11029 §16.6 Table 301). Channel regs start at +0x400,
// 16 bytes per channel (CFG, CTLSTAT, XFERCFG, reserved).
constexpr fl::u32 kOffDmaCtrl       = 0x000u;
constexpr fl::u32 kOffDmaIntStat    = 0x004u;
constexpr fl::u32 kOffDmaSramBase   = 0x008u;
constexpr fl::u32 kOffDmaEnableSet0 = 0x020u;
constexpr fl::u32 kOffDmaEnableClr0 = 0x028u;
constexpr fl::u32 kOffDmaActive0    = 0x030u;
constexpr fl::u32 kOffDmaBusy0      = 0x038u;
constexpr fl::u32 kOffDmaErrInt0    = 0x040u;
constexpr fl::u32 kOffDmaIntEnSet0  = 0x048u;
constexpr fl::u32 kOffDmaIntEnClr0  = 0x050u;
constexpr fl::u32 kOffDmaIntA0      = 0x058u;
constexpr fl::u32 kOffDmaIntB0      = 0x060u;
constexpr fl::u32 kOffDmaSetValid0  = 0x068u;
constexpr fl::u32 kOffDmaSetTrig0   = 0x070u;
constexpr fl::u32 kOffDmaAbort0     = 0x078u;
constexpr fl::u32 kDmaChCfg(fl::u32 ch)     { return 0x400u + 16u * ch; }
constexpr fl::u32 kDmaChCtlStat(fl::u32 ch) { return 0x404u + 16u * ch; }
constexpr fl::u32 kDmaChXferCfg(fl::u32 ch) { return 0x408u + 16u * ch; }

// DMA channel CFG bit positions (UM11029 §16.6 Table 318).
constexpr fl::u32 kDmaCfgPeriphReqEn = (1u << 0);
constexpr fl::u32 kDmaCfgHwTrigEn    = (1u << 1);
constexpr fl::u32 kDmaCfgTrigPolHi   = (1u << 4);   // rising edge / high level
constexpr fl::u32 kDmaCfgTrigTypeLvl = (1u << 5);   // 0 = edge, 1 = level
constexpr fl::u32 kDmaCfgTrigBurst   = (1u << 6);

// DMA channel XFERCFG bit positions (UM11029 §16.6 Table 321).
constexpr fl::u32 kXferCfgValid  = (1u << 0);
constexpr fl::u32 kXferReload    = (1u << 1);
constexpr fl::u32 kXferSwTrig    = (1u << 2);
constexpr fl::u32 kXferClrTrig   = (1u << 3);
constexpr fl::u32 kXferSetIntA   = (1u << 4);
constexpr fl::u32 kXferSetIntB   = (1u << 5);
constexpr fl::u32 kXferWidth32   = (2u << 8);       // [9:8] WIDTH (0=8b, 1=16b, 2=32b)
constexpr fl::u32 kXferSrcIncOff = (0u << 12);      // [13:12] SRCINC
constexpr fl::u32 kXferDstInc1   = (1u << 14);      // [15:14] DSTINC = +1 width
constexpr fl::u32 kXferCount(fl::u32 n) { return ((n - 1u) & 0x3FFu) << 16; }   // [25:16]

// EV CTRL builder for "input-capture on SCT input N, edge E"
constexpr fl::u32 evCtrlInputCapture(fl::u32 iosel, fl::u32 iocond) {
    return (0u << 5)                  // OUTSEL = 0 (input)
         | ((iosel & 0xFu) << 6)      // IOSEL
         | ((iocond & 0x3u) << 10)    // IOCOND  (1=Rise, 2=Fall)
         | (2u << 12);                // COMBMODE = IO-only
}
constexpr fl::u32 kIoCondRise = 1u;
constexpr fl::u32 kIoCondFall = 2u;

inline volatile fl::u32& reg(fl::u32 base, fl::u32 offset) FL_NOEXCEPT {
    return *reinterpret_cast<volatile fl::u32*>(base + offset);  // ok reinterpret cast — MMIO addressing
}
inline volatile fl::u32& sct(fl::u32 offset) FL_NOEXCEPT { return reg(kSctBase, offset); }
inline volatile fl::u32& sct_cap(fl::u32 n) FL_NOEXCEPT { return reg(kSctBase, kOffMATCH0    + 4u * n); }
inline volatile fl::u32& sct_capctrl(fl::u32 n) FL_NOEXCEPT { return reg(kSctBase, kOffMATCHREL0 + 4u * n); }
inline volatile fl::u32& sct_ev_state(fl::u32 n) FL_NOEXCEPT { return reg(kSctBase, kOffEV0_STATE + 8u * n); }
inline volatile fl::u32& sct_ev_ctrl(fl::u32 n) FL_NOEXCEPT { return reg(kSctBase, kOffEV0_CTRL  + 8u * n); }
inline fl::u32* sct_cap_addr(fl::u32 n) FL_NOEXCEPT {
    return reinterpret_cast<fl::u32*>(kSctBase + kOffMATCH0 + 4u * n);  // ok reinterpret cast — DMA src address
}
inline volatile fl::u32& dma(fl::u32 offset) FL_NOEXCEPT { return reg(kDmaBase, offset); }

// Assign or unassign the user's GPIO pin to SCT input 0 (SCT0_GPIO_IN_A_I)
// via SWM PINASSIGN6 byte[31:24]. The byte value is the encoded pin number
// (PIO0_n -> n; PIO1_n -> 0x20+n).
inline void swmAssignSctInput0(fl::u8 swm_byte) FL_NOEXCEPT {
    volatile fl::u32& r = reg(kSwmBase, kOffPINASSIGN6);
    fl::u32 v = r;
    v = (v & 0x00FFFFFFu) | (static_cast<fl::u32>(swm_byte) << 24);
    r = v;
}

// Convert an unsigned tick delta (free-running 32-bit SCT counter, F_CPU
// ticks) to nanoseconds. F_CPU is the SCT clock when CLKMODE=0; on
// LPC845-BRK that is 30_000_000 Hz, giving 33.33 ns per tick.
// Compute with u64 to avoid mid-multiplication overflow at the long-period
// extreme: a full 32-bit tick wrap at 30 MHz is ~143 s = 1.43e11 ns, which
// fits u64 with margin. EdgeTime.ns is a 31-bit bitfield (max ~2.1 s) so
// we saturate above that.
inline fl::u32 ticksToNs(fl::u32 ticks) FL_NOEXCEPT {
    constexpr fl::u32 kFcpuMHz = (F_CPU + 500000u) / 1000000u;  // 30 on LPC845
    fl::u64 ns64 = (static_cast<fl::u64>(ticks) * 1000u + (kFcpuMHz / 2)) / kFcpuMHz;
    if (ns64 > 0x7FFFFFFFull) ns64 = 0x7FFFFFFFull;
    return static_cast<fl::u32>(ns64);
}

#if defined(FASTLED_LPC_RX_SCT_DMA)
// ============================================================================
// DMA capture path — Phase 2 acceleration. Configures two DMA channels
// (one per edge direction) to autonomously stream SCT CAP[0..1] timestamps
// into RAM rings without CPU involvement. wait() drains the rings into
// `mEdges` sorted by timestamp.
//
// Channel assignment:
//   * DMA ch 0 — source SCT->CAP[0] (rising edge timestamps),
//                triggered by SCT_DMA0 (raised by SCT EV0), writes into
//                g_dma_rising[].
//   * DMA ch 1 — source SCT->CAP[1] (falling edge timestamps),
//                triggered by SCT_DMA1 (raised by SCT EV1), writes into
//                g_dma_falling[].
//
// Both channels are single-shot per begin() (no auto-reload). On wait()
// completion the SCT is halted; the channels stop on their own once
// XFERCOUNT reaches 0. We measure how many edges were captured by
// reading the residual XFERCOUNT field of each channel's CHANNEL.XFERCFG.

// Ring sizing constraints (LPC845 has 16 KB SRAM total):
//   * Each ring entry: 4 bytes (one 32-bit SCT counter tick)
//   * Two rings × 512 entries × 4 B = 4 KB total — 25 % of SRAM, fits
//     comfortably alongside fl::Remote + watchdog + Arduino core
//   * 512 entries per direction → 512 rising + 512 falling = 1024 total
//     edges = 1024 / (24 * 2) = ~21 WS2812 LEDs per capture window
//   * For longer captures, bump kDmaRingSize at the cost of SRAM
constexpr fl::size kDmaRingSize = 512;

FL_ALIGNAS(4) fl::u32 g_dma_rising[kDmaRingSize];
FL_ALIGNAS(4) fl::u32 g_dma_falling[kDmaRingSize];

// DMA descriptor table. UM11029 §16.7.1 requires 256-byte alignment for
// chips with up to 16 channels; LPC845 has 25 channels and the required
// alignment grows to 512 bytes (the highest channel descriptor must land
// in the same 512-byte aligned region). Each descriptor is 16 bytes:
//   [ 0]  reserved (some SDKs label this XFERCFG mirror)
//   [ 4]  SRC_END_ADDR
//   [ 8]  DST_END_ADDR
//   [12]  LINK_ADDR (0 = single-shot)
//
// We allocate the full 25-channel table (400 bytes used + 112 bytes
// alignment slack = 512 bytes) — DMA->SRAMBASE only accepts the table
// base, not a slice.
struct FL_ALIGNAS(16) DmaDescriptor {
    fl::u32 reserved;
    fl::u32 src_end_addr;
    fl::u32 dst_end_addr;
    fl::u32 link_addr;
};
FL_ALIGNAS(512) DmaDescriptor g_dma_descriptors[25];

// Compute how many DMA transfers (= captured edges) have landed in the
// channel's ring. The CHANNEL.XFERCFG register exposes the live residual
// count and the CFGVALID flag (UM11029 §16.6 Table 321):
//   * While the channel is armed, residual = (N-1) - K where K = writes done
//   * On natural completion (K == N), CFGVALID drops to 0 and residual = 0
inline fl::u32 dmaChannelEdgeCount(fl::u32 ch) FL_NOEXCEPT {
    const fl::u32 cfgnow = dma(kDmaChXferCfg(ch));
    const fl::u32 residual = (cfgnow >> 16) & 0x3FFu;
    const bool completed = (cfgnow & kXferCfgValid) == 0;
    if (completed) {
        return static_cast<fl::u32>(kDmaRingSize);
    }
    if (residual >= kDmaRingSize) {
        return 0;
    }
    return static_cast<fl::u32>(kDmaRingSize) - 1u - residual;
}
#endif  // FASTLED_LPC_RX_SCT_DMA

}  // namespace

#endif  // FASTLED_LPC_RX_SCT && FL_IS_ARM_LPC_845

// ============================================================================
// Factory
// ============================================================================

fl::shared_ptr<LpcSctRxChannel> LpcSctRxChannel::create(int pin) FL_NOEXCEPT {
    return fl::make_shared<LpcSctRxChannel>(pin);
}

// ============================================================================
// Constructor
// ============================================================================

LpcSctRxChannel::LpcSctRxChannel(int pin) FL_NOEXCEPT
    : mPin(pin)
    , mFinished(false)
    , mCapacity(0)
    , mPrevTick(0)
    , mPrevSeen(false)
    , mLastRising(false) {
}

LpcSctRxChannel::~LpcSctRxChannel() FL_NOEXCEPT {
#if defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)
    // Release the pin from SCT input 0 so a follow-up driver can claim it.
    // Leave the SCT/SWM clocks gated on — turning them off would require
    // proof no other driver is mid-flight, which is brittle. Cost of
    // leaving them on: ~tens of µA static, negligible vs the WWDT (always on).
    sct(kOffCTRL) |= kCtrlHaltL;
    sct(kOffEVEN)  = 0;
    swmAssignSctInput0(kSwmUnassign);
#if defined(FASTLED_LPC_RX_SCT_DMA)
    // Disable DMA channels + abort any in-flight transfer so the next
    // driver instance starts from a clean DMA state.
    dma(kOffDmaEnableClr0) = (1u << 0) | (1u << 1);
    dma(kOffDmaAbort0)     = (1u << 0) | (1u << 1);
    sct(kOffDMAREQ0) = 0u;
    sct(kOffDMAREQ1) = 0u;
#endif
#endif
}

// ============================================================================
// RxDevice interface
// ============================================================================

bool LpcSctRxChannel::begin(const RxConfig& config) FL_NOEXCEPT {
    mFinished = false;
    mCapacity = config.buffer_size > 0 ? config.buffer_size : 4096u;
    mPrevTick = 0;
    mPrevSeen = false;
    mLastRising = false;
    mEdges.clear();
    mEdges.reserve(mCapacity);

#if defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)
    // ---- 1. Power up SCT + SWM (and DMA when the DMA path is opted in),
    //         then pulse their resets. ----
    reg(kSysconBase, kOffSYSAHBCLKCTRL0) |= (kClkSCT | kClkSWM
#if defined(FASTLED_LPC_RX_SCT_DMA)
        | kClkDMA
#endif
        );
    volatile fl::u32& presetctrl = reg(kSysconBase, kOffPRESETCTRL0);
    presetctrl &= ~(kRstSCT | kRstSWM
#if defined(FASTLED_LPC_RX_SCT_DMA)
        | kRstDMA
#endif
        );   // assert reset (low pulse)
    presetctrl |=  (kRstSCT | kRstSWM
#if defined(FASTLED_LPC_RX_SCT_DMA)
        | kRstDMA
#endif
        );   // release reset

    // ---- 2. Route mPin -> SCT input 0 via SWM PINASSIGN6 byte[31:24]. ----
    swmAssignSctInput0(static_cast<fl::u8>(mPin & 0xFFu));

    // ---- 3. Configure SCT: UNIFY counter + INSYNC for input 0. ----
    // The 2-clock synchronizer adds ~67 ns of capture jitter at F_CPU=30 MHz,
    // well below the WS2812 T0H/T1H discrimination window (~500 ns margin).
    sct(kOffCTRL)    = kCtrlHaltL | kCtrlClrCtrL;   // halted, counter zeroed
    sct(kOffCONFIG)  = kCfgUnify  | kCfgInsync0;
    sct(kOffLIMIT)   = 0u;                          // free-running counter
    sct(kOffCOUNT)   = 0u;

    // ---- 4. Mark register slots 0 + 1 as CAPTURE (REGMODE bits 0,1 = 1). ----
    sct(kOffREGMODE) = (1u << 0) | (1u << 1);

    // ---- 5. Event 0 → rising edge on SCT input 0 → captures into CAP[0].
    //         Event 1 → falling edge on SCT input 0 → captures into CAP[1].
    sct_ev_state(0) = 0x1u;   // active in STATE 0
    sct_ev_ctrl (0) = evCtrlInputCapture(0u, kIoCondRise);
    sct_ev_state(1) = 0x1u;
    sct_ev_ctrl (1) = evCtrlInputCapture(0u, kIoCondFall);

    sct_capctrl(0) = (1u << 0);   // EV0 loads CAP[0]
    sct_capctrl(1) = (1u << 1);   // EV1 loads CAP[1]

    sct(kOffEVEN)   = 0u;                        // polling mode — IRQs off
    sct(kOffEVFLAG) = 0xFFu;                     // clear any stale flags

#if defined(FASTLED_LPC_RX_SCT_DMA)
    // ---- 6. SCT → DMA wiring. ----
    // SCT->DMAREQ0 = (1<<0)  → EV0 (rising) raises SCT_DMA0 trigger line
    // SCT->DMAREQ1 = (1<<1)  → EV1 (falling) raises SCT_DMA1 trigger line
    sct(kOffDMAREQ0) = (1u << 0);
    sct(kOffDMAREQ1) = (1u << 1);

    // INPUTMUX: route SCT_DMA0 → DMA ch 0, SCT_DMA1 → DMA ch 1.
    reg(kInputMuxBase, kOffDmaItrigInmux0 + 4u * 0) = kItrigSctDma0;
    reg(kInputMuxBase, kOffDmaItrigInmux0 + 4u * 1) = kItrigSctDma1;

    // ---- 7. DMA controller setup. ----
    // SRAMBASE must hold the channel-descriptor table base address.
    // Required alignment is 512 bytes (LPC845 has 25 channels — UM11029
    // §16.7.1). `g_dma_descriptors` is `alignas(512)`.
    dma(kOffDmaCtrl)     = 1u;                                    // global enable
    dma(kOffDmaSramBase) = reinterpret_cast<fl::u32>(&g_dma_descriptors[0]);  // ok reinterpret cast — DMA descriptor table base

    // Pre-zero the rings so partial captures are detectable (we still
    // count edges via XFERCOUNT residual; the zero-fill is defensive).
    for (fl::size i = 0; i < kDmaRingSize; ++i) {
        g_dma_rising[i]  = 0u;
        g_dma_falling[i] = 0u;
    }

    // Build descriptors for channels 0 and 1.
    //   * SRC: SCT->CAP[n] (no increment — same address every transfer)
    //     → src_end_addr == src_start_addr == &CAP[n]
    //   * DST: g_dma_rising/falling, post-increment by 4 bytes per word
    //     → dst_end_addr = &ring[kDmaRingSize - 1] (last word in ring)
    //   * LINK: 0 — single-shot, no auto-reload
    g_dma_descriptors[0].reserved     = 0u;
    g_dma_descriptors[0].src_end_addr = reinterpret_cast<fl::u32>(sct_cap_addr(0));  // ok reinterpret cast — MMIO source addr
    g_dma_descriptors[0].dst_end_addr = reinterpret_cast<fl::u32>(&g_dma_rising[kDmaRingSize - 1]);  // ok reinterpret cast — RAM dest addr
    g_dma_descriptors[0].link_addr    = 0u;

    g_dma_descriptors[1].reserved     = 0u;
    g_dma_descriptors[1].src_end_addr = reinterpret_cast<fl::u32>(sct_cap_addr(1));  // ok reinterpret cast — MMIO source addr
    g_dma_descriptors[1].dst_end_addr = reinterpret_cast<fl::u32>(&g_dma_falling[kDmaRingSize - 1]);  // ok reinterpret cast — RAM dest addr
    g_dma_descriptors[1].link_addr    = 0u;

    // Channel CFG: hardware trigger via INPUTMUX, rising-edge trigger
    // polarity (the SCT_DMAn lines pulse high on each capture event).
    const fl::u32 chan_cfg = kDmaCfgHwTrigEn | kDmaCfgTrigPolHi;   // edge-triggered (TRIGTYPE=0)
    dma(kDmaChCfg(0)) = chan_cfg;
    dma(kDmaChCfg(1)) = chan_cfg;

    // Channel XFERCFG: 32-bit width, no-inc source, +1 word dest,
    // XFERCOUNT = kDmaRingSize - 1 (encoded). No interrupt — wait()
    // polls residuals.
    const fl::u32 chan_xfercfg =
        kXferCfgValid
        | kXferWidth32
        | kXferSrcIncOff
        | kXferDstInc1
        | kXferCount(kDmaRingSize);
    dma(kDmaChXferCfg(0)) = chan_xfercfg;
    dma(kDmaChXferCfg(1)) = chan_xfercfg;

    // SETVALID arms the channel: descriptor's VALIDPENDING → VALID
    dma(kOffDmaSetValid0) = (1u << 0) | (1u << 1);

    // Enable channels (now armed and waiting on the SCT_DMAn trigger).
    dma(kOffDmaEnableSet0) = (1u << 0) | (1u << 1);
#endif

    // ---- 8. Release HALT so the counter runs. Capture events will start
    //         latching CAP[0] / CAP[1] on every input edge from now on.
    sct(kOffCTRL) &= ~kCtrlHaltL;
#endif

    return true;
}

bool LpcSctRxChannel::finished() const FL_NOEXCEPT {
    return mFinished;
}

fl::size LpcSctRxChannel::pollOnce() FL_NOEXCEPT {
#if defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)
    // Drain SCT EVFLAG into the edge buffer. For each event that has
    // fired since the last poll:
    //   * EV0 (rising-edge capture) → read CAP[0]. If we already saw a
    //     prior edge, the line was LOW from `mPrevTick` to `tick` (since
    //     this is a rising edge, so it was LOW before).
    //   * EV1 (falling-edge capture) → read CAP[1]. Line was HIGH from
    //     `mPrevTick` to `tick`.
    //
    // The "phase level" labeled into EdgeTime.high is the level the line
    // was at DURING the just-completed phase, not the edge direction.
    // That matches the decoder's expectation: HIGH duration then LOW
    // duration per WS2812 bit.
    //
    // We deliberately don't clear EVFLAG until AFTER reading CAP[n]; the
    // SCT latches a fresh CAP[n] on the *next* matching edge regardless
    // of the EVFLAG bit, so the read/W1C order matters only for the
    // poll-once semantic (one capture per poll-cycle per event).

    fl::size pushed = 0;
    const fl::u32 evflag = sct(kOffEVFLAG);

    if (evflag & 0x1u) {
        const fl::u32 tick = sct_cap(0);
        sct(kOffEVFLAG) = 0x1u;  // W1C
        if (mPrevSeen) {
            const fl::u32 delta = tick - mPrevTick;   // u32 wrap is fine — modulo arithmetic over 32-bit counter
            if (mEdges.size() < mCapacity) {
                mEdges.push_back(EdgeTime(mLastRising, ticksToNs(delta)));
                ++pushed;
            }
        }
        mPrevTick = tick;
        mPrevSeen = true;
        mLastRising = true;  // a rising edge just fired → line was LOW before, HIGH now
    }

    if (evflag & 0x2u) {
        const fl::u32 tick = sct_cap(1);
        sct(kOffEVFLAG) = 0x2u;
        if (mPrevSeen) {
            const fl::u32 delta = tick - mPrevTick;
            if (mEdges.size() < mCapacity) {
                mEdges.push_back(EdgeTime(mLastRising, ticksToNs(delta)));
                ++pushed;
            }
        }
        mPrevTick = tick;
        mPrevSeen = true;
        mLastRising = false;
    }

    return pushed;
#else
    return 0;
#endif
}

RxWaitResult LpcSctRxChannel::wait(u32 timeout_ms) FL_NOEXCEPT {
#if defined(FASTLED_LPC_RX_SCT_DMA) && defined(FL_IS_ARM_LPC_845)
    // DMA capture path. The two DMA channels are autonomously latching
    // SCT->CAP[0..1] into the rings via the SCT_DMAn trigger lines —
    // CPU just sleeps for the capture window, then halts the SCT and
    // merges the two rings by timestamp into mEdges.
    //
    // ---- 1. Sleep for the capture window. ----
    const fl::u32 start_ms = millis();
    while ((millis() - start_ms) < timeout_ms) {
        const fl::u32 rising_done  = dmaChannelEdgeCount(0);
        const fl::u32 falling_done = dmaChannelEdgeCount(1);
        if (rising_done >= kDmaRingSize && falling_done >= kDmaRingSize) {
            break;  // both rings full — no point waiting longer
        }
        // No need to spin tight; the DMA work happens in hardware.
        // A small idle keeps wakelet contention down. (millis() based
        // throttle — we don't need precise pacing here.)
    }

    // ---- 2. Halt SCT to stop further captures. ----
    sct(kOffCTRL) |= kCtrlHaltL;

    // ---- 3. Snapshot how many entries each ring received. ----
    const fl::u32 n_rising  = dmaChannelEdgeCount(0);
    const fl::u32 n_falling = dmaChannelEdgeCount(1);

    // Disable channels so SETVALID can re-arm in the next begin().
    dma(kOffDmaEnableClr0) = (1u << 0) | (1u << 1);

    // ---- 4. Merge the two rings by timestamp into mEdges. ----
    // The rings hold raw 32-bit SCT counter ticks. Walk both in parallel
    // (they're already monotonic per-direction), pick the earlier tick,
    // emit `EdgeTime{level, ns-delta-from-prior}`.
    fl::u32 ir = 0, jf = 0;
    fl::u32 prev_tick = 0;
    bool prev_seen = false;
    bool last_was_rising = false;
    while (ir < n_rising || jf < n_falling) {
        if (mEdges.size() >= mCapacity) break;

        bool pick_rising;
        if (ir >= n_rising) {
            pick_rising = false;
        } else if (jf >= n_falling) {
            pick_rising = true;
        } else {
            // Both rings have unread entries; pick the earlier tick.
            // Unsigned subtract treats wrap as modulo — assume neither
            // ring's outstanding entries span a full 32-bit wrap (~143 s
            // at 30 MHz, way beyond our test windows).
            pick_rising = (g_dma_rising[ir] - g_dma_falling[jf]) >= 0x80000000u;
        }

        const fl::u32 tick = pick_rising ? g_dma_rising[ir] : g_dma_falling[jf];
        if (prev_seen) {
            const fl::u32 delta = tick - prev_tick;
            mEdges.push_back(EdgeTime(last_was_rising, ticksToNs(delta)));
        }
        prev_tick = tick;
        prev_seen = true;
        last_was_rising = pick_rising;   // rising = HIGH starts now, prior was LOW

        if (pick_rising) ++ir;
        else             ++jf;
    }

#elif defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)
    // Polling path (Phase 1 fallback). Spin-poll for `timeout_ms` or until
    // the buffer is full. Callers that need to interleave TX (bit-bang)
    // with capture should drive pollOnce() directly inside their TX loop —
    // see `examples/AutoResearchLpc/AutoResearchLpc.ino::pinToggleRx`.
    const fl::u32 start_ms = millis();
    while ((millis() - start_ms) < timeout_ms && mEdges.size() < mCapacity) {
        pollOnce();
    }
    sct(kOffCTRL) |= kCtrlHaltL;
#else
    (void)timeout_ms;
#endif

    mFinished = true;
    return mEdges.empty() ? RxWaitResult::TIMEOUT : RxWaitResult::SUCCESS;
}

// ============================================================================
// Decode — edge-pair → bytes
// ============================================================================
//
// Delegates to the shared 4-phase WS2812 decoder in
// `fl/channels/rx/decode_ws2812.h` (formerly an inline copy here; see
// FastLED #3035 Phase 3 for the consolidation history).
fl::result<u32, DecodeError> LpcSctRxChannel::decode(const ChipsetTiming4Phase& timing,
                                                     fl::span<u8> out) FL_NOEXCEPT {
    if (mEdges.empty()) {
        FL_WARN("LpcSctRxChannel::decode: No edges recorded for pin " << mPin);
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return fl::channels::rx::decodeWs2812Edges(
        timing, fl::span<const EdgeTime>(mEdges), out);
}

size_t LpcSctRxChannel::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NOEXCEPT {
    size_t total = mEdges.size();
    if (offset >= total) return 0;

    size_t count = total - offset;
    if (count > out.size()) count = out.size();

    for (size_t i = 0; i < count; i++) {
        out[i] = mEdges[offset + i];
    }
    return count;
}

const char* LpcSctRxChannel::name() const FL_NOEXCEPT {
    return "LPC_SCT_CAPTURE";
}

int LpcSctRxChannel::getPin() const FL_NOEXCEPT {
    return mPin;
}

bool LpcSctRxChannel::injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT {
    for (size_t i = 0; i < edges.size(); i++) {
        mEdges.push_back(edges[i]);
    }
    return true;
}

} // namespace fl

#endif // FL_IS_ARM_LPC

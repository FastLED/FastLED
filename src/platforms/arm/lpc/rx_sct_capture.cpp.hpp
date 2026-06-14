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
///     see #3021). Phase 2 (WS2812 at 800 kHz) will upgrade to SCT→DMA in a
///     follow-up commit.
///   * LPC845 builds *without* `-DFASTLED_LPC_RX_SCT`: same as host —
///     `begin()` is a no-op past clearing the buffer; the flash budget on
///     the LPC845 bring-up sketch is preserved for sketches that don't
///     opt in to the RX driver.

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/lpc/rx_sct_capture.h"

#if defined(FL_IS_ARM_LPC)

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
constexpr fl::u32 kSctBase      = 0x50004000u;

constexpr fl::u32 kOffSYSAHBCLKCTRL0 = 0x080u;
constexpr fl::u32 kOffPRESETCTRL0    = 0x088u;
constexpr fl::u32 kClkSWM = (1u <<  7);
constexpr fl::u32 kClkSCT = (1u <<  8);
constexpr fl::u32 kRstSWM = (1u <<  7);
constexpr fl::u32 kRstSCT = (1u <<  8);

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

constexpr fl::u32 kCfgUnify    = (1u << 0);
constexpr fl::u32 kCfgInsync0  = (1u << 9);
constexpr fl::u32 kCtrlHaltL   = (1u << 2);
constexpr fl::u32 kCtrlClrCtrL = (1u << 3);

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
    // ---- 1. Power up SCT + SWM (and pulse their resets). ----
    reg(kSysconBase, kOffSYSAHBCLKCTRL0) |= (kClkSCT | kClkSWM);
    volatile fl::u32& presetctrl = reg(kSysconBase, kOffPRESETCTRL0);
    presetctrl &= ~(kRstSCT | kRstSWM);   // assert reset (low pulse)
    presetctrl |=  (kRstSCT | kRstSWM);   // release reset

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

    // ---- 6. Release HALT so the counter runs. Capture events will start
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
#if defined(FASTLED_LPC_RX_SCT) && defined(FL_IS_ARM_LPC_845)
    // Spin-poll for `timeout_ms` or until the buffer is full. Callers
    // that need to interleave TX (bit-bang) with capture should drive
    // pollOnce() directly inside their TX loop — see
    // `examples/AutoResearchLpc/AutoResearchLpc.ino::pinToggleRx`.
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
// Same 4-phase decoder as `rx_device_native.cpp.hpp::decode` (and the
// per-driver copies in FlexPWM / FlexIO). Each WS2812 bit produces two
// edge entries:
//
//   EdgeTime(high=true,  ns=T0H or T1H)  — HIGH duration
//   EdgeTime(high=false, ns=T0L or T1L)  — LOW duration
//
// Bit 0: T0H ∈ [t0h_min, t0h_max], T0L ∈ [t0l_min, t0l_max]
// Bit 1: T1H ∈ [t1h_min, t1h_max], T1L ∈ [t1l_min, t1l_max]
//
// Kept in-TU per the per-driver convention. Consolidation target:
// `src/fl/channels/rx/decode_ws2812.h` (follow-up refactor).
fl::result<u32, DecodeError> LpcSctRxChannel::decode(const ChipsetTiming4Phase& timing,
                                                     fl::span<u8> out) FL_NOEXCEPT {
    size_t edge_count = mEdges.size();
    if (edge_count == 0) {
        FL_WARN("LpcSctRxChannel::decode: No edges recorded for pin " << mPin);
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t bytes_written = 0;
    size_t bit_index = 0;
    u8 current_byte = 0;
    u32 error_count = 0;

    size_t i = 0;
    while (i + 1 < edge_count) {
        EdgeTime high_edge = mEdges[i];
        EdgeTime low_edge  = mEdges[i + 1];

        // Edges must alternate HIGH then LOW.
        if (!high_edge.high || low_edge.high) {
            error_count++;
            i++;
            continue;
        }

        u32 high_ns = high_edge.ns;
        u32 low_ns  = low_edge.ns;

        bool is_bit1 = (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
                        low_ns  >= timing.t1l_min_ns && low_ns  <= timing.t1l_max_ns);
        bool is_bit0 = (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
                        low_ns  >= timing.t0l_min_ns && low_ns  <= timing.t0l_max_ns);

        if (!is_bit0 && !is_bit1) {
            // Reset pulse?
            if (low_ns >= (u32)(timing.reset_min_us * 1000u)) {
                if (bit_index != 0) {
                    FL_WARN("LpcSctRxChannel::decode: Partial byte at reset (bit_index="
                            << bit_index << ")");
                }
                break;
            }
            // Tolerable inter-frame gap (PARLIO-style)?
            if (timing.gap_tolerance_ns > 0 && low_ns <= timing.gap_tolerance_ns) {
                if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                    is_bit1 = true;
                } else if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                    is_bit0 = true;
                }
            }

            if (!is_bit0 && !is_bit1) {
                error_count++;
                i += 2;
                continue;
            }
        }

        int bit = is_bit1 ? 1 : 0;
        current_byte = (u8)((current_byte << 1) | (u8)bit);
        bit_index++;

        if (bit_index == 8) {
            if (bytes_written >= out.size()) {
                return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out[bytes_written++] = current_byte;
            current_byte = 0;
            bit_index = 0;
        }

        i += 2;
    }

    // > 10% error rate → reject the decode
    if (bytes_written > 0 && error_count * 10 > bytes_written * 8) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::result<u32, DecodeError>::success(static_cast<u32>(bytes_written));
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

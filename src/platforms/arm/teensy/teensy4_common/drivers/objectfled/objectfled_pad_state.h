#pragma once

// IWYU pragma: private

/// @file objectfled_pad_state.h
/// @brief Saves and restores the pad state ObjectFLED's SPI and clockless modes
/// take over. The clockless mode claims each output pin on its own through
/// objectfled_clockless_claim_pad() below, using the same snapshot per pad.
///
/// The SPI mode moves its two pads from fast GPIO6 to GPIO1, because eDMA cannot
/// reach GPIO6, by clearing their IOMUXC_GPR_GPR26 bits. It then drives them as
/// GPIO1 outputs with its own mux and pad settings. Teensy's pinMode() and
/// FastPin keep using GPIO6 and never write GPR26, so a remap left behind
/// disconnects ordinary GPIO from those pads. This snapshot covers every
/// register the mode writes for the pads: the GPR26 bits, the GPIO1 direction
/// and output bits, and each pad's mux and pad-control value. Other bits in the
/// shared registers are never written.
///
/// Registers come in by pointer, so this runs against the real IMXRT
/// registers on Teensy and against plain variables in host tests.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// The registers the SPI mode writes for its two pads.
struct ObjectFLEDPadRegs {
    volatile u32* gpr26;       ///< IOMUXC_GPR_GPR26: bit set = GPIO6, clear = GPIO1.
    volatile u32* gpio1_gdir;  ///< GPIO1 direction.
    volatile u32* gpio1_dr;    ///< GPIO1 output latch (read: pad level for inputs).
    volatile u32* gpio1_dr_set;    ///< GPIO1_DR_SET: write 1s to set latch bits.
    volatile u32* gpio1_dr_clear;  ///< GPIO1_DR_CLEAR: write 1s to clear latch bits.
    volatile u32* mosi_mux;
    volatile u32* mosi_pad;
    volatile u32* sclk_mux;
    volatile u32* sclk_pad;
};

class ObjectFLEDPadState {
  public:
    /// Records the pads' state before the mode touches them. `mask` holds both
    /// pads' bits, which are the same in GPR26, GPIO1 and GPIO6.
    void capture(const ObjectFLEDPadRegs& regs, u32 mask) FL_NO_EXCEPT {
        mMask = mask;
        mGpr26 = *regs.gpr26 & mask;
        mGdir = *regs.gpio1_gdir & mask;
        // Reading DR for an input pin returns the pad level, not the latch, so
        // the latch of a pin that was an input cannot be known here. Record it
        // only for outputs; restore leaves input pins' latch alone (its value
        // does not matter while the pin is an input).
        mDr = *regs.gpio1_dr & mGdir;
        mMosiMux = *regs.mosi_mux;
        mMosiPad = *regs.mosi_pad;
        mSclkMux = *regs.sclk_mux;
        mSclkPad = *regs.sclk_pad;
        mCaptured = true;
    }

    /// Puts the recorded state back, then forgets it; a second call does
    /// nothing. Call only once the DMA has stopped writing GPIO1. Order: the
    /// bank select first, so a pad that started on GPIO6 goes straight to its
    /// old GPIO6 state while GPIO1 still holds the mode's parked-low outputs;
    /// then GPIO1 pins saved as inputs are released (GDIR cleared) before any
    /// latch write, so no input pin is driven a high pulse; then the latch of
    /// pins saved as outputs (via DR_SET/DR_CLEAR) before their direction, so a pad that started as
    /// a GPIO1 output never drives a stale level; then pad control, then the
    /// mux, which gives the pad back to its previous owner last. Input pins'
    /// latch is left as the mode left it (see capture()).
    void restore(const ObjectFLEDPadRegs& regs) FL_NO_EXCEPT {
        if (!mCaptured) {
            return;
        }
        const u32 inputs = mMask & ~mGdir;
        const u32 outputs = mGdir;
        *regs.gpr26 = (*regs.gpr26 & ~mMask) | mGpr26;
        *regs.gpio1_gdir = *regs.gpio1_gdir & ~inputs;
        // Through DR_SET/DR_CLEAR only: a read-modify-write of DR would copy
        // the pad level of unrelated GPIO1 inputs into their latches.
        *regs.gpio1_dr_set = mDr;
        *regs.gpio1_dr_clear = outputs & ~mDr;
        *regs.gpio1_gdir = (*regs.gpio1_gdir & ~mMask) | mGdir;
        *regs.mosi_pad = mMosiPad;
        *regs.sclk_pad = mSclkPad;
        *regs.mosi_mux = mMosiMux;
        *regs.sclk_mux = mSclkMux;
        mCaptured = false;
    }

    bool captured() const FL_NO_EXCEPT { return mCaptured; }

  private:
    u32 mMask = 0;
    u32 mGpr26 = 0;
    u32 mGdir = 0;
    u32 mDr = 0;
    u32 mMosiMux = 0;
    u32 mMosiPad = 0;
    u32 mSclkMux = 0;
    u32 mSclkPad = 0;
    bool mCaptured = false;
};

/// Registers for one pad, for the clockless mode, which claims each output pin
/// on its own. Both pad slots of ObjectFLEDPadRegs name the same mux and
/// pad-control registers, so ObjectFLEDPadState snapshots and restores a single
/// pad. `gpr` is the pad's bank select, IOMUXC_GPR_GPR26..29 for GPIO6..9, and
/// the GPIO registers are those of the GPIO1..4 bank it is remapped to.
inline ObjectFLEDPadRegs objectfled_single_pad_regs(
    volatile u32* gpr, volatile u32* gdir, volatile u32* dr, volatile u32* dr_set,
    volatile u32* dr_clear, volatile u32* mux, volatile u32* pad) FL_NO_EXCEPT {
    return ObjectFLEDPadRegs{gpr, gdir, dr, dr_set, dr_clear, mux, pad, mux, pad};
}

/// What clockless ObjectFLED begin() does to one output pin: snapshot the pad
/// (once; a repeated begin() keeps the first snapshot), take the mux, set the
/// pad's SPEED/DSE/SRE bits, remap it to GPIO1..4 and make it an output there.
inline void objectfled_clockless_claim_pad(ObjectFLEDPadState& state,
                                           const ObjectFLEDPadRegs& regs, u32 mask,
                                           u32 mux, u32 pad_ctl) FL_NO_EXCEPT {
    if (!state.captured()) {
        state.capture(regs, mask);
    }
    *regs.mosi_mux = mux;
    constexpr u32 kSpeedDseSre = 0xF9U;  // bits 7:6 SPEED, 5:3 DSE, 0 SRE
    *regs.mosi_pad = (*regs.mosi_pad & ~kSpeedDseSre) | (pad_ctl & kSpeedDseSre);
    *regs.gpr26 = *regs.gpr26 & ~mask;
    *regs.gpio1_gdir = *regs.gpio1_gdir | mask;
}

/// Gives back every pad claimed through objectfled_clockless_claim_pad(), in
/// reverse claim order so a pin listed twice ends in its first, pre-driver
/// snapshot. `regs_for(i)` returns slot i's registers; it is only called for
/// slots that hold a snapshot. Call only once the DMA has stopped.
template <typename RegsFor>
void objectfled_clockless_release_pads(ObjectFLEDPadState* states, u32 count,
                                       RegsFor regs_for) FL_NO_EXCEPT {
    for (u32 i = count; i-- > 0;) {
        if (states[i].captured()) {
            states[i].restore(regs_for(i));
        }
    }
}

}  // namespace fl

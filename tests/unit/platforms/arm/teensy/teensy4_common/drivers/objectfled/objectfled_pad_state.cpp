/// @file objectfled_pad_state.cpp
/// @brief The pad snapshot ObjectFLED's SPI mode takes at init and restores at
/// deinit, run against plain variables standing in for the IMXRT registers.
///
/// A small pad model decides what each pad shows. With the mux on ALT5 (GPIO),
/// a pad follows GPIO6 when its GPR26 bit is set and GPIO1 when it is clear.
/// Ordinary Teensy GPIO (pinMode/digitalWrite, FastPin) only ever writes GPIO6,
/// so the tests write through GPIO6 after deinit and check the pad moves.

#include "test.h"

#include "fl/stl/int.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_pad_state.h"

namespace fl {

FL_TEST_FILE(FL_FILEPATH) {

namespace {

constexpr u32 kAlt5 = 5U;           // GPIO
constexpr u32 kAlt5Sion = 5U | 0x10U;  // What Teensy's pinMode writes.
constexpr u32 kMosiBit = 18;        // Teensy 4.x pin 14 is GPIO6 bit 18.
constexpr u32 kSclkBit = 19;        // Pin 15.
constexpr u32 kMosi = 1U << kMosiBit;
constexpr u32 kSclk = 1U << kSclkBit;
constexpr u32 kBoth = kMosi | kSclk;

struct Chip {
    volatile u32 gpr26 = 0;
    volatile u32 gpio1_gdir = 0;
    volatile u32 gpio1_dr = 0;
    volatile u32 gpio1_dr_set = 0;    // Write-only: OR into gpio1_dr on latch().
    volatile u32 gpio1_dr_clear = 0;  // Write-only: clear from gpio1_dr on latch().
    volatile u32 gpio6_gdir = 0;
    volatile u32 gpio6_dr = 0;
    volatile u32 mosi_mux = 0;
    volatile u32 mosi_pad = 0;
    volatile u32 sclk_mux = 0;
    volatile u32 sclk_pad = 0;

    ObjectFLEDPadRegs regs() {
        return ObjectFLEDPadRegs{&gpr26, &gpio1_gdir, &gpio1_dr, &gpio1_dr_set,
                                 &gpio1_dr_clear, &mosi_mux, &mosi_pad,
                                 &sclk_mux, &sclk_pad};
    }

    // What the hardware does with the DR_SET/DR_CLEAR writes: touch only the
    // written bits of the latch.
    void latch() {
        gpio1_dr = (gpio1_dr | gpio1_dr_set) & ~gpio1_dr_clear;
        gpio1_dr_set = 0;
        gpio1_dr_clear = 0;
    }

    // What a GPIO pad drives: -1 when it is not a driven GPIO output.
    int padLevel(u32 bit, u32 mux) const {
        if ((mux & 7U) != kAlt5) {
            return -1;
        }
        const u32 m = 1U << bit;
        const bool onGpio6 = (gpr26 & m) != 0;
        const u32 dir = onGpio6 ? gpio6_gdir : gpio1_gdir;
        const u32 out = onGpio6 ? gpio6_dr : gpio1_dr;
        if ((dir & m) == 0) {
            return -1;
        }
        return (out & m) ? 1 : 0;
    }
    int mosiLevel() const { return padLevel(kMosiBit, mosi_mux); }
    int sclkLevel() const { return padLevel(kSclkBit, sclk_mux); }

    // Teensy's pinMode(pin, OUTPUT) and digitalWrite(): GPIO6 only, never GPR26.
    void ordinaryOutput(u32 bit, volatile u32& mux, volatile u32& pad) {
        mux = kAlt5Sion;
        pad = 0x10B0U;
        gpio6_gdir = gpio6_gdir | (1U << bit);
    }
    void ordinaryWrite(u32 bit, bool high) {
        gpio6_dr = high ? (gpio6_dr | (1U << bit)) : (gpio6_dr & ~(1U << bit));
    }
};

// What the SPI mode's ownership does to the pads: remap to GPIO1, drive both
// low as outputs, take the mux and pad config, then toggle data on GPIO1.
// Arbitrary on purpose: restore must undo any of it.
void own(Chip& c) {
    c.gpr26 = c.gpr26 & ~kBoth;
    c.gpio1_gdir = c.gpio1_gdir | kBoth;
    c.gpio1_dr = (c.gpio1_dr & ~kBoth) | kSclk;
    c.mosi_mux = kAlt5;
    c.sclk_mux = kAlt5;
    c.mosi_pad = 0x18U;
    c.sclk_pad = 0x18U;
}

// GPIO1 latch bits own() leaves on the pads.
constexpr u32 kOwnDr = kSclk;

// Every register as it should read after restore, given the starting chip and
// unrelated bits that changed while the mode owned the pads. A pad saved as a
// GPIO1 output gets its old latch back; a pad saved as an input keeps the
// latch the mode left (DR reads the pad level for inputs, so the old latch is
// unknowable and irrelevant while the pin is an input).
void expectRestored(const Chip& before, const Chip& after, u32 gprOthers, u32 gdirOthers,
                    u32 drOthers) {
    const u32 savedOutputs = before.gpio1_gdir & kBoth;
    const u32 savedInputs = kBoth & ~savedOutputs;
    FL_CHECK_EQ(after.gpr26, (before.gpr26 & kBoth) | gprOthers);
    FL_CHECK_EQ(after.gpio1_gdir, savedOutputs | gdirOthers);
    FL_CHECK_EQ(after.gpio1_dr,
                (before.gpio1_dr & savedOutputs) | (kOwnDr & savedInputs) | drOthers);
    FL_CHECK_EQ(after.gpio6_gdir, before.gpio6_gdir);
    FL_CHECK_EQ(after.gpio6_dr, before.gpio6_dr);
    FL_CHECK_EQ(after.mosi_mux, before.mosi_mux);
    FL_CHECK_EQ(after.sclk_mux, before.sclk_mux);
    FL_CHECK_EQ(after.mosi_pad, before.mosi_pad);
    FL_CHECK_EQ(after.sclk_pad, before.sclk_pad);
}

void copyChip(Chip& dst, const Chip& src) {
    dst.gpr26 = src.gpr26;
    dst.gpio1_gdir = src.gpio1_gdir;
    dst.gpio1_dr = src.gpio1_dr;
    dst.gpio6_gdir = src.gpio6_gdir;
    dst.gpio6_dr = src.gpio6_dr;
    dst.mosi_mux = src.mosi_mux;
    dst.mosi_pad = src.mosi_pad;
    dst.sclk_mux = src.sclk_mux;
    dst.sclk_pad = src.sclk_pad;
}

} // namespace

FL_TEST_CASE("pads that started on GPIO6 work with ordinary GPIO after deinit") {
    Chip c;
    // Teensy's startup state: every pad on GPIO6 (GPR26 all ones).
    c.gpr26 = 0xFFFFFFFFU;
    c.gpio6_gdir = kBoth;
    c.gpio6_dr = kMosi;
    c.mosi_mux = kAlt5Sion;
    c.sclk_mux = kAlt5Sion;
    c.mosi_pad = 0x10B0U;
    c.sclk_pad = 0x10B0U;
    Chip before;
    copyChip(before, c);

    ObjectFLEDPadState state;
    state.capture(c.regs(), kBoth);
    own(c);
    FL_CHECK_EQ(c.mosiLevel(), 0); // While owned, the pads follow GPIO1.
    FL_CHECK_EQ(c.sclkLevel(), 1);
    state.restore(c.regs());
    c.latch();
    expectRestored(before, c, 0xFFFFFFFFU & ~kBoth, 0, 0);
    FL_CHECK_EQ(c.mosiLevel(), 1); // Back on GPIO6 with its old latch.
    FL_CHECK_EQ(c.sclkLevel(), 0);

    // Ordinary GPIO on the same pins reaches the pads again.
    c.ordinaryOutput(kMosiBit, c.mosi_mux, c.mosi_pad);
    c.ordinaryOutput(kSclkBit, c.sclk_mux, c.sclk_pad);
    c.ordinaryWrite(kMosiBit, false);
    c.ordinaryWrite(kSclkBit, true);
    FL_CHECK_EQ(c.mosiLevel(), 0);
    FL_CHECK_EQ(c.sclkLevel(), 1);
    c.ordinaryWrite(kSclkBit, false);
    FL_CHECK_EQ(c.sclkLevel(), 0);
}

FL_TEST_CASE("other prior states come back exactly: GPIO1 bank, another mux, inputs") {
    Chip c;
    // MOSI already on GPIO1 (bit clear) as a high output; SCLK on ALT3 with
    // its GPR26 bit set and as an input.
    c.gpr26 = kSclk | 0x5U;
    c.gpio1_gdir = kMosi | 0x100U;
    c.gpio1_dr = kMosi | 0x200U;
    c.gpio6_gdir = 0x3U;
    c.gpio6_dr = 0x1U;
    c.mosi_mux = kAlt5;
    c.sclk_mux = 3U;
    c.mosi_pad = 0x1234U;
    c.sclk_pad = 0x5678U;
    Chip before;
    copyChip(before, c);

    ObjectFLEDPadState state;
    state.capture(c.regs(), kBoth);
    own(c);
    state.restore(c.regs());
    c.latch();
    expectRestored(before, c, 0x5U, 0x100U, 0x200U);
    FL_CHECK_EQ(c.mosiLevel(), 1);
    FL_CHECK_EQ(c.sclkLevel(), -1); // ALT3 again: not GPIO.
}

FL_TEST_CASE("an input pin that reads high is released without a high pulse") {
    Chip c;
    // Both pads on GPIO1 (GPR26 bits clear). MOSI is an input whose pad reads
    // high, so DR shows kMosi set although the latch is low; SCLK is a high
    // output whose latch must come back.
    c.gpr26 = 0;
    c.gpio1_gdir = kSclk;
    c.gpio1_dr = kMosi | kSclk;
    c.mosi_mux = kAlt5;
    c.sclk_mux = kAlt5;
    Chip before;
    copyChip(before, c);

    ObjectFLEDPadState state;
    state.capture(c.regs(), kBoth);
    own(c);
    c.gpio1_dr = c.gpio1_dr & ~kSclk; // The mode parks SCLK low at deinit.
    FL_CHECK_EQ(c.gpio1_dr & kMosi, 0U);
    state.restore(c.regs());
    c.latch();
    // The sampled input level never reaches the latch: with the old order the
    // pad, still an output from the mode, would have been driven high.
    FL_CHECK_EQ(c.gpio1_dr & kMosi, 0U);
    FL_CHECK_EQ(c.gpio1_gdir & kMosi, 0U);
    FL_CHECK_EQ(c.mosiLevel(), -1);
    // The saved output's high latch and direction come back.
    FL_CHECK_EQ(c.gpio1_dr & kSclk, kSclk);
    FL_CHECK_EQ(c.gpio1_gdir & kSclk, kSclk);
    FL_CHECK_EQ(c.sclkLevel(), 1);
}

FL_TEST_CASE("restore never rewrites the whole GPIO1 latch") {
    Chip c;
    // An unrelated GPIO1 input (0x400) reads high while its latch is low.
    // Writing DR back would latch it high, so a later switch to output would
    // drive the pin high. Only DR_SET/DR_CLEAR, limited to the saved outputs,
    // may be used.
    constexpr u32 kOtherInput = 0x400U;
    c.gpr26 = 0;
    c.gpio1_gdir = kSclk;
    c.gpio1_dr = kSclk;
    c.mosi_mux = kAlt5;
    c.sclk_mux = kAlt5;
    ObjectFLEDPadState state;
    state.capture(c.regs(), kBoth);
    own(c);
    c.gpio1_dr = (c.gpio1_dr & ~kSclk) | kOtherInput;  // SCLK parked low.
    state.restore(c.regs());
    FL_CHECK_EQ(c.gpio1_dr, kOtherInput);  // DR itself was not written.
    FL_CHECK_EQ(c.gpio1_dr_set, kSclk);
    FL_CHECK_EQ(c.gpio1_dr_clear, 0U);
}

FL_TEST_CASE("unrelated bits that change during ownership are kept") {
    Chip c;
    c.gpr26 = 0xFFFFFFFFU;
    c.mosi_mux = kAlt5Sion;
    c.sclk_mux = kAlt5Sion;
    Chip before;
    copyChip(before, c);

    ObjectFLEDPadState state;
    state.capture(c.regs(), kBoth);
    own(c);
    // Another driver remaps and drives other pads in the meantime.
    c.gpr26 = c.gpr26 & ~0x3U;
    c.gpio1_gdir = c.gpio1_gdir | 0x3U;
    c.gpio1_dr = c.gpio1_dr | 0x1U;
    state.restore(c.regs());
    c.latch();
    expectRestored(before, c, 0xFFFFFFFFU & ~kBoth & ~0x3U, 0x3U, 0x1U);
}

FL_TEST_CASE("restore is one-shot, needs a capture, and repeats cleanly") {
    Chip c;
    c.gpr26 = 0xFFFFFFFFU;
    c.mosi_mux = kAlt5Sion;
    c.sclk_mux = kAlt5Sion;
    ObjectFLEDPadState state;
    FL_CHECK_FALSE(state.captured());
    own(c);
    state.restore(c.regs()); // Nothing captured: nothing written.
    FL_CHECK_EQ(c.gpr26, 0xFFFFFFFFU & ~kBoth);
    FL_CHECK_EQ(c.mosi_mux, kAlt5);

    c.gpr26 = 0xFFFFFFFFU;
    c.mosi_mux = kAlt5Sion;
    for (int cycle = 0; cycle < 3; ++cycle) {
        state.capture(c.regs(), kBoth);
        FL_CHECK(state.captured());
        own(c);
        state.restore(c.regs());
    c.latch();
        FL_CHECK_FALSE(state.captured());
        FL_CHECK_EQ(c.gpr26, 0xFFFFFFFFU);
        FL_CHECK_EQ(c.mosi_mux, kAlt5Sion);
    }
    // A second restore after a deinit must not undo later changes.
    c.gpr26 = 0;
    state.restore(c.regs());
    c.latch();
    FL_CHECK_EQ(c.gpr26, 0U);
}

} // FL_TEST_FILE

} // namespace fl

#pragma once

// GPIO instrumentation only: all bit order, loops, port words and delays are
// supplied by the included production GenericSoftwareSPIOutput implementation.
#include "fl/stl/stdint.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "fl/system/fastpin.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace { // anonymous namespace for generic_software_spi fixture

template <int Model> struct SpiTraceState {
    using Word = fl::conditional_t<Model == 0 || Model == 1 || Model == 8,
        fl::u8, fl::conditional_t<Model == 2 || Model == 3 || Model == 9,
        fl::u16, fl::u32>>;
    static constexpr bool kSeparate = Model == 1 || Model == 3 || Model == 5 || Model == 7 || Model >= 16;
    static constexpr bool kDataInverted = Model == 6 || Model == 11 || Model == 12 || Model == 14 || Model >= 16;
    static constexpr bool kClockInverted = Model == 6 || Model == 11 || Model == 13 || Model == 15 || Model >= 16;
    static constexpr bool kStream = (Model >= 8 && Model <= 11) || Model == 14 || Model == 15;
    static constexpr int kClockBank = kSeparate ? 1 : 0;
    // Wider models exercise upper GPIO bits, not merely a wider typedef.
    static constexpr int kDataBit = sizeof(Word) == 1 ? 0 : (sizeof(Word) == 2 ? 8 : 16);
    static constexpr Word kDataMask = static_cast<Word>(fl::u32{1} << kDataBit);
    static constexpr Word kClockMask = static_cast<Word>(fl::u32{1} << (kDataBit + 1));
    static constexpr Word kSentinel = static_cast<Word>((fl::u32{1} << (sizeof(Word) * 8 - 1)) | 0x80);

    volatile Word ports[2] = {};
    fl::vector<fl::u8> sampled;
    int writes = 0;
    fl::vector<fl::cycle_t> high_cycles;
    fl::vector<fl::cycle_t> low_cycles;
    int high_delays = 0;
    int low_delays = 0;
    bool last_clock = false;
    bool last_data = false;
    bool data_stable = true;
    bool preserved = true;
    bool interference = false;
    bool injected = false;
    bool selected = false;
    bool require_selected = false;
    bool selection_valid = true;
    int selects = 0;
    int releases = 0;

    static SpiTraceState& get() {
        static SpiTraceState state;
        return state;
    }
    bool logicalData() const { return bool(ports[0] & kDataMask) != kDataInverted; }
    bool logicalClock() const { return bool(ports[kClockBank] & kClockMask) != kClockInverted; }

    void reset() {
        ports[0] = static_cast<Word>(kSentinel | (kDataInverted ? kDataMask : 0));
        ports[1] = kSentinel;
        if (kClockInverted) ports[kClockBank] = static_cast<Word>(ports[kClockBank] | kClockMask);
        interference = false;
        injected = false;
        require_selected = false;
        clearTrace();
    }
    void clearTrace() {
        sampled.clear();
        writes = 0;
        high_cycles.clear();
        low_cycles.clear();
        high_delays = 0;
        low_delays = 0;
        last_clock = logicalClock();
        last_data = logicalData();
        data_stable = true;
        preserved = true;
        selected = false;
        selection_valid = true;
        selects = 0;
        releases = 0;
    }
    void record() {
        ++writes;
        if (require_selected && !selected) selection_valid = false;
        const bool clock = logicalClock();
        const bool data = logicalData();
        if (clock && !last_clock) {
            sampled.push_back(data ? 1 : 0);
            if (data != last_data) data_stable = false;
        }
        last_clock = clock;
        last_data = data;
        for (int bank = 0; bank < 2; ++bank) {
            const Word owned = static_cast<Word>((bank == 0 ? kDataMask : 0) |
                (bank == kClockBank ? kClockMask : 0));
            const Word unrelated = static_cast<Word>(~owned);
            const Word expected = static_cast<Word>(kSentinel | (injected ? 0x40 : 0));
            if ((ports[bank] & unrelated) != expected) preserved = false;
        }
        // Simulate an unrelated GPIO owner after the first production store,
        // including after bulk paths have captured their whole-port snapshots.
        if (interference && !injected) {
            ports[0] = static_cast<Word>(ports[0] | 0x40);
            ports[1] = static_cast<Word>(ports[1] | 0x40);
            injected = true;
        }
    }
};

template <int Model, bool Clock> class SpiTracePin {
  public:
    using State = SpiTraceState<Model>;
    using port_t = typename State::Word;
    using port_ptr_t = volatile port_t*;
    static constexpr int kBank = Clock ? State::kClockBank : 0;
    static constexpr bool kInverted = Clock ? State::kClockInverted : State::kDataInverted;
    static constexpr port_t mask() { return Clock ? State::kClockMask : State::kDataMask; }
    static constexpr bool validpin() { return true; }
    static void setOutput() {}
    static void setInput() {}
    static port_ptr_t port() { return &State::get().ports[kBank]; }
    static port_t hival() {
        return kInverted ? static_cast<port_t>(*port() & ~mask())
                                : static_cast<port_t>(*port() | mask());
    }
    static port_t loval() {
        return kInverted ? static_cast<port_t>(*port() | mask())
                                : static_cast<port_t>(*port() & ~mask());
    }
    static void fastset(port_ptr_t destination, port_t value) {
        *destination = value;
        State::get().record();
    }
    static void hi() { fastset(port(), hival()); }
    static void lo() { fastset(port(), loval()); }
    static void hi(port_ptr_t) { hi(); }
    static void lo(port_ptr_t) { lo(); }
    static void set(port_t value) { fastset(port(), value); }
    static void toggle() {
        fastset(port(), static_cast<port_t>(*port() ^ mask()));
    }
};

template <int Model, class Pin> struct SpiTracePolicy : fl::BitbangTiming {
    static constexpr bool kAtomicWrite = Model == 7 || Model == 16;
    static constexpr bool kTwoWriteStream = SpiTraceState<Model>::kStream;
    static constexpr fl::cycle_t kMinWriteCycles = 0;
    template <fl::cycle_t Cycles> static void delay() {
        FL_STATIC_ASSERT(Cycles >= 0, "negative requested GPIO padding");
        auto& state = SpiTraceState<Model>::get();
        if (state.logicalClock()) {
            ++state.high_delays;
            state.high_cycles.push_back(Cycles);
        } else {
            ++state.low_delays;
            state.low_cycles.push_back(Cycles);
        }
    }
    template <fl::u8 Bit> static void write(fl::u8 value) {
        // A pin write is the instrumentation boundary, not a copied SPI loop.
        if (value & (1u << Bit)) Pin::hi();
        else Pin::lo();
    }
};

template <int Model> class SpiTraceSelect : public fl::Selectable {
  public:
    void select() FL_NO_EXCEPT override {
        auto& state = SpiTraceState<Model>::get();
        if (state.selected || state.writes != 0) state.selection_valid = false;
        state.selected = true;
        ++state.selects;
    }
    void release() FL_NO_EXCEPT override {
        auto& state = SpiTraceState<Model>::get();
        if (!state.selected || state.logicalClock()) state.selection_valid = false;
        state.selected = false;
        ++state.releases;
    }
    bool isSelected() FL_NO_EXCEPT override { return SpiTraceState<Model>::get().selected; }
};

} // anonymous namespace for generic_software_spi fixture

namespace fl {
// Reserved test-only logical pins. These are model semantics, not claims about
// any vendor's pin map, peripheral register layout or physical timing.
#define FL_SPI_TRACE_MODEL(MODEL, DATA, CLOCK)                                  \
    template <> class FastPin<DATA> : public ::SpiTracePin<MODEL, false> {};     \
    template <> class FastPin<CLOCK> : public ::SpiTracePin<MODEL, true> {};     \
    template <> struct BitbangPin<FastPin<DATA>>                              \
        : ::SpiTracePolicy<MODEL, FastPin<DATA>> {};                           \
    template <> struct BitbangPin<FastPin<CLOCK>>                             \
        : ::SpiTracePolicy<MODEL, FastPin<CLOCK>> {};

FL_SPI_TRACE_MODEL(0, 220, 221)
FL_SPI_TRACE_MODEL(1, 222, 223)
FL_SPI_TRACE_MODEL(2, 224, 225)
FL_SPI_TRACE_MODEL(3, 226, 227)
FL_SPI_TRACE_MODEL(4, 228, 229)
FL_SPI_TRACE_MODEL(5, 230, 231)
FL_SPI_TRACE_MODEL(6, 232, 233)
FL_SPI_TRACE_MODEL(7, 234, 235)
FL_SPI_TRACE_MODEL(8, 236, 237)
FL_SPI_TRACE_MODEL(9, 238, 239)
FL_SPI_TRACE_MODEL(10, 240, 241)
FL_SPI_TRACE_MODEL(11, 242, 243)
FL_SPI_TRACE_MODEL(12, 244, 245)
FL_SPI_TRACE_MODEL(13, 246, 247)
FL_SPI_TRACE_MODEL(14, 248, 249)
FL_SPI_TRACE_MODEL(15, 250, 251)
FL_SPI_TRACE_MODEL(16, 252, 253)
FL_SPI_TRACE_MODEL(17, 254, 255)

#undef FL_SPI_TRACE_MODEL
} // namespace fl

namespace { // anonymous namespace for generic_software_spi fixture

template <int Model, fl::u32 Divider = 16> class SpiTraceFixture {
  public:
    using Output = fl::GenericSoftwareSPIOutput<220 + Model * 2, 221 + Model * 2, Divider>;
    void reset() { state().reset(); }
    void clearTrace() { state().clearTrace(); }
    const fl::vector<fl::u8>& bits() const { return state().sampled; }
    int gpioWrites() const { return state().writes; }
    bool clockLow() const { return !state().logicalClock(); }
    bool unrelatedPreserved() const { return state().preserved; }
    bool dataStable() const { return state().data_stable; }
    const fl::vector<fl::cycle_t>& highCycles() const { return state().high_cycles; }
    const fl::vector<fl::cycle_t>& lowCycles() const { return state().low_cycles; }
    int highDelayCalls() const { return state().high_delays; }
    int lowDelayCalls() const { return state().low_delays; }
    void injectInterference(bool enabled) { state().interference = enabled; }
    fl::Selectable* selectable() {
        state().require_selected = true;
        return &mSelect;
    }
    bool selectionOrderValid() const {
        return state().selection_valid && state().selects == 1 &&
            state().releases == 1 && !state().selected;
    }

  private:
    static SpiTraceState<Model>& state() { return SpiTraceState<Model>::get(); }
    SpiTraceSelect<Model> mSelect;
};

} // anonymous namespace for generic_software_spi fixture

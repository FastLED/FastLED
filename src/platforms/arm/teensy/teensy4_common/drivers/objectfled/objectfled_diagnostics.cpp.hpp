// IWYU pragma: private

// FL_LINT_ALLOW_GLOBAL_FILE(Tier 1d: ObjectFLED diagnostic snapshot state (s_events / s_lastPins / s_busyState + counters) is used from 40+ sites in this TU. Mechanical migration to `fl::Singleton<DiagState>` is tracked in FastLED#3487 — kept as-is here to keep the linter-landing PR focused.)
#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X) && defined(FASTLED_OBJECTFLED_DIAGNOSTICS) && FASTLED_OBJECTFLED_DIAGNOSTICS

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_diagnostics.h"

#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/stl/sstream.h"
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLEDDmaManager.h"

#include <Arduino.h>
#include "DMAChannel.h"  // ok include path - Teensy core header
// IWYU pragma: end_keep

namespace fl {
namespace {

constexpr u8 kMaxEvents = 16;
constexpr u8 kMaxPins = 16;

struct PinRegisterSnapshot {
    u8 pin = 0;
    u8 bit = 0;
    u8 fastGpio = 0;
    u8 standardGpio = 0;
    u32 mask = 0;
    u32 gpr = 0;
    u32 gprMask = 0;
    bool mappedToStandard = false;
    u32 muxRegister = 0;
    u32 mux = 0;
    u32 padRegister = 0;
    u32 pad = 0;
    u32 outputRegister = 0;
    u32 outputValue = 0;
    u32 modeRegister = 0;
    u32 modeValue = 0;
    u32 inputRegister = 0;
    u32 inputValue = 0;
};

struct TimerChannelSnapshot {
    u32 ctrl = 0;
    u32 sctrl = 0;
    u32 csctrl = 0;
    u32 cntr = 0;
    u32 load = 0;
    u32 comp1 = 0;
    u32 comp2 = 0;
    u32 cmpld1 = 0;
    u32 cmpld2 = 0;
};

struct TcdSnapshot {
    u32 tcdAddress = 0;
    u32 saddr = 0;
    i32 soff = 0;
    u32 attr = 0;
    u32 nbytes = 0;
    i32 slast = 0;
    u32 daddr = 0;
    i32 doff = 0;
    u32 citer = 0;
    i32 dlastsga = 0;
    u32 csr = 0;
    u32 biter = 0;
};

struct DmaChannelSnapshot {
    const char* name = "";
    bool hasHardwareChannel = false;
    u8 channel = 0;
    u32 dmamuxChcfg = 0;
    TcdSnapshot tcd;
};

struct DmaSnapshot {
    u32 es = 0;
    u32 erq = 0;
    u32 err = 0;
    u32 irq = 0;
    u32 hrs = 0;
    u32 allocatedMask = 0;
    u32 currentOwner = 0;
    u32 frameBuffer = 0;
    u32 bitdata = 0;
    u32 bitmask = 0;
    u32 framebufferIndex = 0;
    u32 numbytes = 0;
    u32 numpins = 0;
    u32 bitmaskWords[4] = {0, 0, 0, 0};
    DmaChannelSnapshot dma1;
    DmaChannelSnapshot dma2;
    DmaChannelSnapshot dma3;
    DmaChannelSnapshot dma2next;
};

struct BusyStateSnapshot {
    bool dmaActive = false;
    bool latchActive = false;
    bool ready = true;
};

struct SnapshotEvent {
    const char* stage = "";
    u32 millisValue = 0;
    u32 microsValue = 0;
    u8 pinCount = 0;
    PinRegisterSnapshot pins[kMaxPins];
    u32 gpr[4] = {0, 0, 0, 0};
    TimerChannelSnapshot tmr[3];
    u32 tmrEnbl = 0;
    u32 xbarSel15 = 0;
    u32 xbarSel47 = 0;
    u32 xbarSel0 = 0;
    u32 xbarSel1 = 0;
    u32 xbarCtrl0 = 0;
    u32 xbarCtrl1 = 0;
    DmaSnapshot dma;
    BusyStateSnapshot busy;
    // RX-pin probe: snapshot the configured RX pin's pad status across the
    // run so afterShowReturn (captured while DMA is still actively driving
    // TX) can prove whether the RX pad sees any transition.
    bool rxPinValid = false;
    u8 rxPin = 0;
    u8 rxBit = 0;
    u32 rxFastPsr = 0;
    u32 rxStandardPsr = 0;
};

SnapshotEvent s_events[kMaxEvents];
u8 s_eventCount = 0;
u32 s_overflowCount = 0;
u8 s_lastPins[kMaxPins];
u8 s_lastPinCount = 0;
BusyStateSnapshot s_busyState;
bool s_rxPinValid = false;
u8 s_rxPin = 0;

u32 ptrToU32(const volatile void* ptr) FL_NO_EXCEPT {
    return static_cast<u32>(fl::ptr_to_int(const_cast<void*>(ptr)));
}

void setU32(fl::json& out, const char* key, u32 value) FL_NO_EXCEPT {
    out.set(key, static_cast<i64>(value));
}

volatile u32* standardGpioAddr(volatile u32* fastgpio) FL_NO_EXCEPT {
    return reinterpret_cast<volatile u32*>( // ok reinterpret cast - Teensy GPIO alias address map
        ptrToU32(fastgpio) - 0x01E48000u);
}

PinRegisterSnapshot capturePin(u8 pin) FL_NO_EXCEPT {
    PinRegisterSnapshot out;
    out.pin = pin;
    if (pin >= NUM_DIGITAL_PINS) {
        return out;
    }

    out.bit = digitalPinToBit(pin);
    out.mask = 1u << out.bit;

    volatile u32* outputReg = portOutputRegister(pin);
    volatile u32* modeReg = portModeRegister(pin);
    volatile u32* inputReg = portInputRegister(pin);
    volatile u32* muxReg = portConfigRegister(pin);
    volatile u32* padReg = portControlRegister(pin);

    out.outputRegister = ptrToU32(outputReg);
    out.outputValue = *outputReg;
    out.modeRegister = ptrToU32(modeReg);
    out.modeValue = *modeReg;
    out.inputRegister = ptrToU32(inputReg);
    out.inputValue = *inputReg;
    out.muxRegister = ptrToU32(muxReg);
    out.mux = *muxReg;
    out.padRegister = ptrToU32(padReg);
    out.pad = *padReg;

    const u32 gpio6Base = ptrToU32(&GPIO6_DR);
    const u32 outputAddress = ptrToU32(outputReg);
    const u8 offset = static_cast<u8>((outputAddress - gpio6Base) >> 14);
    if (offset <= 3) {
        out.fastGpio = static_cast<u8>(6 + offset);
        out.standardGpio = static_cast<u8>(1 + offset);
        volatile u32* gprReg = &IOMUXC_GPR_GPR26 + offset;
        out.gpr = *gprReg;
        out.gprMask = out.mask;
        out.mappedToStandard = ((*gprReg & out.mask) == 0);

        volatile u32* standardModeReg = standardGpioAddr(modeReg);
        out.modeRegister = ptrToU32(standardModeReg);
        out.modeValue = *standardModeReg;
    }
    return out;
}

TimerChannelSnapshot captureTimerChannel(u8 index) FL_NO_EXCEPT {
    TimerChannelSnapshot out;
    if (index == 0) {
        out.ctrl = TMR4_CTRL0;
        out.sctrl = TMR4_SCTRL0;
        out.csctrl = TMR4_CSCTRL0;
        out.cntr = TMR4_CNTR0;
        out.load = TMR4_LOAD0;
        out.comp1 = TMR4_COMP10;
        out.comp2 = TMR4_COMP20;
        out.cmpld1 = TMR4_CMPLD10;
        out.cmpld2 = TMR4_CMPLD20;
    } else if (index == 1) {
        out.ctrl = TMR4_CTRL1;
        out.sctrl = TMR4_SCTRL1;
        out.csctrl = TMR4_CSCTRL1;
        out.cntr = TMR4_CNTR1;
        out.load = TMR4_LOAD1;
        out.comp1 = TMR4_COMP11;
        out.comp2 = TMR4_COMP21;
        out.cmpld1 = TMR4_CMPLD11;
        out.cmpld2 = TMR4_CMPLD21;
    } else {
        out.ctrl = TMR4_CTRL2;
        out.sctrl = TMR4_SCTRL2;
        out.csctrl = TMR4_CSCTRL2;
        out.cntr = TMR4_CNTR2;
        out.load = TMR4_LOAD2;
        out.comp1 = TMR4_COMP12;
        out.comp2 = TMR4_COMP22;
        out.cmpld1 = TMR4_CMPLD12;
        out.cmpld2 = TMR4_CMPLD22;
    }
    return out;
}

TcdSnapshot captureTcd(const DMABaseClass::TCD_t* tcd) FL_NO_EXCEPT {
    TcdSnapshot out;
    if (tcd == nullptr) {
        return out;
    }
    out.tcdAddress = ptrToU32(tcd);
    out.saddr = ptrToU32(tcd->SADDR);
    out.soff = tcd->SOFF;
    out.attr = tcd->ATTR;
    out.nbytes = tcd->NBYTES;
    out.slast = tcd->SLAST;
    out.daddr = ptrToU32(tcd->DADDR);
    out.doff = tcd->DOFF;
    out.citer = tcd->CITER;
    out.dlastsga = tcd->DLASTSGA;
    out.csr = tcd->CSR;
    out.biter = tcd->BITER;
    return out;
}

u32 captureDmamuxChcfg(u8 channel) FL_NO_EXCEPT {
    volatile u32* dmamux = &DMAMUX_CHCFG0 + channel;
    return *dmamux;
}

DmaChannelSnapshot captureDmaChannel(const char* name, const DMAChannel& channel) FL_NO_EXCEPT {
    DmaChannelSnapshot out;
    out.name = name;
    out.hasHardwareChannel = true;
    out.channel = channel.channel;
    out.dmamuxChcfg = captureDmamuxChcfg(channel.channel);
    out.tcd = captureTcd(channel.TCD);
    return out;
}

DmaChannelSnapshot captureDmaSetting(const char* name, const DMASetting& setting) FL_NO_EXCEPT {
    DmaChannelSnapshot out;
    out.name = name;
    out.hasHardwareChannel = false;
    out.tcd = captureTcd(setting.TCD);
    return out;
}

DmaSnapshot captureDma() FL_NO_EXCEPT {
    auto& dma = ObjectFLEDDmaManager::getInstance();
    DmaSnapshot out;
    out.es = DMA_ES;
    out.erq = DMA_ERQ;
    out.err = DMA_ERR;
    out.irq = DMA_INT;
    out.hrs = DMA_HRS;
    out.allocatedMask = dma_channel_allocated_mask;
    out.currentOwner = ptrToU32(dma.getCurrentOwner());
    out.frameBuffer = ptrToU32(dma.frameBuffer);
    out.bitdata = ptrToU32(dma.bitdata);
    out.bitmask = ptrToU32(dma.bitmask);
    out.framebufferIndex = dma.framebuffer_index;
    out.numbytes = dma.numbytes;
    out.numpins = dma.numpins;
    for (u8 i = 0; i < 4; ++i) {
        out.bitmaskWords[i] = dma.bitmask[i];
    }
    out.dma1 = captureDmaChannel("dma1", dma.dma1);
    out.dma2 = captureDmaChannel("dma2", dma.dma2);
    out.dma3 = captureDmaChannel("dma3", dma.dma3);
    out.dma2next = captureDmaSetting("dma2next", dma.dma2next);
    return out;
}

void appendTcd(fl::sstream& out, const char* name,
               const TcdSnapshot& tcd) FL_NO_EXCEPT {
    out << ' ' << name << ".tcd=" << tcd.tcdAddress
        << " saddr=" << tcd.saddr
        << " soff=" << tcd.soff
        << " attr=" << tcd.attr
        << " nbytes=" << tcd.nbytes
        << " slast=" << tcd.slast
        << " daddr=" << tcd.daddr
        << " doff=" << tcd.doff
        << " citer=" << tcd.citer
        << " dlastsga=" << tcd.dlastsga
        << " csr=" << tcd.csr
        << " biter=" << tcd.biter;
}

void appendDmaChannel(fl::sstream& out,
                      const DmaChannelSnapshot& ch) FL_NO_EXCEPT {
    out << ' ' << ch.name << ".hasChannel=" << (ch.hasHardwareChannel ? 1 : 0)
        << ' ' << ch.name << ".channel=" << static_cast<int>(ch.channel)
        << ' ' << ch.name << ".dmamux=" << ch.dmamuxChcfg;
    appendTcd(out, ch.name, ch.tcd);
}

void appendTimer(fl::sstream& out, const char* name,
                 const TimerChannelSnapshot& timer) FL_NO_EXCEPT {
    out << ' ' << name << ".ctrl=" << timer.ctrl
        << ' ' << name << ".sctrl=" << timer.sctrl
        << ' ' << name << ".csctrl=" << timer.csctrl
        << ' ' << name << ".cntr=" << timer.cntr
        << ' ' << name << ".load=" << timer.load
        << ' ' << name << ".comp1=" << timer.comp1
        << ' ' << name << ".comp2=" << timer.comp2
        << ' ' << name << ".cmpld1=" << timer.cmpld1
        << ' ' << name << ".cmpld2=" << timer.cmpld2;
}

void appendPin(fl::sstream& out, const PinRegisterSnapshot& pin) FL_NO_EXCEPT {
    out << " pin=" << static_cast<int>(pin.pin)
        << " bit=" << static_cast<int>(pin.bit)
        << " mask=" << pin.mask
        << " fastGpio=" << static_cast<int>(pin.fastGpio)
        << " standardGpio=" << static_cast<int>(pin.standardGpio)
        << " mappedToStandard=" << (pin.mappedToStandard ? 1 : 0)
        << " gpr=" << pin.gpr
        << " gprMask=" << pin.gprMask
        << " muxReg=" << pin.muxRegister
        << " mux=" << pin.mux
        << " padReg=" << pin.padRegister
        << " pad=" << pin.pad
        << " outReg=" << pin.outputRegister
        << " out=" << pin.outputValue
        << " modeReg=" << pin.modeRegister
        << " mode=" << pin.modeValue
        << " inReg=" << pin.inputRegister
        << " in=" << pin.inputValue;
}

void appendEvent(fl::sstream& out, const SnapshotEvent& event) FL_NO_EXCEPT {
    out << "stage=" << event.stage
        << " ms=" << event.millisValue
        << " us=" << event.microsValue
        << " busy.dmaActive=" << (event.busy.dmaActive ? 1 : 0)
        << " busy.latchActive=" << (event.busy.latchActive ? 1 : 0)
        << " busy.ready=" << (event.busy.ready ? 1 : 0)
        << " gpr26=" << event.gpr[0]
        << " gpr27=" << event.gpr[1]
        << " gpr28=" << event.gpr[2]
        << " gpr29=" << event.gpr[3]
        << " tmr4.enbl=" << event.tmrEnbl
        << " xbar.sel0=" << event.xbarSel0
        << " xbar.sel1=" << event.xbarSel1
        << " xbar.sel15=" << event.xbarSel15
        << " xbar.sel47=" << event.xbarSel47
        << " xbar.ctrl0=" << event.xbarCtrl0
        << " xbar.ctrl1=" << event.xbarCtrl1;
    appendTimer(out, "tmr4.ch0", event.tmr[0]);
    appendTimer(out, "tmr4.ch1", event.tmr[1]);
    appendTimer(out, "tmr4.ch2", event.tmr[2]);
    out << " dma.es=" << event.dma.es
        << " dma.erq=" << event.dma.erq
        << " dma.err=" << event.dma.err
        << " dma.int=" << event.dma.irq
        << " dma.hrs=" << event.dma.hrs
        << " dma.allocatedMask=" << event.dma.allocatedMask
        << " dma.currentOwner=" << event.dma.currentOwner
        << " dma.frameBuffer=" << event.dma.frameBuffer
        << " dma.bitdata=" << event.dma.bitdata
        << " dma.bitmask=" << event.dma.bitmask
        << " dma.framebufferIndex=" << event.dma.framebufferIndex
        << " dma.numbytes=" << event.dma.numbytes
        << " dma.numpins=" << event.dma.numpins
        << " dma.bitmaskWords=[" << event.dma.bitmaskWords[0] << ','
        << event.dma.bitmaskWords[1] << ',' << event.dma.bitmaskWords[2]
        << ',' << event.dma.bitmaskWords[3] << ']';
    appendDmaChannel(out, event.dma.dma1);
    appendDmaChannel(out, event.dma.dma2);
    appendDmaChannel(out, event.dma.dma3);
    appendDmaChannel(out, event.dma.dma2next);
    for (u8 i = 0; i < event.pinCount; ++i) {
        out << " pin[" << static_cast<int>(i) << "].";
        appendPin(out, event.pins[i]);
    }
    if (event.rxPinValid) {
        const u32 mask = 1u << event.rxBit;
        out << " rx.pin=" << static_cast<int>(event.rxPin)
            << " rx.bit=" << static_cast<int>(event.rxBit)
            << " rx.fastPsr=" << event.rxFastPsr
            << " rx.standardPsr=" << event.rxStandardPsr
            << " rx.fastHigh=" << ((event.rxFastPsr & mask) ? 1 : 0)
            << " rx.standardHigh=" << ((event.rxStandardPsr & mask) ? 1 : 0);
    }
    out << '\n';
}

bool stageEquals(const char* left, const char* right) FL_NO_EXCEPT {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    return fl::strcmp(left, right) == 0;
}

bool stageAlreadyCaptured(const char* stage) FL_NO_EXCEPT {
    for (u8 i = 0; i < s_eventCount; ++i) {
        if (stageEquals(s_events[i].stage, stage)) {
            return true;
        }
    }
    return false;
}

} // namespace

void objectFledDiagnosticsReset() FL_NO_EXCEPT {
    fl::memset(s_events, 0, sizeof(s_events));
    fl::memset(s_lastPins, 0, sizeof(s_lastPins));
    s_eventCount = 0;
    s_overflowCount = 0;
    s_lastPinCount = 0;
    s_busyState = BusyStateSnapshot();
    s_rxPinValid = false;
    s_rxPin = 0;
}

void objectFledDiagnosticsSetBusyState(bool dma_active,
                                       bool latch_active) FL_NO_EXCEPT {
    s_busyState.dmaActive = dma_active;
    s_busyState.latchActive = latch_active;
    s_busyState.ready = !dma_active && !latch_active;
}

void objectFledDiagnosticsSetRxPin(int rx_pin) FL_NO_EXCEPT {
#if defined(NUM_DIGITAL_PINS)
    if (rx_pin < 0 || rx_pin >= NUM_DIGITAL_PINS) {
        s_rxPinValid = false;
        return;
    }
    s_rxPinValid = true;
    s_rxPin = static_cast<u8>(rx_pin);
#else
    (void)rx_pin;
    s_rxPinValid = false;
#endif
}

void objectFledDiagnosticsRecord(const char* stage, const u8* pins,
                                 u32 pin_count) FL_NO_EXCEPT {
    if (stageAlreadyCaptured(stage)) {
        return;
    }

    if (pins != nullptr && pin_count > 0) {
        s_lastPinCount = static_cast<u8>(pin_count < kMaxPins ? pin_count : kMaxPins);
        for (u8 i = 0; i < s_lastPinCount; ++i) {
            s_lastPins[i] = pins[i];
        }
    } else {
        pins = s_lastPins;
        pin_count = s_lastPinCount;
    }

    if (s_eventCount >= kMaxEvents) {
        ++s_overflowCount;
        return;
    }

    SnapshotEvent& event = s_events[s_eventCount++];
    event.stage = stage != nullptr ? stage : "";
    event.millisValue = millis();
    event.microsValue = micros();
    event.busy = s_busyState;
    event.pinCount = static_cast<u8>(pin_count < kMaxPins ? pin_count : kMaxPins);
    for (u8 i = 0; i < event.pinCount; ++i) {
        event.pins[i] = capturePin(pins[i]);
    }
    event.gpr[0] = IOMUXC_GPR_GPR26;
    event.gpr[1] = IOMUXC_GPR_GPR27;
    event.gpr[2] = IOMUXC_GPR_GPR28;
    event.gpr[3] = IOMUXC_GPR_GPR29;
    event.tmrEnbl = TMR4_ENBL;
    event.tmr[0] = captureTimerChannel(0);
    event.tmr[1] = captureTimerChannel(1);
    event.tmr[2] = captureTimerChannel(2);
    event.xbarSel15 = XBARA1_SEL15;
    event.xbarSel47 = XBARA1_SEL47;
    event.xbarSel0 = XBARA1_SEL0;
    event.xbarSel1 = XBARA1_SEL1;
    event.xbarCtrl0 = XBARA1_CTRL0;
    event.xbarCtrl1 = XBARA1_CTRL1;
    event.dma = captureDma();

#if defined(NUM_DIGITAL_PINS)
    if (s_rxPinValid && s_rxPin < NUM_DIGITAL_PINS) {
        event.rxPinValid = true;
        event.rxPin = s_rxPin;
        event.rxBit = digitalPinToBit(s_rxPin);
        volatile u32* fastOut = portOutputRegister(s_rxPin);
        volatile u32* fastPsr =
            reinterpret_cast<volatile u32*>( // ok reinterpret cast - Teensy GPIO PSR offset
                ptrToU32(fastOut) + 0x08u);
        volatile u32* standardPsr =
            reinterpret_cast<volatile u32*>( // ok reinterpret cast - Teensy GPIO alias address map
                ptrToU32(fastOut) - 0x01E48000u + 0x08u);
        event.rxFastPsr = *fastPsr;
        event.rxStandardPsr = *standardPsr;
    }
#endif
}

fl::json objectFledDiagnosticsToJson() FL_NO_EXCEPT {
    fl::json out = fl::json::object();
    out.set("enabled", true);
    setU32(out, "eventCount", s_eventCount);
    setU32(out, "overflowCount", s_overflowCount);
    out.set("format", "objectfled-register-snapshot-v2");
    out.set("note",
            "snapshot is newline-delimited key=value register state");
    fl::sstream snapshot;
    for (u8 i = 0; i < s_eventCount; ++i) {
        appendEvent(snapshot, s_events[i]);
    }
    fl::string text = snapshot.str();
    out.set("snapshot", text.c_str());
    return out;
}

} // namespace fl

#endif // FASTLED_OBJECTFLED_DIAGNOSTICS

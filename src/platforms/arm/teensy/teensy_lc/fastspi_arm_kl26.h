// IWYU pragma: private

#ifndef __INC_FASTSPI_ARM_KL26_H
#define __INC_FASTSPI_ARM_KL26_H

#include "fastspi_types.h"
#include "fastled_delay.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {
template <int VAL> void getScalars(u8 & sppr, u8 & spr) FL_NO_EXCEPT {
  if(VAL > 4096) { sppr=7; spr=8; }
  else if(VAL > 3584) { sppr=6; spr=8; }
  else if(VAL > 3072) { sppr=5; spr=8; }
  else if(VAL > 2560) { sppr=4; spr=8; }
  else if(VAL > 2048) { sppr=7; spr=7; }
  else if(VAL > 2048) { sppr=3; spr=8; }
  else if(VAL > 1792) { sppr=6; spr=7; }
  else if(VAL > 1536) { sppr=5; spr=7; }
  else if(VAL > 1536) { sppr=2; spr=8; }
  else if(VAL > 1280) { sppr=4; spr=7; }
  else if(VAL > 1024) { sppr=7; spr=6; }
  else if(VAL > 1024) { sppr=3; spr=7; }
  else if(VAL > 1024) { sppr=1; spr=8; }
  else if(VAL > 896) { sppr=6; spr=6; }
  else if(VAL > 768) { sppr=5; spr=6; }
  else if(VAL > 768) { sppr=2; spr=7; }
  else if(VAL > 640) { sppr=4; spr=6; }
  else if(VAL > 512) { sppr=7; spr=5; }
  else if(VAL > 512) { sppr=3; spr=6; }
  else if(VAL > 512) { sppr=1; spr=7; }
  else if(VAL > 512) { sppr=0; spr=8; }
  else if(VAL > 448) { sppr=6; spr=5; }
  else if(VAL > 384) { sppr=5; spr=5; }
  else if(VAL > 384) { sppr=2; spr=6; }
  else if(VAL > 320) { sppr=4; spr=5; }
  else if(VAL > 256) { sppr=7; spr=4; }
  else if(VAL > 256) { sppr=3; spr=5; }
  else if(VAL > 256) { sppr=1; spr=6; }
  else if(VAL > 256) { sppr=0; spr=7; }
  else if(VAL > 224) { sppr=6; spr=4; }
  else if(VAL > 192) { sppr=5; spr=4; }
  else if(VAL > 192) { sppr=2; spr=5; }
  else if(VAL > 160) { sppr=4; spr=4; }
  else if(VAL > 128) { sppr=7; spr=3; }
  else if(VAL > 128) { sppr=3; spr=4; }
  else if(VAL > 128) { sppr=1; spr=5; }
  else if(VAL > 128) { sppr=0; spr=6; }
  else if(VAL > 112) { sppr=6; spr=3; }
  else if(VAL > 96) { sppr=5; spr=3; }
  else if(VAL > 96) { sppr=2; spr=4; }
  else if(VAL > 80) { sppr=4; spr=3; }
  else if(VAL > 64) { sppr=7; spr=2; }
  else if(VAL > 64) { sppr=3; spr=3; }
  else if(VAL > 64) { sppr=1; spr=4; }
  else if(VAL > 64) { sppr=0; spr=5; }
  else if(VAL > 56) { sppr=6; spr=2; }
  else if(VAL > 48) { sppr=5; spr=2; }
  else if(VAL > 48) { sppr=2; spr=3; }
  else if(VAL > 40) { sppr=4; spr=2; }
  else if(VAL > 32) { sppr=7; spr=1; }
  else if(VAL > 32) { sppr=3; spr=2; }
  else if(VAL > 32) { sppr=1; spr=3; }
  else if(VAL > 32) { sppr=0; spr=4; }
  else if(VAL > 28) { sppr=6; spr=1; }
  else if(VAL > 24) { sppr=5; spr=1; }
  else if(VAL > 24) { sppr=2; spr=2; }
  else if(VAL > 20) { sppr=4; spr=1; }
  else if(VAL > 16) { sppr=7; spr=0; }
  else if(VAL > 16) { sppr=3; spr=1; }
  else if(VAL > 16) { sppr=1; spr=2; }
  else if(VAL > 16) { sppr=0; spr=3; }
  else if(VAL > 14) { sppr=6; spr=0; }
  else if(VAL > 12) { sppr=5; spr=0; }
  else if(VAL > 12) { sppr=2; spr=1; }
  else if(VAL > 10) { sppr=4; spr=0; }
  else if(VAL > 8) { sppr=3; spr=0; }
  else if(VAL > 8) { sppr=1; spr=1; }
  else if(VAL > 8) { sppr=0; spr=2; }
  else if(VAL > 6) { sppr=2; spr=0; }
  else if(VAL > 4) { sppr=1; spr=0; }
  else if(VAL > 4) { sppr=0; spr=1; }
  else /* if(VAL > 2) */ { sppr=0; spr=0; }
}


#define SPIX (*(KINETISL_SPI_t*)pSPIX)
#define ARM_HARDWARE_SPI

template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER, u32 pSPIX>
class ARMHardwareSPIOutput {
  Selectable *mPSelect;

  static inline void enable_pins(void) FL_NO_EXCEPT __attribute__((always_inline)) {
    switch(_DATA_PIN) {
      case 0: CORE_PIN0_CONFIG =  PORT_PCR_MUX(2); break;
      case 1: CORE_PIN1_CONFIG =  PORT_PCR_MUX(5); break;
      case 7: CORE_PIN7_CONFIG =  PORT_PCR_MUX(2); break;
      case 8: CORE_PIN8_CONFIG =  PORT_PCR_MUX(5); break;
      case 11: CORE_PIN11_CONFIG =  PORT_PCR_MUX(2); break;
      case 12: CORE_PIN12_CONFIG =  PORT_PCR_MUX(5); break;
      case 21: CORE_PIN21_CONFIG =  PORT_PCR_MUX(2); break;
    }

    switch(_CLOCK_PIN) {
      case 13: CORE_PIN13_CONFIG =  PORT_PCR_MUX(2); break;
      case 14: CORE_PIN14_CONFIG =  PORT_PCR_MUX(2); break;
      case 20: CORE_PIN20_CONFIG =  PORT_PCR_MUX(2); break;
    }
  }

  static inline void disable_pins(void) FL_NO_EXCEPT __attribute((always_inline)) {
    switch(_DATA_PIN) {
      case 0: CORE_PIN0_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 1: CORE_PIN1_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 7: CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 8: CORE_PIN8_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 11: CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 12: CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 21: CORE_PIN21_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
    }

    switch(_CLOCK_PIN) {
      case 13: CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 14: CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 20: CORE_PIN20_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
    }
  }

  static inline auto dataPinConfig() FL_NO_EXCEPT -> decltype(&CORE_PIN11_CONFIG) {
    switch(_DATA_PIN) {
      case 0: return &CORE_PIN0_CONFIG;
      case 1: return &CORE_PIN1_CONFIG;
      case 7: return &CORE_PIN7_CONFIG;
      case 8: return &CORE_PIN8_CONFIG;
      case 11: return &CORE_PIN11_CONFIG;
      case 12: return &CORE_PIN12_CONFIG;
      case 21: return &CORE_PIN21_CONFIG;
    }
    return nullptr;
  }

  static inline auto clockPinConfig() FL_NO_EXCEPT -> decltype(&CORE_PIN13_CONFIG) {
    switch(_CLOCK_PIN) {
      case 13: return &CORE_PIN13_CONFIG;
      case 14: return &CORE_PIN14_CONFIG;
      case 20: return &CORE_PIN20_CONFIG;
    }
    return nullptr;
  }

  void setSPIRate() FL_NO_EXCEPT {
    u8 sppr, spr;
    getScalars<_SPI_CLOCK_DIVIDER>(sppr, spr);

    // Set the speed
    SPIX.BR = SPI_BR_SPPR(sppr) | SPI_BR_SPR(spr);

    // Also, force 8 bit transfers (don't want to juggle 8/16 since that flushes the world)
    SPIX.C2 = 0;
    // Also force mode 0, MSB first: another SPI client may have left CPOL set, and then
    // handing the clock pad back after writeBit would be an extra rising edge.
    SPIX.C1 = SPI_C1_MSTR | SPI_C1_SPE;
  }

public:
  ARMHardwareSPIOutput() { mPSelect = nullptr; }
  ARMHardwareSPIOutput(Selectable *pSelect) { mPSelect = pSelect; }

  // set the object representing the selectable
  void setSelect(Selectable *pSelect) { mPSelect = pSelect; }

  // initialize the SPI subssytem
  void init() FL_NO_EXCEPT {
    FastPin<_DATA_PIN>::setOutput();
    FastPin<_CLOCK_PIN>::setOutput();

    // Enable the SPI clocks
    u32 sim4 = SIM_SCGC4;
    if ((pSPIX == 0x40076000) && !(sim4 & SIM_SCGC4_SPI0)) {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI0;
    }

    if ( (pSPIX == 0x40077000) && !(sim4 & SIM_SCGC4_SPI1)) {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI1;
    }

    SPIX.C1 = SPI_C1_MSTR | SPI_C1_SPE;
    SPIX.C2 = 0;
    SPIX.BR = SPI_BR_SPPR(1) | SPI_BR_SPR(0);
  }

  // latch the CS select
  void inline select() FL_NO_EXCEPT __attribute__((always_inline)) {
    if(mPSelect != nullptr) { mPSelect->select(); }
    setSPIRate();
    enable_pins();
  }


  // release the CS select
  void inline release() FL_NO_EXCEPT __attribute__((always_inline)) {
    waitFully();  // Taking the pads away mid-frame would cut off the last byte.
    disable_pins();
    if(mPSelect != nullptr) { mPSelect->release(); }
  }

  void endTransaction() FL_NO_EXCEPT {
    release();
  }

  // Wait for the world to be clear
  static void wait() __attribute__((always_inline)) { while(!(SPIX.S & SPI_S_SPTEF));  }

  // Wait until all queued data has left the shifter. SPTEF only says the buffer is
  // empty, and there is no shifter-idle flag: SPRF cannot stand in, because nothing
  // here reads DL. So wait out one frame, plus one bit of margin for the load from the
  // buffer. Each peripheral read takes at least one bus clock, so this cannot be short.
  static void waitFully() FL_NO_EXCEPT {
    wait();
    const u8 rate = SPIX.BR;
    const u32 busClocksPerBit = (static_cast<u32>((rate >> 4) & 0x7) + 1) << ((rate & 0xF) + 1);
    for (u32 reads = 9 * busClocksPerBit; reads != 0; --reads) {
      const u8 status = SPIX.S;
      (void)status;
    }
  }

  // The peripheral only sends 8 or 16 bit frames, so one bit is sent by hand: drain,
  // give both pads to GPIO, pulse, and give them back as they were. The clock latch
  // goes low first so that it cannot rise as the pad leaves the mux, matching SPI
  // mode 0's idle level. Slow, but only sm16716-style start bits and trailers use it.
  template <u8 BIT> inline static void writeBit(u8 b) FL_NO_EXCEPT {
    waitFully();
    FastPin<_CLOCK_PIN>::lo();
    auto* const dataConfig = dataPinConfig();
    auto* const clockConfig = clockPinConfig();
    const u32 savedData = dataConfig ? static_cast<u32>(*dataConfig) : 0;
    const u32 savedClock = clockConfig ? static_cast<u32>(*clockConfig) : 0;
    // GPIO without PORT_PCR_SRE: this pulse is short, and the bytes around it are fast.
    if (dataConfig) { *dataConfig = PORT_PCR_MUX(1); }
    if (clockConfig) { *clockConfig = PORT_PCR_MUX(1); }

    if (b & (1 << BIT)) {
      FastPin<_DATA_PIN>::hi();
    } else {
      FastPin<_DATA_PIN>::lo();
    }
    delaycycles<NS(35)>();
    FastPin<_CLOCK_PIN>::hi();
    delaycycles<NS(35)>();
    FastPin<_CLOCK_PIN>::lo();

    // Without ISF, which is write-1-to-clear: restoring it would drop a pending pin interrupt.
    if (dataConfig) { *dataConfig = savedData & ~PORT_PCR_ISF; }
    if (clockConfig) { *clockConfig = savedClock & ~PORT_PCR_ISF; }
  }

  // write a byte out via SPI (returns immediately on writing register)
  static void writeByte(u8 b) __attribute__((always_inline)) { wait(); SPIX.DL = b; }
  // write a word out via SPI (returns immediately on writing register)
  static void writeWord(u16 w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w & 0xFF); }

  // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
  static void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
    while(len--) { writeByte(value); }
  }

  // A full cycle of writing a value for len bytes, including select, release, and waiting
  void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
    setSPIRate();
    select();
    while(len--) {
      writeByte(value);
    }
    release();
  }

  // A full cycle of writing a raw block of data out, including select, release, and waiting
  template <class D> void writeBytes(FASTLED_REGISTER u8 *data, int len) FL_NO_EXCEPT {
    setSPIRate();
    u8 *end = data + len;
    select();
    // could be optimized to write 16bit words out instead of 8bit bytes
    while(data != end) {
      writeByte(D::adjust(*data++));
    }
    D::postBlock(len);
    release();
  }

  void writeBytes(FASTLED_REGISTER u8 *data, int len) { writeBytes<DATA_NOP>(data, len); }


  template <u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NO_EXCEPT {
    int len = pixels.mLen;

    select();
    while(pixels.has(1)) {
      if(FLAGS & FLAG_START_BIT) {
        writeBit<0>(1);
        writeByte(D::adjust(pixels.loadAndScale0()));
        writeByte(D::adjust(pixels.loadAndScale1()));
        writeByte(D::adjust(pixels.loadAndScale2()));
      } else {
        writeByte(D::adjust(pixels.loadAndScale0()));
        writeByte(D::adjust(pixels.loadAndScale1()));
        writeByte(D::adjust(pixels.loadAndScale2()));
      }

      pixels.advanceData();
      pixels.stepDithering();
    }
    D::postBlock(len);
    release();
  }

  /// Finalize transmission (no-op for Teensy LC SPI)
  /// This method exists for compatibility with other SPI implementations
  /// that may need to flush buffers or perform post-transmission operations
  static void finalizeTransmission() { }

};
}  // namespace fl

FL_DISABLE_WARNING_POP

#endif

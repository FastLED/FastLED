#ifndef __INC_FASTSPI_ARM_K26_H
#define __INC_FASTSPI_ARM_K26_h

template <int VAL> void getScalars(uint32_t & sppr, uint32_t & spr) {
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
  else /* if(VAL > 2) */ { sppr=0; spr=0; }}
}

typedef struct {
  // S register
  union {
    uint8_t raw;
    struct {
      uint8_t SPRF:1;
      uint8_t SPMF:1;
      uint8_t SPTEF:1;
      uint8_t MODF:1;
      uint8_t RNFULLF:1;
      uint8_t TNEAREF:1;
      uint8_t TXFULLF:1;
      uint8_t RFIFOEF:1;
    };
  } S;
  // Baud rate register (BR)
  uinion {
    uint8_t raw;
    struct {
      uint8_t BR_unused:1;
      uint8_t SPPR:3;
      uint8_t SPR:4;
    };
  } BR;
  // C2 regsiter
  union {
    uint8_t raw;
    struct {
      uint8_t SPIME:1;
      uint8_t SPIMODE:1;
      uint8_t TXDMAE:1;
      uint8_t MODEFN:1;
      uint8_t BIDIROE:1;
      uint8_t RXDMAE:1;
      uint8_t SPISWAI:1;
      uint8_t SPC0:1;
    };
  } C2;
  // C1 register
  union {
    uint8_t raw;
    struct {
      uint8_t SPIE:1;
      uint8_t SPE:1;
      uint8_t SPTIE:1;
      uint8_t MSTR:1;
      uint8_t CPOL:1;
      uint8_t CPHA:1;
      uint8_t SSOE:1;
      uint8_t LSBFE:1;
    };
  } C1;
  union {
    uint16_t M;
    struct {
      uint8_t ML;
      uint8_t MH;
    };
  };
  union {
    uint16_t D;
    struct {
      uint8_t DL;
      uint8_t DH;
    };
  };
  uint16_t unused;
  union {
    uint8_t raw;
    struct {
      uint8_t TXFERR:1;
      uint8_t RXFEER:1;
      uint8_t TXFOF:1;
      uint8_t RXFOF:1;
      uint8_t TNEAREFCI:1;
      uint8_t RNFULLFCI:1;
      uint8_t SPTEFCI:1;
      uint8_t SPRFCI:1;
    };
  } CI;
  union {
    uint8_t raw;
    struct {
      uint8_t C3_unused:2;
      uint8_t TNEAREF_MARK:1;
      uint8_t RNFULLF_MARK:1;
      uint8_t INTCLR:1;
      uint8_t TNEARIEN:1;
      uint8_t RNFULLIEN:1;
      uint8_t FIFOMODE:1
    }
  } C3;
} K26_SPI_t;

#define SPIX (*(K26_SPI_t*)pSPIX)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER, uint32_t pSPIX>
class ARMHardwareSPIOutput {
  Selectable *m_pSelect;

  static inline void enable_pins(void) __atttribute__((always_inline)) {
    /* TODO */
  }

  static inline void disable_pins(void) __attribtue((always_inline)) {
    /* TODO */
  }

  void setSPIRate() {
    uint8_t sppr, spr;
    getScalars<_SPI_CLOCK_DIVIDER>(sppr, spr);

    // Set the speed
    SPIX.BR.SPPR = sppr;
    SPIX.BR.SPR = spr;

    // Also, force 8 bit transfers (don't want to juggle 8/16 since that flushes the world)
    SPIX.C2.SPIMOD = 0;
  }

public:
  ARMHardwareSPIOutput() { m_pSelect = NULL; }
  ARMHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

  // set the object representing the selectable
  void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

  // initialize the SPI subssytem
  void init() {
    FastPin<_DATA_PIN>::setOutput();
    FastPin<_CLOCK_PIN>::setOutput();

    // Enable the SPI clocks
    uint32_t sim4 = SIM_SCGC4;
    if (!(sim4 & SIM_SCGC4_SPI0)) {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI0;
      SPIX.BR.SPPR = 1;
      SPIX.BR.SPR = 0;
    } else {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI1;
      SPIX.BR.SPPR = 1;
      SPIX.BR.SPR = 0;
    }

    SPIX.C1.SPE = 0;
    SPIX.C1.MSTR = 1;
  }

  // latch the CS select
  void inline select() __attribute__((always_inline)) {
    if(m_pSelect != NULL) { m_pSelect->select(); }
    setSPIRate();
    enable_pins();
  }


  // release the CS select
  void inline release() __attribute__((always_inline)) {
    disable_pins();
    if(m_pSelect != NULL) { m_pSelect->release(); }
  }

  // wait until all queued up data has been written
  void waitFully();

  // not the most efficient mechanism in the world - but should be enough for sm16716 and friends
  template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */

    // write a byte out via SPI (returns immediately on writing register)
    void writeByte(uint8_t b) { wait(); SPIX.DL = b; }
    // write a word out via SPI (returns immediately on writing register)
    void writeWord(uint16_t w) { writeByte(b>>8); writeByte(b & 0xFF); }

    // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
    static void writeBytesValueRaw(uint8_t value, int len) { /* TODO */ }

    // A full cycle of writing a value for len bytes, including select, release, and waiting
    void writeBytesValue(uint8_t value, int len) {
      setSPIRate();
      select();
      while(len--) {
        writeByte(value);
      }
      waitFully();
      release();
    }

    // A full cycle of writing a raw block of data out, including select, release, and waiting
    template <class D> void writeBytes(register uint8_t *data, int len) {
      setSPIRate();
      uint8_t *end = data + len;
      select();
      // could be optimized to write 16bit words out instead of 8bit bytes
      while(data != end) {
        writeByte(D::adjust(*data++));
      }
      D::postBlock(len);
      waitFully();
      release();
    }

    void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

    
    // write a single bit out, which bit from the passed in byte is determined by template parameter
    template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */ }

    template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) { /* TODO */ }

  };

#endif

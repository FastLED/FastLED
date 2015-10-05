#ifndef __INC_FASTSPI_NRF_H
#define __INC_FASTSPI_NRF_H

#ifdef NRF51

#ifndef FASTLED_FORCE_SOFTWARE_SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

// A nop/stub class, mostly to show the SPI methods that are needed/used by the various SPI chipset implementations.  Should
// be used as a definition for the set of methods that the spi implementation classes should use (since C++ doesn't support the
// idea of interfaces - it's possible this could be done with virtual classes, need to decide if i want that overhead)
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class NRF51SPIOutput {

  struct saveData {
    uint32_t sck;
    uint32_t mosi;
    uint32_t miso;
    uint32_t freq;
    uint32_t enable;
  } mSavedData;

  void saveSPIData() {
    mSavedData.sck = NRF_SPI0->PSELSCK;
    mSavedData.mosi = NRF_SPI0->PSELMOSI;
    mSavedData.miso = NRF_SPI0->PSELMISO;
    mSavedData.freq = NRF_SPI0->FREQUENCY;
    mSavedData.enable = NRF_SPI0->ENABLE;
  }

  void restoreSPIData() {
    NRF_SPI0->PSELSCK = mSavedData.sck;
    NRF_SPI0->PSELMOSI = mSavedData.mosi;
    NRF_SPI0->PSELMISO = mSavedData.miso;
    NRF_SPI0->FREQUENCY = mSavedData.freq;
    mSavedData.enable = NRF_SPI0->ENABLE;
  }

public:
  NRF51SPIOutput() { FastPin<_DATA_PIN>::setOutput(); FastPin<_CLOCK_PIN>::setOutput(); }
  NRF51SPIOutput(Selectable *pSelect) {  FastPin<_DATA_PIN>::setOutput(); FastPin<_CLOCK_PIN>::setOutput();  }

  // set the object representing the selectable
  void setSelect(Selectable *pSelect) { /* TODO */ }

  // initialize the SPI subssytem
  void init() {
    FastPin<_DATA_PIN>::setOutput();
    FastPin<_CLOCK_PIN>::setOutput();
    NRF_SPI0->PSELSCK = _CLOCK_PIN;
    NRF_SPI0->PSELMOSI = _DATA_PIN;
    NRF_SPI0->PSELMISO = 0xFFFFFFFF;
    NRF_SPI0->FREQUENCY = 0x80000000;
    NRF_SPI0->ENABLE = 1;
    NRF_SPI0->EVENTS_READY = 0;
  }

  // latch the CS select
  void select() { saveSPIData(); init(); }

  // release the CS select
  void release() { restoreSPIData(); }

  static bool shouldWait(bool wait = false) __attribute__((always_inline)) __attribute__((always_inline)) {
    static bool sWait=false;
    return false; // if(sWait) { sWait = wait; return true; } else { sWait = wait; return false; }
  }

  // wait until all queued up data has been written
  static void waitFully() __attribute__((always_inline)){ if(shouldWait()) { while(NRF_SPI0->EVENTS_READY==0); } NRF_SPI0->EVENTS_READY=0; uint8_t b = NRF_SPI0->RXD; }
  void wait() __attribute__((always_inline)){ if(shouldWait()) { while(NRF_SPI0->EVENTS_READY==0); } NRF_SPI0->EVENTS_READY=0; uint8_t b = NRF_SPI0->RXD; }
  // void waitFully() { while(NRF_SPI0->EVENTS_READY==0); NRF_SPI0->EVENTS_READY=0; uint8_t b = NRF_SPI0->RXD; }
  // void wait() { while(NRF_SPI0->EVENTS_READY==0); NRF_SPI0->EVENTS_READY=0; uint8_t b = NRF_SPI0->RXD; }

  // write a byte out via SPI (returns immediately on writing register)
  // void writeByte(uint8_t b) { wait(); NRF_SPI0->TXD = b;  shouldWait(true); }
  // void writeByte(uint8_t b) __attribute__((always_inline)){ wait(); NRF_SPI0->TXD = b;  shouldWait(true);  }
  static void writeByte(uint32_t b) __attribute__((always_inline)) {  /*NRF_SPI0->EVENTS_READY=0; */ /*uint8_t x = NRF_SPI0->RXD;*/ NRF_SPI0->TXD = b; }

  // write a word out via SPI (returns immediately on writing register)
  static void writeWord(uint32_t w) __attribute__((always_inline)){ writeByte(w>>8); writeByte(w & 0xFF);  }

  // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
  static void writeBytesValueRaw(uint8_t value, int len) { while(len--) { writeByte(value);  } }

  // A full cycle of writing a value for len bytes, including select, release, and waiting
  void writeBytesValue(uint8_t value, int len) {
    select();
    while(len--) {
      writeByte(value);
    }
    waitFully();
    release();
  }

  // A full cycle of writing a raw block of data out, including select, release, and waiting
  template<class D> void writeBytes(uint8_t *data, int len) {
    uint8_t *end = data + len;
    select();
    while(data != end) {
      writeByte(D::adjust(*data++));
    }
    D::postBlock(len);
    waitFully();
    release();
  }

  void writeBytes(uint8_t *data, int len) {
    writeBytes<DATA_NOP>(data, len);
  }

  // write a single bit out, which bit from the passed in byte is determined by template parameter
  template <uint8_t BIT> inline static void writeBit(uint8_t b) {
    waitFully();
    NRF_SPI0->ENABLE = 0;
    if(b & 1<<BIT) {
      FastPin<_DATA_PIN>::hi();
    } else {
      FastPin<_DATA_PIN>::lo();
    }
    FastPin<_CLOCK_PIN>::toggle();
    FastPin<_CLOCK_PIN>::toggle();
    NRF_SPI0->ENABLE = 1;
  }

  template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
    select();
    int len = pixels.mLen;
    while(pixels.has(1)) {
      if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
      }
			writeByte(D::adjust(pixels.loadAndScale0()));
			writeByte(D::adjust(pixels.loadAndScale1()));
			writeByte(D::adjust(pixels.loadAndScale2()));

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		waitFully();
		release();
  }

};

#endif
#endif

#endif

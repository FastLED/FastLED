#ifndef __FAST_SPI_NULL_H__
#define __FAST_SPI_NULL_H__

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class NULLSPIOutput {
	Selectable *m_pSelect;

	static inline void waitForEmpty() { }

	void enableConfig() { }
	void disableConfig() { }

	void enableSPI() {  }
	void disableSPI() {  }
	void resetSPI() {  }

	static inline void readyTransferBits(register uint32_t bits) {
	}

	template<int BITS> static inline void writeBits(uint16_t w) {
	}

public:
	NULLSPIOutput() {  }
	NULLSPIOutput(Selectable *pSelect) { }

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() {
	
	}

	// latch the CS select
	void inline select() __attribute__((always_inline)) { }

	// release the CS select
	void inline release() __attribute__((always_inline)) {  }

	// wait until all queued up data has been written
	void waitFully() {}

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(uint8_t b) {
	}

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(uint16_t w) {
		
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) {
			}

	template <class D> void writeBytes(register uint8_t *data, int len) {
	}

	void writeBytes(register uint8_t *data, int len) { }


	template <uint8_t BIT> inline void writeBit(uint8_t b) {
	}


	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
	}
};


#endif // __FAST_SPI_NULL_H__

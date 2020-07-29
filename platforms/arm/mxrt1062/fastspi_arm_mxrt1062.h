#ifndef __INC_FASTSPI_ARM_MXRT1062_H
#define __INC_FASTSPI_ARM_MXRT1062_H

FASTLED_NAMESPACE_BEGIN

#if defined (FASTLED_TEENSY4) && defined(ARM_HARDWARE_SPI)
#include <SPI.h>

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint32_t _SPI_CLOCK_RATE, SPIClass & _SPIObject, int _SPI_INDEX>
class Teesy4HardwareSPIOutput {
	Selectable *m_pSelect;
	uint32_t  m_bitCount;
	uint32_t m_bitData;
	inline IMXRT_LPSPI_t & port() __attribute__((always_inline)) {
		switch(_SPI_INDEX) {
			case 0:
			return IMXRT_LPSPI4_S;
			case 1:
			return IMXRT_LPSPI3_S;
			case 2:
			return IMXRT_LPSPI1_S;
		}
	}

public:
	Teesy4HardwareSPIOutput() { m_pSelect = NULL; m_bitCount = 0;}
	Teesy4HardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; m_bitCount = 0;}

	// set the object representing the selectable -- ignore for now
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() { _SPIObject.begin(); }

	// latch the CS select
	void inline select() __attribute__((always_inline)) {
		// begin the SPI transaction
		_SPIObject.beginTransaction(SPISettings(_SPI_CLOCK_RATE, MSBFIRST, SPI_MODE0));
		if(m_pSelect != NULL) { m_pSelect->select(); }
	}

	// release the CS select
	void inline release() __attribute__((always_inline)) {
		if(m_pSelect != NULL) { m_pSelect->release(); }
		_SPIObject.endTransaction();
	}

	// wait until all queued up data has been written
	static void waitFully() { /* TODO */ }

	// write a byte out via SPI (returns immediately on writing register) -
	void inline writeByte(uint8_t b) __attribute__((always_inline)) {
		if(m_bitCount == 0) {
			_SPIObject.transfer(b);
		} else {
			// There's been a bit of data written, add that to the output as well
			uint32_t outData = (m_bitData << 8) | b;
			uint32_t tcr = port().TCR;
			port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ((8+m_bitCount) - 1);  // turn on 9 bit mode
			port().TDR = outData;		// output 9 bit data.
			while ((port().RSR & LPSPI_RSR_RXEMPTY)) ;	// wait while the RSR fifo is empty...
			port().TCR = (tcr & 0xfffff000) | LPSPI_TCR_FRAMESZ((8) - 1);  // turn back on 8 bit mode
			port().RDR;
			m_bitCount = 0;
		}
	}

	// write a word out via SPI (returns immediately on writing register)
	void inline writeWord(uint16_t w) __attribute__((always_inline)) {
		writeByte(((w>>8) & 0xFF));
		_SPIObject.transfer(w & 0xFF);
	}

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { _SPIObject.transfer(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) {
		select(); writeBytesValueRaw(value, len); release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	template <class D> void writeBytes(register uint8_t *data, int len) {
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

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline void writeBit(uint8_t b) {
		m_bitData = (m_bitData<<1) | ((b&(1<<BIT)) != 0);
		// If this is the 8th bit we've collected, just write it out raw
		register uint32_t bc = m_bitCount;
		bc = (bc + 1) & 0x07;
		if (!bc) {
			m_bitCount = 0;
			_SPIObject.transfer(m_bitData);
		}
		m_bitCount = bc;
	}

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
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
		release();
	}

};


#endif

FASTLED_NAMESPACE_END
#endif

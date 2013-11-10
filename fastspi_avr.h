#ifndef __INC_FASTSPI_AVR_H
#define __INC_FASTSPI_AVR_H

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using USART registers and friends
//
// TODO: Complete/test implementation - right now this doesn't work
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// uno/mini/duemilanove
#if defined(AVR_HARDWARE_SPI)
#if defined(UBRR0)
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRUSARTSPIOutput { 
	Selectable *m_pSelect;

public:
	AVRUSARTSPIOutput() { m_pSelect = NULL; }
	AVRUSARTSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() { 
		UBRR0 = 0;
		UCSR0A = 1<<TXC0;

		FastPin<_CLOCK_PIN>::setOutput();
		FastPin<_DATA_PIN>::setOutput();

		UCSR0C = _BV (UMSEL00) | _BV (UMSEL01);  // Master SPI mode
		UCSR0B = _BV (TXEN0) | _BV (RXEN0);  // transmit enable and receive enable

		// must be done last, see page 206
		UBRR0 = 3;  // 2 Mhz clock rate
	}

	static void stop() { 
		// TODO: stop the uart spi output
	}

	static void wait() __attribute__((always_inline)) { while(!(UCSR0A & (1<<UDRE0))); }
	static void waitFully() __attribute__((always_inline)) { wait(); }
	
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { UDR0 = b;}
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { UDR0 = b; wait(); }
	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); UDR0 = b; }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }
	
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		if(b && (1 << BIT)) { 
			FastPin<_DATA_PIN>::hi();
		} else { 
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
	}

	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } // FastPin<_SELECT_PIN>::hi(); }
	void release() { 
		// wait for all transmissions to finish
  		while ((UCSR0A & (1 <<TXC0)) == 0) {}
    	if(m_pSelect != NULL) { m_pSelect->release(); } // FastPin<_SELECT_PIN>::hi(); 
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}
	
	void writeBytesValue(uint8_t value, int len) { 
		select();
		while(len--) { 
			writeByte(value);
		}
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		select();
		while(data != end) { 
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
#else
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
#endif
		}
		D::postBlock(len);
		release();	
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			writeByte(D::adjust(data[SPI_B0], scale));
			writeByte(D::adjust(data[SPI_B1], scale));
			writeByte(D::adjust(data[SPI_B2], scale));
			data += SPI_ADVANCE;
		}
		D::postBlock(len);
		release();
	}

	template <uint8_t SKIP, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<SKIP, DATA_NOP, RGB_ORDER>(data, len, scale); 
	}
	template <class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, D, RGB_ORDER>(data, len, scale); 
	}
	template <EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, DATA_NOP, RGB_ORDER>(data, len, scale); 
	}
	void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, DATA_NOP, RGB>(data, len, scale); 
	}

};

#endif

#if defined(SPSR)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using SPDR registers and friends
//
// Technically speaking, this uses the AVR SPI registers.  This will work on the Teensy 3.0 because Paul made a set of compatability
// classes that map the AVR SPI registers to ARM's, however this caps the performance of output.  
//
// TODO: implement ARMHardwareSPIOutput
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRHardwareSPIOutput { 
	Selectable *m_pSelect;
	bool mWait;
public:
	AVRHardwareSPIOutput() { m_pSelect = NULL; mWait = false;}
	AVRHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void setSPIRate() { 
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR |= (1<<SPR0); } 
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }
	}
	
	void init() {
		volatile uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
#ifdef SPI_SELECT
		// Make sure the slave select line is set to output, or arduino will block us
		FastPin<SPI_SELECT>::setOutput();
		FastPin<SPI_SELECT>::lo();
#endif
		release();

		SPCR |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

		clr = SPSR; // clear SPI status register 
		clr = SPDR; // clear SPI data register
		clr; 

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR |= (1<<SPR0); } 
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }

	    SPDR=0;
	    shouldWait(false);
	}

	static bool shouldWait(bool wait = false) __attribute__((always_inline)) { 
		static bool sWait=false; 
		if(sWait) { sWait = wait; return true; } else { sWait = wait; return false; } 
		// return true;
	}
	static void wait() __attribute__((always_inline)) { if(shouldWait()) { while(!(SPSR & (1<<SPIF))); } }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR=b;  shouldWait(true); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; shouldWait(true); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; shouldWait(true); }

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		SPCR &= ~(1 << SPE);
		if(b & (1 << BIT)) { 
			FastPin<_DATA_PIN>::hi();
		} else { 
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
		SPCR |= 1 << SPE;
		shouldWait(false);
	}

	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } // FastPin<_SELECT_PIN>::hi(); }
	void release() { if(m_pSelect != NULL) { m_pSelect->release(); } } // FastPin<_SELECT_PIN>::lo(); }

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	void writeBytesValue(uint8_t value, int len) { 
		//setSPIRate();
		select();
		while(len--) { 
			writeByte(value);
		}
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();	
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			if(SKIP & FLAG_START_BIT) { 
				writeBit<0>(1);
			}
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			if(false && _SPI_CLOCK_DIVIDER == 0) { 
				writeByteNoWait(D::adjust(data[SPI_B0], scale)); delaycycles<13>();
				writeByteNoWait(D::adjust(data[SPI_B1], scale)); delaycycles<13>();
				writeByteNoWait(D::adjust(data[SPI_B2], scale)); delaycycles<9>();
			} else if(SKIP & FLAG_START_BIT) { 
				writeBytePostWait(D::adjust(data[SPI_B0], scale));
				writeBytePostWait(D::adjust(data[SPI_B1], scale));
				writeBytePostWait(D::adjust(data[SPI_B2], scale));
			} else { 
				writeByte(D::adjust(data[SPI_B0], scale));
				writeByte(D::adjust(data[SPI_B1], scale));
				writeByte(D::adjust(data[SPI_B2], scale));
			}

			data += SPI_ADVANCE;
		}
		D::postBlock(len);
		release();
	}

	template <uint8_t SKIP, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<SKIP, DATA_NOP, RGB_ORDER>(data, len, scale); 
	}
	template <class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, D, RGB_ORDER>(data, len, scale); 
	}
	template <EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, DATA_NOP, RGB_ORDER>(data, len, scale); 
	}
	void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		writeBytes3<0, DATA_NOP, RGB>(data, len, scale); 
	}

};
#endif

#else
// #define FORCE_SOFTWARE_SPI
#endif

#endif
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
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
#if defined(SPSR)
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
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

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
class AVRHardwareSPIOutput { 
	Selectable *m_pSelect;
public:
	AVRHardwareSPIOutput() { m_pSelect = NULL; }
	AVRHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		volatile uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
		release();

		SPCR |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

		clr = SPSR; // clear SPI status register 
		clr = SPDR; // clear SPI data register
		clr; 
	    bool b2x = false;
	    int hiBit = 0;
	    int spd = _SPI_SPEED;
	    while(spd >>= 1) { hiBit++; }
	 
	 	// Spped mappings are a little different, here they are based on the highest bit set in the speed parameter.  
	 	// If bit 8 is set, it's at osc/128, bit 7, then osc/64, etc... down the line.
	    switch(hiBit) { 
	      /* fosc/2   */ case 0: // no bits set 
	    				 case 1: // speed set to 1
	    				 case 2: // speed set to 2
	    				 	b2x=true; break;
	      /* fosc/4   */ case 3: break;
	      /* fosc/8   */ case 4: SPCR |= (1<<SPR0); b2x=true; break;
	      /* fosc/16  */ case 5: SPCR |= (1<<SPR0); break;
	      /* fosc/32  */ case 6: SPCR |= (1<<SPR1); b2x=true; break;
	      /* fosc/64  */ case 7: SPCR |= (1<<SPR1); break;
	      // /* fosc/64  */ case 6: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); b2x=true; break;
	      /* fosc/128 */ default: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); break;
	    }
	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }

	    // push 192 0s to prime the spi stuff
	    select();
	    SPDR = 0;
	    for(int i = 0; i < 191; i++) { 
	    	writeByte(0); writeByte(0); writeByte(0);
	    }
	    release();
	}

	static void wait() __attribute__((always_inline)) { while(!(SPSR & (1<<SPIF))); }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR=b; }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; }

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
	}

	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } // FastPin<_SELECT_PIN>::hi(); }
	void release() { if(m_pSelect != NULL) { m_pSelect->release(); } } // FastPin<_SELECT_PIN>::lo(); }

	void writeBytesValue(uint8_t value, int len) { 
		select();
		while(len--) { 
			writeBytePostWait(value);
		}
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
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
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			if(SKIP & FLAG_START_BIT) { 
				writeBit<0>(1);
			}
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			if(false && _SPI_SPEED == 0) { 
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
#else
// #define FORCE_SOFTWARE_SPI
#endif

#endif
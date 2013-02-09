#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock cycle counted delay loop
//
////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__arm__) 
# define NOP __asm__ __volatile__ ("nop\n");
#else
#  define NOP __asm__ __volatile__ ("cp r0,r0\n");
#endif

// predeclaration to not upset the compiler
template<int CYCLES> inline void delaycycles();

// worker template - this will nop for LOOP * 3 + PAD cycles total
template<int LOOP, int PAD> inline void _delaycycles() { 
	delaycycles<PAD>();
	// the loop below is 3 cycles * LOOP.  the LDI is one cycle,
	// the DEC is 1 cycle, the BRNE is 2 cycles if looping back and
	// 1 if not (the LDI balances out the BRNE being 1 cycle on exit)
	__asm__ __volatile__ ( 
		"		LDI R16, %0\n"
		"L_%=:  DEC R16\n"
		"		BRNE L_%=\n"
		: /* no outputs */ 
		: "M" (LOOP) 
		: "r16"
		);
}

// usable definition
#if 1
template<int CYCLES> inline void delaycycles() { 
	_delaycycles<CYCLES / 3, CYCLES % 3>();	
}
#else
template<int CYCLES> inline void delaycycles() { 
	NOP; delaycycles<CYCLES-1>();
}
#endif

// pre-instantiations for values small enough to not need the loop
template<> inline void delaycycles<0>() {}
template<> inline void delaycycles<1>() {NOP;}
template<> inline void delaycycles<2>() {NOP;NOP;}
template<> inline void delaycycles<3>() {NOP;NOP;NOP;}

class DATA_NOP { 
public:
	static uint8_t adjust(uint8_t data) { return data; } 
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Software SPI (aka bit-banging) support - with optimizations for when the clock and data pin are on the same port
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t LATCH_PIN, uint8_t SPI_SPEED>
class AVRSoftwareSPIOutput { 
public:
	static void init() {
		// set the pins to output
		Pin<DATA_PIN>::setOutput();
		Pin<LATCH_PIN>::setOutput();
		Pin<CLOCK_PIN>::setOutput();
		release();

		// Flush out a bunch of 0's to clear out the line
		writeBytesValue(0, 192*3);
	}

	static void wait() __attribute__((always_inline)) { }
	
	static void writeByte(uint8_t b) __attribute__((always_inline)) { 
		register volatile uint8_t *clockpin = Pin<CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<DATA_PIN>::port();
		writeByte(b, clockpin, datapin);
	}
	
	static void writeByte(uint8_t b, volatile uint8_t* clockpin, volatile uint8_t *datapin) __attribute__((always_inline)) { 
		writeBit<7>(b, clockpin, datapin);
		writeBit<6>(b, clockpin, datapin);
		writeBit<5>(b, clockpin, datapin);
		writeBit<4>(b, clockpin, datapin);
		writeBit<3>(b, clockpin, datapin);
		writeBit<2>(b, clockpin, datapin);
		writeBit<1>(b, clockpin, datapin);
		writeBit<0>(b, clockpin, datapin);
	}

	static void writeByte(uint8_t b, volatile uint8_t *datapin, 
						  uint8_t hival, uint8_t loval, uint8_t hiclock, uint8_t loclock) __attribute__((always_inline, hot)) { 
		writeBit<7>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, datapin, hival, loval, hiclock, loclock);
	}

	static void writeByte(uint8_t b, volatile uint8_t* clockpin, volatile uint8_t *datapin, 
						  uint8_t hival, uint8_t loval, uint8_t hiclock, uint8_t loclock) __attribute__((always_inline)) { 
		writeBit<7>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, clockpin, datapin, hival, loval, hiclock, loclock);
	}

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		register volatile uint8_t *clockpin = Pin<CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<DATA_PIN>::port();
		writeBit<BIT>(b, clockpin, datapin);
	}
	
	template <uint8_t BIT> inline static void writeBit(uint8_t b, volatile uint8_t *clockpin, volatile uint8_t *datapin) { 
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::hi(datapin);
		} else { 
			Pin<DATA_PIN>::lo(datapin);
		}

		Pin<CLOCK_PIN>::hi(clockpin);
		Pin<CLOCK_PIN>::lo(clockpin);
	}

	// the version of write to use when clock and data are on separate pins with precomputed values for setting
	// the clock and data pins
	template <uint8_t BIT> inline static void writeBit(uint8_t b, volatile uint8_t *clockpin, volatile uint8_t *datapin, 
													uint8_t hival, uint8_t loval, uint8_t hiclock, uint8_t loclock) { 
		// // only need to explicitly set clock hi if clock and data are on different ports
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::fastset(datapin, hival);
		} else { 
			// NOP;
			Pin<DATA_PIN>::fastset(datapin, loval);
		}


		Pin<CLOCK_PIN>::fastset(clockpin, hiclock);
		Pin<CLOCK_PIN>::fastset(clockpin, loclock);
	}

	// the version of write to use when clock and data are on the same pin with precomputed values for the various
	// combinations
	template <uint8_t BIT> inline static void writeBit(uint8_t b, volatile uint8_t *clockdatapin, 
													uint8_t datahiclockhi, uint8_t dataloclockhi, 
													uint8_t datahiclocklo, uint8_t dataloclocklo) { 
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::fastset(clockdatapin, datahiclockhi);
			Pin<DATA_PIN>::fastset(clockdatapin, datahiclocklo);
		} else { 
			// NOP;
			Pin<DATA_PIN>::fastset(clockdatapin, dataloclockhi);
			Pin<DATA_PIN>::fastset(clockdatapin, dataloclocklo);
		}
	}

	static void latch() { Pin<LATCH_PIN>::lo(); }
	static void release() { Pin<LATCH_PIN>::hi(); }

	static void writeBytesValue(uint8_t value, int len) { 
		latch();
		register volatile uint8_t *clockpin = Pin<CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register uint8_t datahi = Pin<DATA_PIN>::hival();
			register uint8_t datalo = Pin<DATA_PIN>::loval();
			register uint8_t clockhi = Pin<CLOCK_PIN>::hival();
			register uint8_t clocklo = Pin<CLOCK_PIN>::loval();
			while(len--) { 
				writeByte(value, clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register uint8_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register uint8_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();

			while(len--) { 
				writeByte(value, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
		release();	
	}

	// write a block of len uint8_ts out 
	template <class D> static void writeBytes(register uint8_t *data, int len) { 
		latch();
		register volatile uint8_t *clockpin = Pin<CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register uint8_t datahi = Pin<DATA_PIN>::hival();
			register uint8_t datalo = Pin<DATA_PIN>::loval();
			register uint8_t clockhi = Pin<CLOCK_PIN>::hival();
			register uint8_t clocklo = Pin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register uint8_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register uint8_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
		release();	
	}

	static void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }


	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D> static void writeBytes3(register uint8_t *data, int len) { 
		latch();
		register volatile uint8_t *clockpin = Pin<CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register uint8_t datahi = Pin<DATA_PIN>::hival();
			register uint8_t datalo = Pin<DATA_PIN>::loval();
			register uint8_t clockhi = Pin<CLOCK_PIN>::hival();
			register uint8_t clocklo = Pin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				data += SKIP;
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register uint8_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register uint8_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register uint8_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				data += SKIP;
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}

			release();
		}	
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using USART registers and friends
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _LATCH_PIN, uint8_t _SPI_SPEED>
class AVRUSARTSPIOutput { 
public:
	static void init() { 
		Pin<_LATCH_PIN>::setOutput();
		UBRR0 = 0;
		UCSR0A = 1<<TXC0;

		Pin<_CLOCK_PIN>::setOutpuut();
		Pin<_DATA_PIN>::setOutput();

		UCSR0C = _BV (UMSEL00) | _BV (UMSEL01);  // Master SPI mode
		UCSR0B = _BV (TXEN0) | _BV (RXEN0);  // transmit enable and receive enable

		// must be done last, see page 206
		UBRR0 = 3;  // 2 Mhz clock rate
	}

	static void wait() __attribute__((always_inline)) { while(!(UCSR0A & (1<<UDRE0))); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); UDR0 = b; }
	static void writeByte(uint8_t b, volatile uint8_t *, volatile uint8_t *,
						  uint8_t , uint8_t , uint8_t , uint8_t ) __attribute__((always_inline)) { writeByte(b); }
	static void writeByte(uint8_t b, volatile uint8_t *, 
						  uint8_t , uint8_t , uint8_t , uint8_t ) __attribute__((always_inline)) { writeByte(b); }

	static void writeByte(uint8_t b, volatile uint8_t*, volatile uint8_t*) __attribute__((always_inline)) { writeByte(b); }

	template <uint8_t BIT> inline static void writeBit(uint8_t b, volatile uint8_t *clockpin, volatile uint8_t *datapin) { 
		if(b & (1 << BIT)) { 
			Pin<_DATA_PIN>::hi(datapin);
		} else { 
			Pin<_DATA_PIN>::lo(datapin);
		}

		Pin<_CLOCK_PIN>::hi(clockpin);
		Pin<_CLOCK_PIN>::lo(clockpin);
	}

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		register volatile uint8_t *clockpin = Pin<_CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<_DATA_PIN>::port();
		writeBit<BIT>(b, clockpin, datapin);	
	}

	static void latch() { Pin<_LATCH_PIN>::lo(); }
	static void release() { 
		// wait for all transmissions to finish
  		while ((UCSR0A & (1 <<TXC0)) == 0) {}
    	Pin<_LATCH_PIN>::hi(); 
	}

	static void writeBytesValue(uint8_t value, int len) { 
		latch();
		while(len--) { 
			writeByte(value);
		}
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> static void writeBytes(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		latch();
		while(data != end) { 
			writeByte(D::adjust(*data++));
		}
		release();	
	}

	static void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D> static void writeBytes3(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		latch();
		while(data != end) { 
			data += SKIP;
			// a slight touch of delay here helps optimize the timing of the status register check loop 
			writeByte(D::adjust(*data++)); delaycycles<3>();
			writeByte(D::adjust(*data++)); delaycycles<3>();
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using SPDR registers and friends
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _LATCH_PIN, uint8_t _SPI_SPEED>
class AVRHardwareSPIOutput { 
public:
	static void init() {
		uint8_t clr;

		// set the pins to output
		Pin<_DATA_PIN>::setOutput();
		Pin<_LATCH_PIN>::setOutput();
		Pin<_CLOCK_PIN>::setOutput();
		release();

		SPCR |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

		clr = SPSR; // clear SPI status register 
		clr = SPDR; // clear SPI data register

	    bool b2x = false;
	    switch(_SPI_SPEED) { 
	      /* fosc/2   */ case 0: b2x=true; break;
	      /* fosc/4   */ case 1: break;
	      /* fosc/8   */ case 2: SPCR |= (1<<SPR0); b2x=true; break;
	      /* fosc/16  */ case 3: SPCR |= (1<<SPR0); break;
	      /* fosc/32  */ case 4: SPCR |= (1<<SPR1); b2x=true; break;
	      /* fosc/64  */ case 5: SPCR |= (1<<SPR1); break;
	      /* fosc/64  */ case 6: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); b2x=true; break;
	      /* fosc/128 */ default: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); break;
	    }
	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }

	    // push 192 0s to prime the spi stuff
	    latch();
	    SPDR = 0;
	    for(int i = 0; i < 191; i++) { 
	    	writeByte(0); writeByte(0); writeByte(0);
	    }
	    release();
	}

	static void wait() __attribute__((always_inline)) { while(!(SPSR & (1<<SPIF))); }

	// All the write functions stub out and degrade to the SPDR write.  The various parameter combinations are attempts to support
	// bitbanging optimizations
	static void writeByte(uint8_t b, volatile uint8_t *, volatile uint8_t *,
						  uint8_t , uint8_t , uint8_t , uint8_t ) __attribute__((always_inline)) { writeByte(b); }
	static void writeByte(uint8_t b, volatile uint8_t *, 
						  uint8_t , uint8_t , uint8_t , uint8_t ) __attribute__((always_inline)) { writeByte(b); }

	static void writeByte(uint8_t b, volatile uint8_t*, volatile uint8_t*) __attribute__((always_inline)) { writeByte(b); }
	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR=b; }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; }

	template <uint8_t BIT> inline static void writeBit(uint8_t b, volatile uint8_t *clockpin, volatile uint8_t *datapin) { 
		SPCR &= ~(1 << SPE);
		if(b & (1 << BIT)) { 
			Pin<_DATA_PIN>::hi(datapin);
		} else { 
			Pin<_DATA_PIN>::lo(datapin);
		}

		Pin<_CLOCK_PIN>::hi(clockpin);
		Pin<_CLOCK_PIN>::lo(clockpin);
		SPCR |= 1 << SPE;
	}

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		register volatile uint8_t *clockpin = Pin<_CLOCK_PIN>::port();
		register volatile uint8_t *datapin = Pin<_DATA_PIN>::port();
		writeBit<BIT>(b, clockpin, datapin);	
	}

	static void latch() { Pin<_LATCH_PIN>::lo(); }
	static void release() { Pin<_LATCH_PIN>::hi(); }

	static void writeBytesValue(uint8_t value, int len) { 
		latch();
		while(len--) { 
			writeByte(value);
		}
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> static void writeBytes(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		latch();
		while(data != end) { 
			writeByteNoWait(D::adjust(*data++));
		}
		release();	
	}

	static void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D> static void writeBytes3(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		latch();
		while(data != end) { 
			data += SKIP;
			// a slight touch of delay here helps optimize the timing of the status register check loop 
			if(_SPI_SPEED == 0) { 
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<9>();
			} else { 
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
			}
		}
		release();
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _LATCH_PIN, uint8_t _SPI_SPEED>
class AVRSPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _LATCH_PIN, _SPI_SPEED> {};

// uno/mini/duemilanove
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
#define SPI_DATA 11
#define SPI_CLOCK 13
template<uint8_t _LATCH, uint8_t SPI_SPEED>
class AVRSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> {};

#define USART_DATA 0
#define USART_CLOCK 4
template<uint8_t _LATCH, uint8_t SPI_SPEED>
class AVRSPIOutput<USART_DATA, USART_CLOCK, _LATCH, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, _LATCH, SPI_SPEED> {};

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__)
#define SPI_DATA 2
#define SPI_CLOCK 1
template<uint8_t _LATCH, uint8_t SPI_SPEED>
class AVRSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> {};


#else
#pragma message "No hardware SPI pins defined.  All SPI access will default to bitbanged output"
#endif

#endif

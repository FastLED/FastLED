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

// TODO: ARM version of _delaycycles_
// worker template - this will nop for LOOP * 3 + PAD cycles total
template<int LOOP, int PAD> inline void _delaycycles_AVR() { 
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
#if !defined(__MK20DX128__)
template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	_delaycycles_AVR<CYCLES / 3, CYCLES % 3>();	
}
#else
template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	NOP; delaycycles<CYCLES-1>();
}
#endif

// pre-instantiations for values small enough to not need the loop, as well as sanity holders
// for some negative values.
template<> __attribute__((always_inline)) inline void delaycycles<-3>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-2>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-1>() {}
template<> __attribute__((always_inline)) inline void delaycycles<0>() {}
template<> __attribute__((always_inline)) inline void delaycycles<1>() {NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<2>() {NOP;NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<3>() {NOP;NOP;NOP;}

/// Some of the SPI controllers will need to perform a transform on each byte before doing
/// anyting with it.  Creating a class of this form and passing it in as a template parameter to
/// writeBytes/writeBytes3 below will ensure that the body of this method will get called on every
/// byte worked on.  Recommendation, make the adjust method aggressively inlined.
///
/// TODO: Convinience macro for building these
class DATA_NOP { 
public:
	static __attribute__((always_inline)) inline uint8_t adjust(uint8_t data) { return data; } 
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Software SPI (aka bit-banging) support - with aggressive optimizations for when the clock and data pin are on the same port
//
// TODO: Replace the latch pin definition with a set of pins, to allow using mux hardware for routing in the future
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t LATCH_PIN, uint8_t SPI_SPEED>
class AVRSoftwareSPIOutput { 
	// The data types for pointers to the pin port - typedef'd here from the Pin definition because on avr these
	// are pointers to 8 bit values, while on arm they are 32 bit
	typedef typename Pin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename Pin<CLOCK_PIN>::port_ptr_t clock_ptr_t;

	// The data type for what's at a pin's port - typedef'd here from the Pin definition because on avr the ports
	// are 8 bits wide while on arm they are 32.
	typedef typename Pin<DATA_PIN>::port_t data_t;
	typedef typename Pin<CLOCK_PIN>::port_t clock_t;
public:
	static void init() {
		// set the pins to output and make sure the latch is released (which apparently means hi?  This is a bit
		// confusing to me)
		Pin<DATA_PIN>::setOutput();
		Pin<LATCH_PIN>::setOutput();
		Pin<CLOCK_PIN>::setOutput();
		release();
	}

	// stop the SPI output.  Pretty much a NOP with software, as there's no registers to kick
	static void stop() { }

	// wait until the SPI subsystem is ready for more data to write.  A NOP when bitbanging
	static void wait() __attribute__((always_inline)) { }
	
	// naive writeByte implelentation, simply calls writeBit on the 8 bits in the byte.
	static void writeByte(uint8_t b) __attribute__((always_inline)) { 
		writeBit<7>(b);
		writeBit<6>(b);
		writeBit<5>(b);
		writeBit<4>(b);
		writeBit<3>(b);
		writeBit<2>(b);
		writeBit<1>(b);
		writeBit<0>(b);
	}

private:	
	// writeByte implementation with data/clock registers passed in.
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin) __attribute__((always_inline)) { 
		writeBit<7>(b, clockpin, datapin);
		writeBit<6>(b, clockpin, datapin);
		writeBit<5>(b, clockpin, datapin);
		writeBit<4>(b, clockpin, datapin);
		writeBit<3>(b, clockpin, datapin);
		writeBit<2>(b, clockpin, datapin);
		writeBit<1>(b, clockpin, datapin);
		writeBit<0>(b, clockpin, datapin);
	}

	// writeByte implementation with the data register passed in and prebaked values for data hi w/clock hi and
	// low and data lo w/clock hi and lo.  This is to be used when clock and data are on the same GPIO register, 
	// can get close to getting a bit out the door in 2 clock cycles!
	static void writeByte(uint8_t b, data_ptr_t datapin, 
						  data_t hival, data_t loval, 
						  clock_t hiclock, clock_t loclock) __attribute__((always_inline, hot)) { 
		writeBit<7>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, datapin, hival, loval, hiclock, loclock);
	}

	// writeByte implementation with not just registers passed in, but pre-baked values for said registers for
	// data hi/lo and clock hi/lo values.  Note: weird things will happen if this method is called in cases where
	// the data and clock pins are on the same port!  Don't do that!
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin, 
						  data_t hival, data_t loval, 
						  clock_t hiclock, clock_t loclock) __attribute__((always_inline)) { 
		writeBit<7>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, clockpin, datapin, hival, loval, hiclock, loclock);
	}

public:
	#define SPI_DELAY delaycycles< (SPI_SPEED-2) / 2>();

	// write the BIT'th bit out via spi, setting the data pin then strobing the clcok
	template <uint8_t BIT> __attribute__((always_inline, hot)) inline static void writeBit(uint8_t b) { 
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::hi();
			if(SPI_SPEED < 3) { 
				Pin<CLOCK_PIN>::strobe();
			} else { 
				Pin<CLOCK_PIN>::hi(); SPI_DELAY;
				Pin<CLOCK_PIN>::lo(); SPI_DELAY;
			}
		} else { 
			Pin<DATA_PIN>::lo();
			if(SPI_SPEED < 3) { 
				Pin<CLOCK_PIN>::strobe();
			} else { 
				Pin<CLOCK_PIN>::hi(); SPI_DELAY;
				Pin<CLOCK_PIN>::lo(); SPI_DELAY;
			}
		}
	}
	
private:
	// write the BIT'th bit out via spi, setting the data pin then strobing the clock, using the passed in pin registers to accelerate access if needed
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin) { 
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::hi(datapin);
			Pin<CLOCK_PIN>::hi(clockpin); SPI_DELAY;
			Pin<CLOCK_PIN>::lo(clockpin); SPI_DELAY;
		} else { 
			Pin<DATA_PIN>::lo(datapin);
			Pin<CLOCK_PIN>::hi(clockpin); SPI_DELAY;
			Pin<CLOCK_PIN>::lo(clockpin); SPI_DELAY;
		}

	}

	// the version of write to use when clock and data are on separate pins with precomputed values for setting
	// the clock and data pins
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin, 
													data_t hival, data_t loval, clock_t hiclock, clock_t loclock) { 
		// // only need to explicitly set clock hi if clock and data are on different ports
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::fastset(datapin, hival);
			Pin<CLOCK_PIN>::fastset(clockpin, hiclock); SPI_DELAY;
			Pin<CLOCK_PIN>::fastset(clockpin, loclock); SPI_DELAY;
		} else { 
			// NOP;
			Pin<DATA_PIN>::fastset(datapin, loval);
			Pin<CLOCK_PIN>::fastset(clockpin, hiclock);
			Pin<CLOCK_PIN>::fastset(clockpin, loclock);
		}
	}

	// the version of write to use when clock and data are on the same pin with precomputed values for the various
	// combinations
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, data_ptr_t clockdatapin, 
													data_t datahiclockhi, data_t dataloclockhi, 
													data_t datahiclocklo, data_t dataloclocklo) { 
		if(b & (1 << BIT)) { 
			Pin<DATA_PIN>::fastset(clockdatapin, datahiclockhi); SPI_DELAY;
			Pin<DATA_PIN>::fastset(clockdatapin, datahiclocklo); SPI_DELAY;
		} else { 
			// NOP;
			Pin<DATA_PIN>::fastset(clockdatapin, dataloclockhi); SPI_DELAY;
			Pin<DATA_PIN>::fastset(clockdatapin, dataloclocklo); SPI_DELAY;
		}
	}
public:

	// latch the SPI output (TODO: research whether this really means hi or lo.  Alt TODO: move latch responsibility out of the SPI classes
	// entirely, make it up to the caller to remember to lock/latch the line?)
	static void latch() { Pin<LATCH_PIN>::lo(); }

	// release the SPI line
	static void release() { Pin<LATCH_PIN>::hi(); }

	// Write out len bytes of the given value out over SPI.  Useful for quickly flushing, say, a line of 0's down the line.
	static void writeBytesValue(uint8_t value, int len) { 
		latch();
#if 0
		// TODO: Weird things may happen if software bitbanging SPI output and other pins on the output reigsters are being twiddled.  Need
		// to allow specifying whether or not exclusive i/o access is allowed during this process, and if i/o access is not allowed fall
		// back to the degenerative code below
		while(len--) { 
			writeByte(value);
		}
#else
		register clock_ptr_t clockpin = Pin<CLOCK_PIN>::port();
		register data_ptr_t datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = Pin<DATA_PIN>::hival();
			register data_t datalo = Pin<DATA_PIN>::loval();
			register clock_t clockhi = Pin<CLOCK_PIN>::hival();
			register clock_t clocklo = Pin<CLOCK_PIN>::loval();
			while(len--) { 
				writeByte(value, clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register data_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();

			while(len--) { 
				writeByte(value, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
#endif
		release();	
	}

	// write a block of len uint8_ts out.  Need to type this better so that explicit casts into the call aren't required.
	// note that this template version takes a class parameter for a per-byte modifier to the data. 
	template <class D> static void writeBytes(register uint8_t *data, int len) { 
		latch();
#if 0
		uint8_t *end = data + len;
		while(data != end) { 
			writeByte(D::adjust(*data++));
		}
#else
		register clock_ptr_t clockpin = Pin<CLOCK_PIN>::port();
		register data_ptr_t datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = Pin<DATA_PIN>::hival();
			register data_t datalo = Pin<DATA_PIN>::loval();
			register clock_t clockhi = Pin<CLOCK_PIN>::hival();
			register clock_t clocklo = Pin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register data_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
#endif
		release();	
	}

	// default version of writing a block of data out to the SPI port, with no data modifications being made
	static void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }


	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning of each grouping, as well as a class specifying a per
	// byte of data modification to be made.  (See DATA_NOP above)
	template <uint8_t SKIP, class D> static void writeBytes3(register uint8_t *data, int len) { 
		latch();

#if 0
		// If interrupts or other things may be generating output while we're working on things, then we need
		// to use this block
		uint8_t *end = data + len;
		while(data != end) { 
			data += SKIP;
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
		}
#else
		// If we can guaruntee that no one else will be writing data while we are running (namely, changing the values of the PORT/PDOR pins)
		// then we can use a bunch of optimizations in here
		register clock_ptr_t clockpin = Pin<CLOCK_PIN>::port();
		register data_ptr_t datapin = Pin<DATA_PIN>::port();

		if(Pin<DATA_PIN>::port() != Pin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = Pin<DATA_PIN>::hival();
			register data_t datalo = Pin<DATA_PIN>::loval();
			register clock_t clockhi = Pin<CLOCK_PIN>::hival();
			register clock_t clocklo = Pin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				data += SKIP;
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register data_t datahi_clockhi = Pin<DATA_PIN>::hival() | Pin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = Pin<DATA_PIN>::loval() | Pin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = Pin<DATA_PIN>::hival() & ~Pin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = Pin<DATA_PIN>::loval() & ~Pin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				data += SKIP;
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}	
#endif
		release();
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using USART registers and friends
//
// TODO: Complete/test implementation - right now this doesn't work
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// uno/mini/duemilanove
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _LATCH_PIN, uint8_t _SPI_SPEED>
class AVRUSARTSPIOutput { 
public:
	static void init() { 
		Pin<_LATCH_PIN>::setOutput();
		UBRR0 = 0;
		UCSR0A = 1<<TXC0;

		Pin<_CLOCK_PIN>::setOutput();
		Pin<_DATA_PIN>::setOutput();

		UCSR0C = _BV (UMSEL00) | _BV (UMSEL01);  // Master SPI mode
		UCSR0B = _BV (TXEN0) | _BV (RXEN0);  // transmit enable and receive enable

		// must be done last, see page 206
		UBRR0 = 3;  // 2 Mhz clock rate
	}

	static void stop() { 
		// TODO: stop the uart spi output
	}

	static void wait() __attribute__((always_inline)) { while(!(UCSR0A & (1<<UDRE0))); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); UDR0 = b; }

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		if(b && (1 << BIT)) { 
			Pin<_DATA_PIN>::hi();
		} else { 
			Pin<_DATA_PIN>::lo();
		}

		Pin<_CLOCK_PIN>::hi();
		Pin<_CLOCK_PIN>::lo();
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
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
#else
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
#endif
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
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
#else
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
			writeByte(D::adjust(*data++)); delaycycles<3>();
			writeByte(D::adjust(*data++)); delaycycles<3>();
#endif
		}
		release();
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

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
	    latch();
	    SPDR = 0;
	    for(int i = 0; i < 191; i++) { 
	    	writeByte(0); writeByte(0); writeByte(0);
	    }
	    release();
	}

	static void wait() __attribute__((always_inline)) { while(!(SPSR & (1<<SPIF))); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR=b; }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; }

	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		SPCR &= ~(1 << SPE);
		if(b & (1 << BIT)) { 
			Pin<_DATA_PIN>::hi();
		} else { 
			Pin<_DATA_PIN>::lo();
		}

		Pin<_CLOCK_PIN>::hi();
		Pin<_CLOCK_PIN>::lo();
		SPCR |= 1 << SPE;
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
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
#else
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
#endif
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
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
#else 
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			if(_SPI_SPEED == 0) { 
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<9>();
			} else { 
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
			}
#endif
		}
		release();
	}

	template <uint8_t SKIP> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	static void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

};

// Clock speed dividers 
#define SPEED_DIV_2 2
#define SPEED_DIV_4 4
#define SPEED_DIV_8 8
#define SPEED_DIV_16 16
#define SPEED_DIV_32 32
#define SPEED_DIV_64 64
#define SPEED_DIV_128 128


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports on platforms/builds where the pin
// mappings are known at compile time.
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

// #define USART_DATA 0
// #define USART_CLOCK 4
// template<uint8_t _LATCH, uint8_t SPI_SPEED>
// class AVRSPIOutput<USART_DATA, USART_CLOCK, _LATCH, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, _LATCH, SPI_SPEED> {};

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__)
#define SPI_DATA 2
#define SPI_CLOCK 1
template<uint8_t _LATCH, uint8_t SPI_SPEED>
class AVRSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> {};

#elif defined(__MK20DX128__)   // for Teensy 3.0
#define SPI_DATA 11
#define SPI_CLOCK 13
template<uint8_t _LATCH, uint8_t SPI_SPEED>
class AVRSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, _LATCH, SPI_SPEED> {};

#else
#pragma message "No hardware SPI pins defined.  All SPI access will default to bitbanged output"
#endif

#endif

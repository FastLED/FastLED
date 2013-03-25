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

#define FLAG_START_BIT 0x80
#define MASK_SKIP_BITS 0x3F

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Software SPI (aka bit-banging) support - with aggressive optimizations for when the clock and data pin are on the same port
//
// TODO: Replace the select pin definition with a set of pins, to allow using mux hardware for routing in the future
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SPI_SPEED>
class AVRSoftwareSPIOutput { 
	// The data types for pointers to the pin port - typedef'd here from the Pin definition because on avr these
	// are pointers to 8 bit values, while on arm they are 32 bit
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<CLOCK_PIN>::port_ptr_t clock_ptr_t;

	// The data type for what's at a pin's port - typedef'd here from the Pin definition because on avr the ports
	// are 8 bits wide while on arm they are 32.
	typedef typename FastPin<DATA_PIN>::port_t data_t;
	typedef typename FastPin<CLOCK_PIN>::port_t clock_t;
	Selectable 	*m_pSelect;

public:
	AVRSoftwareSPIOutput() { m_pSelect = NULL; }
	AVRSoftwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		// set the pins to output and make sure the select is released (which apparently means hi?  This is a bit
		// confusing to me)
		FastPin<DATA_PIN>::setOutput();
		FastPin<CLOCK_PIN>::setOutput();
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
			FastPin<DATA_PIN>::hi();
			if(SPI_SPEED < 3) { 
				FastPin<CLOCK_PIN>::strobe();
			} else { 
				FastPin<CLOCK_PIN>::hi(); SPI_DELAY;
				FastPin<CLOCK_PIN>::lo(); SPI_DELAY;
			}
		} else { 
			FastPin<DATA_PIN>::lo();
			if(SPI_SPEED < 3) { 
				FastPin<CLOCK_PIN>::strobe();
			} else { 
				FastPin<CLOCK_PIN>::hi(); SPI_DELAY;
				FastPin<CLOCK_PIN>::lo(); SPI_DELAY;
			}
		}
	}
	
private:
	// write the BIT'th bit out via spi, setting the data pin then strobing the clock, using the passed in pin registers to accelerate access if needed
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin) { 
		if(b & (1 << BIT)) { 
			FastPin<DATA_PIN>::hi(datapin);
			FastPin<CLOCK_PIN>::hi(clockpin); SPI_DELAY;
			FastPin<CLOCK_PIN>::lo(clockpin); SPI_DELAY;
		} else { 
			FastPin<DATA_PIN>::lo(datapin);
			FastPin<CLOCK_PIN>::hi(clockpin); SPI_DELAY;
			FastPin<CLOCK_PIN>::lo(clockpin); SPI_DELAY;
		}

	}

	// the version of write to use when clock and data are on separate pins with precomputed values for setting
	// the clock and data pins
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin, 
													data_t hival, data_t loval, clock_t hiclock, clock_t loclock) { 
		// // only need to explicitly set clock hi if clock and data are on different ports
		if(b & (1 << BIT)) { 
			FastPin<DATA_PIN>::fastset(datapin, hival);
			FastPin<CLOCK_PIN>::fastset(clockpin, hiclock); SPI_DELAY;
			FastPin<CLOCK_PIN>::fastset(clockpin, loclock); SPI_DELAY;
		} else { 
			// NOP;
			FastPin<DATA_PIN>::fastset(datapin, loval);
			FastPin<CLOCK_PIN>::fastset(clockpin, hiclock);
			FastPin<CLOCK_PIN>::fastset(clockpin, loclock);
		}
	}

	// the version of write to use when clock and data are on the same pin with precomputed values for the various
	// combinations
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, data_ptr_t clockdatapin, 
													data_t datahiclockhi, data_t dataloclockhi, 
													data_t datahiclocklo, data_t dataloclocklo) { 
#if 0
		writeBit<BIT>(b);
#else
		if(b & (1 << BIT)) { 
			FastPin<DATA_PIN>::fastset(clockdatapin, datahiclocklo); SPI_DELAY;
			FastPin<DATA_PIN>::fastset(clockdatapin, datahiclockhi); SPI_DELAY;
		} else { 
			// NOP;
			FastPin<DATA_PIN>::fastset(clockdatapin, dataloclocklo); SPI_DELAY;
			FastPin<DATA_PIN>::fastset(clockdatapin, dataloclockhi); SPI_DELAY;
		}
#endif
	}
public:

	// select the SPI output (TODO: research whether this really means hi or lo.  Alt TODO: move select responsibility out of the SPI classes
	// entirely, make it up to the caller to remember to lock/select the line?)
	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } // FastPin<SELECT_PIN>::hi(); }

	// release the SPI line
	void release() { if(m_pSelect != NULL) { m_pSelect->release(); } } // FastPin<SELECT_PIN>::lo(); }

	// Write out len bytes of the given value out over SPI.  Useful for quickly flushing, say, a line of 0's down the line.
	void writeBytesValue(uint8_t value, int len) { 
		select();
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// TODO: Weird things may happen if software bitbanging SPI output and other pins on the output reigsters are being twiddled.  Need
		// to allow specifying whether or not exclusive i/o access is allowed during this process, and if i/o access is not allowed fall
		// back to the degenerative code below
		while(len--) { 
			writeByte(value);
		}
#else
		register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();
			while(len--) { 
				writeByte(value, clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();

			while(len--) { 
				writeByte(value, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
#endif
		release();	
	}

	// write a block of len uint8_ts out.  Need to type this better so that explicit casts into the call aren't required.
	// note that this template version takes a class parameter for a per-byte modifier to the data. 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
		select();
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		uint8_t *end = data + len;
		while(data != end) { 
			writeByte(D::adjust(*data++));
		}
#else
		register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			FastPin<CLOCK_PIN>::hi();
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
			FastPin<CLOCK_PIN>::lo();
		}
#endif
		release();	
	}

	// default version of writing a block of data out to the SPI port, with no data modifications being made
	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }


	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning of each grouping, as well as a class specifying a per
	// byte of data modification to be made.  (See DATA_NOP above)
	template <uint8_t SKIP, class D> void writeBytes3(register uint8_t *data, int len) { 
		select();

#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// If interrupts or other things may be generating output while we're working on things, then we need
		// to use this block
		uint8_t *end = data + len;
		while(data != end) { 
			data += (MASK_SKIP_BITS & SKIP);
			if(SKIP & FLAG_START_BIT) { 
				writeBit<0>(1);
			}
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
		}
#else
		// If we can guaruntee that no one else will be writing data while we are running (namely, changing the values of the PORT/PDOR pins)
		// then we can use a bunch of optimizations in here
		register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) { 
				data += (MASK_SKIP_BITS & SKIP);
				if(SKIP & FLAG_START_BIT) { 
					writeBit<0>(1, clockpin, datapin, datahi, datalo, clockhi, clocklo);
				}
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins 
			FastPin<CLOCK_PIN>::hi();
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();
			
			uint8_t *end = data + len;

			while(data != end) { 
				data += (MASK_SKIP_BITS & SKIP);
				if(SKIP & FLAG_START_BIT) { 
					writeBit<0>(1, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				}
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
			FastPin<CLOCK_PIN>::lo();
		}	
#endif
		release();
	}

	template <uint8_t SKIP> void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }
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
	template <uint8_t SKIP, class D> void writeBytes3(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			data += (MASK_SKIP_BITS & SKIP);
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

	template <uint8_t SKIP> void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

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
		uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
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
	    select();
	    SPDR = 0;
	    for(int i = 0; i < 191; i++) { 
	    	writeByte(0); writeByte(0); writeByte(0);
	    }
	    release();
	}

	static void wait() __attribute__((always_inline)) { while(!(SPSR & (1<<SPIF))); }

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
	template <uint8_t SKIP, class D> void writeBytes3(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			data += (MASK_SKIP_BITS & SKIP);
			if(SKIP & FLAG_START_BIT) { 
				writeBit<0>(1);
			}
#if defined(__MK20DX128__) 
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
			writeByte(D::adjust(*data++));
#else 
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			if(false && _SPI_SPEED == 0) { 
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<13>();
				writeByteNoWait(D::adjust(*data++)); delaycycles<9>();
			} else if(SKIP & FLAG_START_BIT) { 
				writeBytePostWait(D::adjust(*data++));
				writeBytePostWait(D::adjust(*data++));
				writeBytePostWait(D::adjust(*data++));
			} else { 
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
				writeByte(D::adjust(*data++)); delaycycles<3>();
			}
#endif
		}
		release();
	}

	template <uint8_t SKIP> void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

};

#if defined(__MK20DX128__) && defined(CORE_TEENSY)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
class ARMHardwareSPIOutput { 
	Selectable *m_pSelect;
public:
	ARMHardwareSPIOutput() { m_pSelect = NULL; }
	ARMHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

    static inline void update_ctar0(uint32_t ctar) __attribute__((always_inline)) {
            if (SPI0_CTAR0 == ctar) return;
            uint32_t mcr = SPI0_MCR;
            if (mcr & SPI_MCR_MDIS) {
                    SPI0_CTAR0 = ctar;
            } else {
                    SPI0_MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
                    SPI0_CTAR0 = ctar;
                    SPI0_MCR = mcr;
            }
    }	

    static inline void update_ctar1(uint32_t ctar) __attribute__((always_inline)) {
            if (SPI0_CTAR1 == ctar) return;
            uint32_t mcr = SPI0_MCR;
            if (mcr & SPI_MCR_MDIS) {
                    SPI0_CTAR1 = ctar;
            } else {
                    SPI0_MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
                    SPI0_CTAR1 = ctar;
                    SPI0_MCR = mcr;
          
            }
    }	

    static inline void set_ctar1_bits(int bits) { 
	    // Set ctar1 to 16 bits
	    int ctar = SPI0_CTAR1;
	    
	    // clear the FMSZ bits
	    ctar &= SPI_CTAR_FMSZ(0x0F);
	    ctar |= SPI_CTAR_FMSZ((bits-1) & 0x0F);

	    update_ctar1(ctar);
    }

	void init() {
		uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
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

	    // force speed faster
	    switch(_SPI_SPEED) {
	    	case 0: // ~20Mbps
		    	{ 
				    uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0) | SPI_CTAR_DBR;
			  		uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0) | SPI_CTAR_DBR;
				    update_ctar0(ctar0);
				    update_ctar1(ctar1);
				    break;
				}
	    	case 1: // ~15Mbps
		    	{ 
				    uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0) | SPI_CTAR_DBR;
			  		uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0) | SPI_CTAR_DBR;
				    update_ctar0(ctar0);
				    update_ctar1(ctar1);
				    break;
				}
	    	case 2: // ~10Mbps
		    	{ 
				    uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
			  		uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
				    update_ctar0(ctar0);
				    update_ctar1(ctar1);
				    break;
				}
			default:
				{
					// Configure ctar1 to be 16 bits based off of ctar0's settings
					update_ctar1(SPI0_CTAR0);

					set_ctar1_bits(16);
				}
	    }

	    // push 192 0s to prime the spi stuff
	    select();
	    writeByteNoWait(0);
	    for(int i = 0; i < 191; i++) { 
	    	writeByte(0); writeByte(0); writeByte(0);
	    }
	    waitFully();
	    release();
	}

	static void waitFully() __attribute__((always_inline)) { 
		while( (SPI0_SR & 0xF000) > 0); 
		while (!(SPI0_SR & SPI_SR_TCF)); 
		SPI0_SR |= SPI_SR_TCF; 
	}
	static void wait() __attribute__((always_inline)) { while( (SPI0_SR & 0xF000) >= 0x4000);  }
	

	static void writeWord(uint16_t w) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI0_PUSHR_CTAS(1) | (w & 0xFFFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPI0_PUSHR = (b & 0xFF); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = (b & 0xFF); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR =  (b & 0xFF); }

	// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		uint32_t ctar1_save = SPI0_CTAR1;

		// Clear out the FMSZ bits, reset them for 9 bits transferd for the start bit
		uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(8);
		update_ctar1(ctar1);

		writeWord( (b & (1 << BIT)) != 0);

		update_ctar1(ctar1_save);
	}

	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } 
	void release() { if(m_pSelect != NULL) { m_pSelect->release(); } } 

	void writeBytesValue(uint8_t value, int len) { 
		select();
		while(len--) { 
			writeByte(value);
		}
		waitFully();
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			writeByte(D::adjust(*data++));
		}
		waitFully();
		release();	
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D> void writeBytes3(register uint8_t *data, int len) { 
		uint8_t *end = data + len;
		select();
		if(false && !SKIP && ((len % 2) == 0)) {
			switch(len % 8) { 
				case 0: while(data != end) { 
					writeWord(D::adjust(*data++) << 8 | D::adjust(*data++));
				case 6: 
					writeWord(D::adjust(*data++) << 8 | D::adjust(*data++));
				case 4:
					writeWord(D::adjust(*data++) << 8 | D::adjust(*data++));
				case 2:
					writeWord(D::adjust(*data++) << 8 | D::adjust(*data++));
					wait();
				}
			}
			waitFully();
		} else if(SKIP & FLAG_START_BIT) {
			uint32_t ctar1_save = SPI0_CTAR1;

			// Clear out the FMSZ bits, reset them for 9 bits transferd for the start bit
			uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(8);
			update_ctar1(ctar1);

			while(data != end) { 
				data += (MASK_SKIP_BITS & SKIP);
				writeWord( 0x100 | D::adjust(*data++));
				writeByte(D::adjust(*data++));
				writeByte(D::adjust(*data++));
			}
			waitFully();

			// restore ctar1
			update_ctar1(ctar1_save);
		} else {
			while(data != end) { 
				data += (MASK_SKIP_BITS & SKIP);
				writeByte(D::adjust(*data++));
				writeWord(D::adjust(*data++) << 8 | D::adjust(*data++));
			}
			waitFully();
		}
		release();
	}

	template <uint8_t SKIP> void writeBytes3(register uint8_t *data, int len) { writeBytes3<SKIP, DATA_NOP>(data, len); }
	template <class D> void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, D>(data, len); }
	void writeBytes3(register uint8_t *data, int len) { writeBytes3<0, DATA_NOP>(data, len); }

};
#endif

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

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
class SPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
class SoftwareSPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

#ifndef FORCE_SOFTWARE_SPI
#if defined(SPI_DATA) && defined(SPI_CLOCK)

#if defined(__MK20DX128__) && defined(CORE_TEENSY)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#else

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#endif

#else
#warning "No hardware SPI pins defined.  All SPI access will default to bitbanged output"

#endif

// #if defined(USART_DATA) && defined(USART_CLOCK)
// template<uint8_t SPI_SPEED>
// class AVRSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> {};
// #endif

#else
#warning "Forcing software SPI - no hardware SPI for you!"
#endif 

#endif

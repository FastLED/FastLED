#ifndef __INC_FASTSPI_ARM_H
#define __INC_FASTSPI_ARM_H


#if defined(__MK20DX128__) && defined(CORE_TEENSY)

#ifndef SPI_PUSHR_CONT
#define SPI_PUSHR_CONT SPI0_PUSHR_CONT
#define SPI_PUSHR_CTAS(X) SPI0_PUSHR_CTAS(X)
#define SPI_PUSHR_EOQ SPI0_PUSHR_EOQ
#define SPI_PUSHR_CTCNT SPI0_PUSHR_CTCNT
#define SPI_PUSHR_PCS(X) SPI0_PUSHR_PCS(X)
#endif

// Template function that, on compilation, expands to a constant representing the highest bit set in a byte.  Right now, 
// if no bits are set (value is 0), it returns 0, which is also the value returned if the lowest bit is the only bit
// set (the zero-th bit).  Unclear if I  will want this to change at some point.
template<int VAL, int BIT> class BitWork { 
	public: 
		static int highestBit() __attribute__((always_inline)) { return (VAL & 1 << BIT) ? BIT : BitWork<VAL, BIT-1>::highestBit(); } 
};
template<int VAL> class BitWork<VAL, 0> { 
	public: 
		static int highestBit() __attribute__((always_inline)) { return 0; } 
};

#define MAX(A, B) (( (A) > (B) ) ? (A) : (B))

#define USE_CONT 0

// Templated function to translate a clock divider value into the prescalar, scalar, and clock doubling setting for the world.
template <int VAL> void getScalars(uint32_t & preScalar, uint32_t & scalar, uint32_t & dbl) {
    switch(VAL) {
    		// Handle the dbl clock cases
    		case 0: case 1:
            case 2: preScalar = 0; scalar = 0; dbl = 1; break;
            case 3: preScalar = 1; scalar = 0; dbl = 1; break;
            case 5: preScalar = 2; scalar = 0; dbl = 1; break;
            case 7: preScalar = 3; scalar = 0; dbl = 1; break;

            // Handle the scalar value 6 cases (since it's not a power of two, it won't get caught
            // below)
            case 9: preScalar = 1; scalar = 2; dbl = 1; break;
            case 18: case 19: preScalar = 1; scalar = 2; dbl = 0; break;

            case 15: preScalar = 2; scalar = 2; dbl = 1; break;
            case 30: case 31: preScalar = 2; scalar = 2; dbl = 0; break;

            case 21: case 22: case 23: preScalar = 3; scalar = 2; dbl = 1; break;
            case 42: case 43: case 44: case 45: case 46: case 47: preScalar = 3; scalar = 2; dbl = 0; break;
            default: {
                int p2 = BitWork<VAL/2, 15>::highestBit();
                int p3 = BitWork<VAL/3, 15>::highestBit();
                int p5 = BitWork<VAL/5, 15>::highestBit();
                int p7 = BitWork<VAL/7, 15>::highestBit();

                int w2 = 2 * (1 << p2);
                int w3 = (VAL/3) > 0 ? 3 * (1 << p3) : 0;
                int w5 = (VAL/5) > 0 ? 5 * (1 << p5) : 0;
                int w7 = (VAL/7) > 0 ? 7 * (1 << p7) : 0;

                int maxval = MAX(MAX(w2, w3), MAX(w5, w7));

                if(w2 == maxval) { preScalar = 0; scalar = p2; }
                else if(w3 == maxval) { preScalar = 1; scalar = p3; }
                else if(w5 == maxval) { preScalar = 2; scalar = p5; }
                else if(w7 == maxval) { preScalar = 3; scalar = p7; }

                dbl = 0;
                if(scalar == 0) { dbl = 1; }
                else if(scalar < 3) { scalar--; }
            }
    }
	return;
}


template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class ARMHardwareSPIOutput { 
	Selectable *m_pSelect;

	// Borrowed from the teensy3 SPSR emulation code
	static inline void enable_pins(void) __attribute__((always_inline)) {
		//serial_print("enable_pins\n");
		CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
		CORE_PIN12_CONFIG = PORT_PCR_MUX(2);
		CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
	}

	// Borrowed from the teensy3 SPSR emulation code
	static inline void disable_pins(void) __attribute__((always_inline)) {
		//serial_print("disable_pins\n");
		CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
		CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
		CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
	}

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

    static inline void set_ctar0_bits(int bits) { 
	    // Set ctar1 to 16 bits
	    int ctar = SPI0_CTAR1;
	    
	    // clear the FMSZ bits
	    ctar &= SPI_CTAR_FMSZ(0x0F);
	    ctar |= SPI_CTAR_FMSZ((bits-1) & 0x0F);

	    update_ctar1(ctar);
    }


    void setSPIRate() { 
		// Configure CTAR0, defaulting to 8 bits and CTAR1, defaulting to 16 bits
	 	uint32_t _PBR = 0;
	 	uint32_t _BR = 0;
	 	uint32_t _CSSCK = 0;
	 	uint32_t _DBR = 0;

	 	// if(_SPI_CLOCK_DIVIDER >= 256) 		{ _PBR = 0; _BR = _CSSCK = 7; _DBR = 0; } // osc/256
	 	// else if(_SPI_CLOCK_DIVIDER >= 128) 	{ _PBR = 0; _BR = _CSSCK = 6; _DBR = 0; } // osc/128
	 	// else if(_SPI_CLOCK_DIVIDER >= 64) 	{ _PBR = 0; _BR = _CSSCK = 5; _DBR = 0; } // osc/64
	 	// else if(_SPI_CLOCK_DIVIDER >= 32) 	{ _PBR = 0; _BR = _CSSCK = 4; _DBR = 0; } // osc/32
	 	// else if(_SPI_CLOCK_DIVIDER >= 16) 	{ _PBR = 0; _BR = _CSSCK = 3; _DBR = 0; } // osc/16
	 	// else if(_SPI_CLOCK_DIVIDER >= 8) 	{ _PBR = 0; _BR = _CSSCK = 1; _DBR = 0; } // osc/8
	 	// else if(_SPI_CLOCK_DIVIDER >= 7) 	{ _PBR = 3; _BR = _CSSCK = 0; _DBR = 1; } // osc/7
	 	// else if(_SPI_CLOCK_DIVIDER >= 5) 	{ _PBR = 2; _BR = _CSSCK = 0; _DBR = 1; } // osc/5
	 	// else if(_SPI_CLOCK_DIVIDER >= 4) 	{ _PBR = 0; _BR = _CSSCK = 0; _DBR = 0; } // osc/4
	 	// else if(_SPI_CLOCK_DIVIDER >= 3) 	{ _PBR = 1; _BR = _CSSCK = 0; _DBR = 1; } // osc/3
	 	// else                                { _PBR = 0; _BR = _CSSCK = 0; _DBR = 1; } // osc/2

	 	getScalars<_SPI_CLOCK_DIVIDER>(_PBR, _BR, _DBR);
	 	_CSSCK = _BR;

	 	uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK);
	 	uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK);

#if USE_CONT == 1
	 	ctar0 |= SPI_CTAR_CPHA | SPI_CTAR_CPOL;
	 	ctar1 |= SPI_CTAR_CPHA | SPI_CTAR_CPOL;
#endif

	 	if(_DBR) { 
	 		ctar0 |= SPI_CTAR_DBR;
	 		ctar1 |= SPI_CTAR_DBR;
	 	}

	    update_ctar0(ctar0);
	    update_ctar1(ctar1);

    }
	
	void init() {
		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
		release();

		// Enable SPI0 clock
		uint32_t sim6 = SIM_SCGC6;
		if (!(sim6 & SIM_SCGC6_SPI0)) {
			//serial_print("init1\n");
			SIM_SCGC6 = sim6 | SIM_SCGC6_SPI0;
			SPI0_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(1);
		}

		setSPIRate();

		// Configure SPI as the master and enable 
		SPI0_MCR |= SPI_MCR_MSTR; // | SPI_MCR_CONT_SCKE);
		SPI0_MCR &= ~(SPI_MCR_MDIS | SPI_MCR_HALT);

		enable_pins();
	}

	static void waitFully() __attribute__((always_inline)) { 
		while( (SPI0_SR & 0xF000) > 0); 
		while (!(SPI0_SR & SPI_SR_TCF)); 
		SPI0_SR |= (SPI_SR_TCF | SPI_SR_EOQF); 
	}

	static bool needwait() __attribute__((always_inline)) { return (SPI0_SR & 0x4000); }
	static void wait() __attribute__((always_inline)) { while( (SPI0_SR & 0x4000) );  }
	static void wait1() __attribute__((always_inline)) { while( (SPI0_SR & 0xF000) >= 0x2000);  }
	
	enum ECont { CONT, NOCONT };
	enum EWait { PRE, POST, NONE };
	enum ELast { NOTLAST, LAST };

#if USE_CONT == 1
	#define CM CONT
#else
	#define CM NOCONT
#endif
	#define WM PRE

	template<ECont CONT_STATE, EWait WAIT_STATE, ELast LAST_STATE> class Write { 
	public:
		static void writeWord(uint16_t w) __attribute__((always_inline)) { 
			if(WAIT_STATE == PRE) { wait(); }
			SPI0_PUSHR = ((LAST_STATE == LAST) ? SPI_PUSHR_EOQ : 0) |
			             ((CONT_STATE == CONT) ? SPI_PUSHR_CONT : 0) | 
			             SPI_PUSHR_CTAS(1) | (w & 0xFFFF);
			if(WAIT_STATE == POST) { wait(); }
		}

		static void writeByte(uint8_t b) __attribute__((always_inline)) { 
			if(WAIT_STATE == PRE) { wait(); }
			SPI0_PUSHR = ((LAST_STATE == LAST) ? SPI_PUSHR_EOQ : 0) |
			             ((CONT_STATE == CONT) ? SPI_PUSHR_CONT : 0) | 
			             SPI_PUSHR_CTAS(0) | (b & 0xFF);
			if(WAIT_STATE == POST) { wait(); }
		}
	};

	static void writeWord(uint16_t w) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI_PUSHR_CTAS(1) | (w & 0xFFFF); }
	static void writeWordNoWait(uint16_t w) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CTAS(1) | (w & 0xFFFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF); }

	static void writeWordCont(uint16_t w) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | (w & 0xFFFF); }
	static void writeWordContNoWait(uint16_t w) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | (w & 0xFFFF); }

	static void writeByteCont(uint8_t b) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); }
	static void writeByteContPostWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); wait(); }
	static void writeByteContNoWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); }

	// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { 
		uint32_t ctar1_save = SPI0_CTAR1;

		// Clear out the FMSZ bits, reset them for 9 bits transferd for the start bit
		uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(0);
		update_ctar1(ctar1);

		writeWord( (b & (1 << BIT)) != 0);

		update_ctar1(ctar1_save);
	}

	void inline select() __attribute__((always_inline)) { if(m_pSelect != NULL) { m_pSelect->select(); } } 
	void inline release() __attribute__((always_inline)) { if(m_pSelect != NULL) { m_pSelect->release(); } } 

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { Write<CM, WM, NOTLAST>::writeByte(value); }
	}

	void writeBytesValue(uint8_t value, int len) { 
		setSPIRate();
		select();
		while(len--) { 
			writeByte(value);
		}
		waitFully();
		release();
	}
	
	// Write a block of n uint8_ts out 
	template <class D> void writeBytes(register uint8_t *data, int len) { 
		setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) { 
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();	
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t SKIP, class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		// setSPIRate();
		uint8_t *end = data + len;
		select();
		if((SKIP & FLAG_START_BIT) == 0) {
			//If no start bit stupiditiy, write out as many 16-bit blocks as we can
			uint8_t *first_end = end - (len % (SPI_ADVANCE * 2));
			
			while(data != first_end) {
				if(WM == NONE) { wait1(); }
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(data[SPI_B0], scale) << 8 | D::adjust(data[SPI_B1], scale));
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(data[SPI_B2], scale) << 8 | D::adjust(data[SPI_ADVANCE + SPI_B0], scale));
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(data[SPI_ADVANCE + SPI_B1], scale) << 8 | D::adjust(data[SPI_ADVANCE + SPI_B2], scale));
				data += (SPI_ADVANCE + SPI_ADVANCE);
			}

			if(data != end) { 
				if(WM == NONE) { wait1(); }
				// write out the rest as alternating 16/8-bit blocks (likely to be just one)
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(data[SPI_B0], scale) << 8 | D::adjust(data[SPI_B1], scale));
				Write<CM, WM, NOTLAST>::writeByte(D::adjust(data[SPI_B2], scale));
			}

			D::postBlock(len);
			waitFully();
		} else if(SKIP & FLAG_START_BIT) {
			uint32_t ctar1_save = SPI0_CTAR1;

			// Clear out the FMSZ bits, reset them for 9 bits transferd for the start bit
			uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(8);
			update_ctar1(ctar1);

			while(data != end) { 
				writeWord( 0x100 | D::adjust(data[SPI_B0], scale));
				writeByte(D::adjust(data[SPI_B1], scale));
				writeByte(D::adjust(data[SPI_B2], scale));
				data += SPI_ADVANCE;
			}
			D::postBlock(len);
			waitFully();

			// restore ctar1
			update_ctar1(ctar1_save);
		// } else {
		// 	while(data != end) { 
		// 		writeByte(D::adjust(data[SPI_B0], scale);
		// 		writeWord(D::adjust(data[SPI_B1], scale) << 8 | D::adjust(data[SPI_B2], scale));
		// 		data += SPI_ADVANCE;
		// 	}
		// 	waitFully();
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

#endif

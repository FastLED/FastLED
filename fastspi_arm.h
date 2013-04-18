#ifndef __INC_FASTSPI_ARM_H
#define __INC_FASTSPI_ARM_H


#if defined(__MK20DX128__) && defined(CORE_TEENSY)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_SPEED>
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

		// Configure CTAR0, defaulting to 8 bits and CTAR1, defaulting to 16 bits
	 	int _PBR = 0;
	 	int _BR = 0;
	 	int _CSSCK = 0;
	 	int _DBR = 1;

	 	if(_SPI_SPEED >= 128) { _PBR = 0; _BR = _CSSCK = 7; _DBR = 0; }      // osc/256
	 	else if(_SPI_SPEED >= 64) { _PBR = 0; _BR = _CSSCK = 6; _DBR = 0;  } // osc/128
	 	else if(_SPI_SPEED >= 32) { _PBR = 0; _BR = _CSSCK = 6; _DBR = 1;  } // osc/64
	 	else if(_SPI_SPEED >= 16) { _PBR = 0; _BR = _CSSCK = 4; _DBR = 0;  } // osc/32
	 	else if(_SPI_SPEED >= 8) { _PBR = 0; _BR = _CSSCK = 4; _DBR = 1;  }  // osc/16
	 	else if(_SPI_SPEED >= 4) { _PBR = 0; _BR = _CSSCK = 1; _DBR = 0;  }  // osc/8
	 	else if(_SPI_SPEED >= 2) { _PBR = 0; _BR = _CSSCK = 0; _DBR = 0;  }  // osc/4
	 	else if(_SPI_SPEED >= 1) { _PBR = 1; _BR = _CSSCK = 0; _DBR = 1;  }  // osc/3
	 	else if(_SPI_SPEED >= 0) { _PBR = 0; _BR = _CSSCK = 0; _DBR = 1; }   // osc/2

	 	// double the clock rate, if necessary 
	 	if(_DBR == 1) { _DBR = SPI_CTAR_DBR; }

	 	uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK) | _DBR;
	 	uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK) | _DBR;
	    update_ctar0(ctar0);
	    update_ctar1(ctar1);

		// Configure SPI as the master and enable 
		SPI0_MCR |= SPI_MCR_MSTR;
		SPI0_MCR &= ~(SPI_MCR_MDIS | SPI_MCR_HALT);

		enable_pins();
	}

	static void waitFully() __attribute__((always_inline)) { 
		while( (SPI0_SR & 0xF000) > 0); 
		while (!(SPI0_SR & SPI_SR_TCF)); 
		SPI0_SR |= SPI_SR_TCF; 
	}
	static void wait() __attribute__((always_inline)) { while( (SPI0_SR & 0xF000) >= 0x4000);  }
	

	static void writeWord(uint16_t w) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI0_PUSHR_CONT | SPI0_PUSHR_CTAS(1) | (w & 0xFFFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPI0_PUSHR = SPI0_PUSHR_CONT | (b & 0xFF); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI0_PUSHR_CONT | (b & 0xFF); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPI0_PUSHR = SPI0_PUSHR_CONT | (b & 0xFF); }

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
	template <uint8_t SKIP, class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		uint8_t *end = data + len;
		select();
		if((SKIP & FLAG_START_BIT) == 0) {
			//If no start bit stupiditiy, write out as many 16-bit blocks as we can
			uint8_t *first_end = end - (len % (SPI_ADVANCE + SPI_ADVANCE));
			while(data != first_end) {
					writeWord(D::adjust(data[SPI_B0], scale) << 8 | D::adjust(data[SPI_B1], scale));
					writeWord(D::adjust(data[SPI_B2], scale) << 8 | D::adjust(data[SPI_ADVANCE + SPI_B0], scale));
					writeWord(D::adjust(data[SPI_ADVANCE + SPI_B1], scale) << 8 | D::adjust(data[SPI_ADVANCE + SPI_B2], scale));
					data += (SPI_ADVANCE + SPI_ADVANCE);
			}

			// write out the rest as alternating 16/8-bit blocks (likely to be just one)
			while(data != end) { 
				writeWord(D::adjust(data[SPI_B0], scale) << 8 | D::adjust(data[SPI_B1], scale));
				writeByte(D::adjust(data[SPI_B2], scale));
				data += SPI_ADVANCE;
			}
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

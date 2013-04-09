#ifndef __INC_FASTSPI_ARM_H
#define __INC_FASTSPI_ARM_H


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
	template <uint8_t SKIP, class D, EOrder RGB_ORDER> void writeBytes3(register uint8_t *data, int len, register uint8_t scale) { 
		uint8_t *end = data + len;
		select();
		if((SKIP & FLAG_START_BIT) == 0) {
			// If no start bit stupiditiy, write out as many 16-bit blocks as we can
			uint8_t *first_end = end - (len % (6 + SPI_ADVANCE + SPI_ADVANCE));
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

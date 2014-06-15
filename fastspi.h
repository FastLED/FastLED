#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

#include "controller.h"
#include "lib8tion.h"
#include "delay.h"

// Some helper macros for getting at mis-ordered byte values
#define SPI_B0 (RGB_BYTE0(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_B1 (RGB_BYTE1(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_B2 (RGB_BYTE2(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
#define SPI_ADVANCE (3 + (MASK_SKIP_BITS & SKIP))

/// Some of the SPI controllers will need to perform a transform on each byte before doing
/// anyting with it.  Creating a class of this form and passing it in as a template parameter to
/// writeBytes/writeBytes3 below will ensure that the body of this method will get called on every
/// byte worked on.  Recommendation, make the adjust method aggressively inlined.
///
/// TODO: Convinience macro for building these
class DATA_NOP {
public:
	static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data) { return data; }
	static __attribute__((always_inline)) inline uint8_t adjust(register uint8_t data, register uint8_t scale) { return scale8(data, scale); }
	static __attribute__((always_inline)) inline void postBlock(int len) {}
};

#define FLAG_START_BIT 0x80
#define MASK_SKIP_BITS 0x3F

// Clock speed dividers
#define SPEED_DIV_2 2
#define SPEED_DIV_4 4
#define SPEED_DIV_8 8
#define SPEED_DIV_16 16
#define SPEED_DIV_32 32
#define SPEED_DIV_64 64
#define SPEED_DIV_128 128

#define MAX_DATA_RATE 0
#if (CLK_DBL == 1)
#define DATA_RATE_MHZ(X) (((F_CPU / 1000000L) / X)/2)
#define DATA_RATE_KHZ(X) (((F_CPU / 1000L) / X)/2)
#else
#define DATA_RATE_MHZ(X) ((F_CPU / 1000000L) / X)
#define DATA_RATE_KHZ(X) ((F_CPU / 1000L) / X)
#endif

// Include the various specific SPI implementations
#include "fastspi_bitbang.h"
#include "fastspi_arm_k20.h"
#include "fastspi_arm_sam.h"
#include "fastspi_avr.h"
#include "fastspi_dma.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports on platforms/builds where the pin
// mappings are known at compile time.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#ifndef FORCE_SOFTWARE_SPI
#if defined(SPI_DATA) && defined(SPI_CLOCK)

#if defined(FASTLED_TEENSY3) && defined(CORE_TEENSY)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, 0x4002C000> {};

#if defined(SPI2_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED, 0x4002C000> {};
#endif

#elif defined(__SAM3X8E__)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public SAMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

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

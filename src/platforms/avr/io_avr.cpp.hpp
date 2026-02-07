#pragma once
// ok no namespace fl
#include "is_avr.h"

#ifdef FL_IS_AVR

// For Arduino AVR builds, use Arduino's I/O implementation
#ifdef ARDUINO
#include "platforms/arduino/io_arduino.hpp"

#else
// For bare-metal AVR builds (no Arduino framework), we would need
// direct UART register access implementations here
// Currently not implemented - Arduino framework is required for AVR I/O
#error "Bare-metal AVR (without Arduino framework) is not currently supported for I/O operations"
#endif

#endif // __AVR__

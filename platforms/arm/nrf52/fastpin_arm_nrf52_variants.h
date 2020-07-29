#ifndef __FASTPIN_ARM_NRF52_VARIANTS_H
#define __FASTPIN_ARM_NRF52_VARIANTS_H

// use this to determine if found variant or not (avoid multiple boards at once)
#undef __FASTPIN_ARM_NRF52_VARIANT_FOUND

// Adafruit Bluefruit nRF52832 Feather
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52832_FEATHER) 
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Adafruit Bluefruit nRF52832 Feather is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _DEFPIN_ARM_IDENTITY_P0( 0); // xtal 1
    _DEFPIN_ARM_IDENTITY_P0( 1); // xtal 2
    _DEFPIN_ARM_IDENTITY_P0( 2); // a0
    _DEFPIN_ARM_IDENTITY_P0( 3); // a1
    _DEFPIN_ARM_IDENTITY_P0( 4); // a2
    _DEFPIN_ARM_IDENTITY_P0( 5); // a3
    _DEFPIN_ARM_IDENTITY_P0( 6); // TXD
    _DEFPIN_ARM_IDENTITY_P0( 7); // GPIO #7
    _DEFPIN_ARM_IDENTITY_P0( 8); // RXD
    _DEFPIN_ARM_IDENTITY_P0( 9); // NFC1
    _DEFPIN_ARM_IDENTITY_P0(10); // NFC2
    _DEFPIN_ARM_IDENTITY_P0(11); // GPIO #11
    _DEFPIN_ARM_IDENTITY_P0(12); // SCK
    _DEFPIN_ARM_IDENTITY_P0(13); // MOSI
    _DEFPIN_ARM_IDENTITY_P0(14); // MISO
    _DEFPIN_ARM_IDENTITY_P0(15); // GPIO #15
    _DEFPIN_ARM_IDENTITY_P0(16); // GPIO #16
    _DEFPIN_ARM_IDENTITY_P0(17); // LED #1 (red)
    _DEFPIN_ARM_IDENTITY_P0(18); // SWO
    _DEFPIN_ARM_IDENTITY_P0(19); // LED #2 (blue)
    _DEFPIN_ARM_IDENTITY_P0(20); // DFU
    // _DEFPIN_ARM_IDENTITY_P0(21); // Reset -- not valid to use for FastLED?
    // _DEFPIN_ARM_IDENTITY_P0(22); // Factory Reset -- not vaild to use for FastLED?
    // _DEFPIN_ARM_IDENTITY_P0(23); // N/A
    // _DEFPIN_ARM_IDENTITY_P0(24); // N/A
    _DEFPIN_ARM_IDENTITY_P0(25); // SDA
    _DEFPIN_ARM_IDENTITY_P0(26); // SCL
    _DEFPIN_ARM_IDENTITY_P0(27); // GPIO #27
    _DEFPIN_ARM_IDENTITY_P0(28); // A4
    _DEFPIN_ARM_IDENTITY_P0(29); // A5
    _DEFPIN_ARM_IDENTITY_P0(30); // A6
    _DEFPIN_ARM_IDENTITY_P0(31); // A7
#endif // defined (ARDUINO_NRF52832_FEATHER) 

// Adafruit Circuit Playground Bluefruit
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_CIRCUITPLAY)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif

    // This board is a bit of a mess ... as it defines
    // multiple arduino pins to map to a single Port/Pin
    // combination.

    // Use PIN_NEOPIXEL (D8) for the ten built-in neopixels
    _FL_DEFPIN( 8, 13, 0); // P0.13 -- D8 / Neopixels

    // PIN_A0 is connect to an amplifier, and thus *might*
    // not be suitable for use with FastLED.
    // Do not enable this pin until can confirm
    // signal integrity from this pin.
    //
    // NOTE: it might also be possible if first disable
    //       the amp using D11 ("speaker shutdown" pin)
    //
    // _FL_DEFPIN(14, 26, 0); // P0.26 -- A0   / D12  / Audio Out
    _FL_DEFPIN(15,  2, 0);    // P0.02 -- A1   /  D6
    _FL_DEFPIN(16, 29, 0);    // P0.29 -- A2   /  D9
    _FL_DEFPIN(17,  3, 0);    // P0.03 -- A3   / D10
    _FL_DEFPIN(18,  4, 0);    // P0.04 -- A4   /  D3  / SCL
    _FL_DEFPIN(19,  5, 0);    // P0.05 -- A5   /  D2  / SDA
    _FL_DEFPIN(20, 30, 0);    // P0.30 -- A6   /  D0  / UART RX
    _FL_DEFPIN(21, 14, 0);    // P0.14 -- AREF /  D1  / UART TX

#endif

// Adafruit Bluefruit nRF52840 Feather Express
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_FEATHER)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif

    // Arduino pins 0..7
    _FL_DEFPIN( 0, 25, 0); // D0  is P0.25 -- UART TX
    //_FL_DEFPIN( 1, 24, 0); // D1  is P0.24 -- UART RX
    _FL_DEFPIN( 2, 10, 0); // D2  is P0.10 -- NFC2
    _FL_DEFPIN( 3, 47, 1); // D3  is P1.15 -- PIN_LED1 (red)
    _FL_DEFPIN( 4, 42, 1); // D4  is P1.10 -- PIN_LED2 (blue)
    _FL_DEFPIN( 5, 40, 1); // D5  is P1.08 -- SPI/SS
    _FL_DEFPIN( 6,  7, 0); // D6  is P0.07
    _FL_DEFPIN( 7, 34, 1); // D7  is P1.02 -- PIN_DFU (Button)
    
    // Arduino pins 8..15
    _FL_DEFPIN( 8, 16, 0); // D8  is P0.16 -- PIN_NEOPIXEL
    _FL_DEFPIN( 9, 26, 0); // D9  is P0.26
    _FL_DEFPIN(10, 27, 0); // D10 is P0.27
    _FL_DEFPIN(11,  6, 0); // D11 is P0.06
    _FL_DEFPIN(12,  8, 0); // D12 is P0.08
    _FL_DEFPIN(13, 41, 1); // D13 is P1.09
    _FL_DEFPIN(14,  4, 0); // D14 is P0.04 -- A0
    _FL_DEFPIN(15,  5, 0); // D15 is P0.05 -- A1

    // Arduino pins 16..23
    _FL_DEFPIN(16, 30, 0); // D16 is P0.30 -- A2
    _FL_DEFPIN(17, 28, 0); // D17 is P0.28 -- A3
    _FL_DEFPIN(18,  2, 0); // D18 is P0.02 -- A4
    _FL_DEFPIN(19,  3, 0); // D19 is P0.03 -- A5
    //_FL_DEFPIN(20, 29, 0); // D20 is P0.29 -- A6 -- Connected to battery!
    //_FL_DEFPIN(21, 31, 0); // D21 is P0.31 -- A7 -- AREF
    _FL_DEFPIN(22, 12, 0); // D22 is P0.12 -- SDA
    _FL_DEFPIN(23, 11, 0); // D23 is P0.11 -- SCL

    // Arduino pins 24..31
    _FL_DEFPIN(24, 15, 0); // D24 is P0.15 -- PIN_SPI_MISO
    _FL_DEFPIN(25, 13, 0); // D25 is P0.13 -- PIN_SPI_MOSI
    _FL_DEFPIN(26, 14, 0); // D26 is P0.14 -- PIN_SPI_SCK
    //_FL_DEFPIN(27, 19, 0); // D27 is P0.19 -- PIN_QSPI_SCK
    //_FL_DEFPIN(28, 20, 0); // D28 is P0.20 -- PIN_QSPI_CS
    //_FL_DEFPIN(29, 17, 0); // D29 is P0.17 -- PIN_QSPI_DATA0
    //_FL_DEFPIN(30, 22, 0); // D30 is P0.22 -- PIN_QSPI_DATA1
    //_FL_DEFPIN(31, 23, 0); // D31 is P0.23 -- PIN_QSPI_DATA2

    // Arduino pins 32..34
    //_FL_DEFPIN(32, 21, 0); // D32 is P0.21 -- PIN_QSPI_DATA3
    //_FL_DEFPIN(33,  9, 0); // D33 is NFC1, only accessible via test point
#endif // defined (ARDUINO_NRF52840_FEATHER)

// Adafruit Bluefruit nRF52840 Metro Express
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_METRO)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Adafruit Bluefruit nRF52840 Metro Express is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 25, 0); // D0  is P0.25 (UART TX)
    _FL_DEFPIN( 1, 24, 0); // D1  is P0.24 (UART RX)
    _FL_DEFPIN( 2, 10, 1); // D2  is P1.10 
    _FL_DEFPIN( 3,  4, 1); // D3  is P1.04 
    _FL_DEFPIN( 4, 11, 1); // D4  is P1.11 
    _FL_DEFPIN( 5, 12, 1); // D5  is P1.12 
    _FL_DEFPIN( 6, 14, 1); // D6  is P1.14
    _FL_DEFPIN( 7, 26, 0); // D7  is P0.26
    _FL_DEFPIN( 8, 27, 0); // D8  is P0.27
    _FL_DEFPIN( 9, 12, 0); // D9  is P0.12
    _FL_DEFPIN(10,  6, 0); // D10 is P0.06 
    _FL_DEFPIN(11,  8, 0); // D11 is P0.08 
    _FL_DEFPIN(12,  9, 1); // D12 is P1.09 
    _FL_DEFPIN(13, 14, 0); // D13 is P0.14 
    _FL_DEFPIN(14,  4, 0); // D14 is P0.04 (A0)
    _FL_DEFPIN(15,  5, 0); // D15 is P0.05 (A1)
    _FL_DEFPIN(16, 28, 0); // D16 is P0.28 (A2)
    _FL_DEFPIN(17, 30, 0); // D17 is P0.30 (A3)
    _FL_DEFPIN(18,  2, 0); // D18 is P0.02 (A4)
    _FL_DEFPIN(19,  3, 0); // D19 is P0.03 (A5)
    _FL_DEFPIN(20, 29, 0); // D20 is P0.29 (A6, battery)
    _FL_DEFPIN(21, 31, 0); // D21 is P0.31 (A7, ARef)
    _FL_DEFPIN(22, 15, 0); // D22 is P0.15 (SDA)
    _FL_DEFPIN(23, 16, 0); // D23 is P0.16 (SCL)
    _FL_DEFPIN(24, 11, 0); // D24 is P0.11 (SPI MISO)
    _FL_DEFPIN(25,  8, 1); // D25 is P1.08 (SPI MOSI)
    _FL_DEFPIN(26,  7, 0); // D26 is P0.07 (SPI SCK )
    //_FL_DEFPIN(27, 19, 0); // D27 is P0.19 (QSPI CLK   )
    //_FL_DEFPIN(28, 20, 0); // D28 is P0.20 (QSPI CS    )
    //_FL_DEFPIN(29, 17, 0); // D29 is P0.17 (QSPI Data 0)
    //_FL_DEFPIN(30, 23, 0); // D30 is P0.23 (QSPI Data 1)
    //_FL_DEFPIN(31, 22, 0); // D31 is P0.22 (QSPI Data 2)
    //_FL_DEFPIN(32, 21, 0); // D32 is P0.21 (QSPI Data 3)
    _FL_DEFPIN(33, 13, 1); // D33 is P1.13 LED1
    _FL_DEFPIN(34, 15, 1); // D34 is P1.15 LED2
    _FL_DEFPIN(35, 13, 0); // D35 is P0.13 NeoPixel
    _FL_DEFPIN(36,  0, 1); // D36 is P1.02 Switch
    _FL_DEFPIN(37,  0, 1); // D37 is P1.00 SWO/DFU
    _FL_DEFPIN(38,  9, 0); // D38 is P0.09 NFC1
    _FL_DEFPIN(39, 10, 0); // D39 is P0.10 NFC2
#endif // defined (ARDUINO_NRF52840_METRO)

// Adafruit Bluefruit on nRF52840DK PCA10056
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52840_PCA10056)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    
    #if defined(USE_ARDUINO_PIN_NUMBERING)
        #error "Define of `USE_ARDUINO_PIN_NUMBERING` has known errors in pin mapping -- select different mapping"
    #elif defined(FASTLED_NRF52_USE_ARDUINO_UNO_R3_HEADER_PIN_NUMBERING)
        /* The following allows defining and using the FastPin<> templates,
           using the Arduino UNO R3 connector pin definitions.
        */
        _FL_DEFPIN( 0,  1, 1); // D0  is P1.01 
        _FL_DEFPIN( 1,  2, 1); // D1  is P1.02 
        _FL_DEFPIN( 2,  3, 1); // D2  is P1.03
        _FL_DEFPIN( 3,  4, 1); // D3  is P1.04 
        _FL_DEFPIN( 4,  5, 1); // D4  is P1.05 
        _FL_DEFPIN( 5,  6, 1); // D5  is P1.06 
        _FL_DEFPIN( 6,  7, 1); // D6  is P1.07 (BUTTON1 option)
        _FL_DEFPIN( 7,  8, 1); // D7  is P1.08 (BUTTON2 option)
        _FL_DEFPIN( 8, 10, 1); // D8  is P1.10 
        _FL_DEFPIN( 9, 11, 1); // D9  is P1.11 
        _FL_DEFPIN(10, 12, 1); // D10 is P1.12 
        _FL_DEFPIN(11, 13, 1); // D11 is P1.13 
        _FL_DEFPIN(12, 14, 1); // D12 is P1.14
        _FL_DEFPIN(13, 15, 1); // D13 is P1.15 
        // Arduino UNO uses pins D14..D19 to map to header pins A0..A5
        // AREF has no equivalent digital pin map on Arduino, would be P0.02
        _FL_DEFPIN(14,  3, 0); // D14 / A0 is P0.03
        _FL_DEFPIN(15,  4, 0); // D15 / A1 is P0.04
        _FL_DEFPIN(16, 28, 0); // D16 / A2 is P0.28
        _FL_DEFPIN(17, 29, 0); // D17 / A3 is P0.29
        // Cannot determine which pin on PCA10056 would be intended solely from UNO R3 digital pin number
        //_FL_DEFPIN(18, 30, 0); // D18 could be one of two pins: A4 would be P0.30, SDA would be P0.26
        //_FL_DEFPIN(19, 31, 0); // D19 could be one of two pins: A5 would be P0.31, SCL would be P0.27
    #elif defined(FASTLED_NRF52_USE_ARDUINO_MEGA_2560_REV3_HEADER_PIN_NUMBERING)
        /* The following allows defining and using the FastPin<> templates,
           using the Arduino UNO R3 connector pin definitions.
        */
        _FL_DEFPIN( 0,  1, 1); // D0  is P1.01
        _FL_DEFPIN( 1,  2, 1); // D1  is P1.02
        _FL_DEFPIN( 2,  3, 1); // D2  is P1.03
        _FL_DEFPIN( 3,  4, 1); // D3  is P1.04
        _FL_DEFPIN( 4,  5, 1); // D4  is P1.05
        _FL_DEFPIN( 5,  6, 1); // D5  is P1.06
        _FL_DEFPIN( 6,  7, 1); // D6  is P1.07 (BUTTON1 option)
        _FL_DEFPIN( 7,  8, 1); // D7  is P1.08 (BUTTON2 option)
        _FL_DEFPIN( 8, 10, 1); // D8  is P1.10
        _FL_DEFPIN( 9, 11, 1); // D9  is P1.11
        _FL_DEFPIN(10, 12, 1); // D10 is P1.12
        _FL_DEFPIN(11, 13, 1); // D11 is P1.13
        _FL_DEFPIN(12, 14, 1); // D12 is P1.14
        _FL_DEFPIN(13, 15, 1); // D13 is P1.15

        // Arduino MEGA 2560 has additional digital pins on lower digital header
        _FL_DEFPIN(14, 10, 0); // D14 is P0.10
        _FL_DEFPIN(15,  9, 0); // D15 is P0.09
        _FL_DEFPIN(16,  8, 0); // D16 is P0.08
        _FL_DEFPIN(17,  7, 0); // D17 is P0.07
        _FL_DEFPIN(18,  6, 0); // D14 is P0.06
        _FL_DEFPIN(19,  5, 0); // D15 is P0.05
        // Cannot determine which pin on PCA10056 would be intended solely from UNO MEGA 2560 digital pin number
        //_FL_DEFPIN(20,  1, 0); // D20 could be one of two pins: D20 on lower header would be P0.01, SDA would be P0.26
        //_FL_DEFPIN(21,  0, 0); // D21 could be one of two pins: D21 on lower header would be P0.00, SCL would be P0.27

        // Arduino MEGA 2560 has D22-D53 exposed on perpendicular two-row header
        // PCA10056 has support for D22-D38 via a 2x19 header at that location (D39 is GND on PCA10056)
        _FL_DEFPIN(22, 11, 0); // D22 is P0.11
        _FL_DEFPIN(23, 12, 0); // D23 is P0.12
        _FL_DEFPIN(24, 13, 0); // D24 is P0.13
        _FL_DEFPIN(25, 14, 0); // D25 is P0.14
        _FL_DEFPIN(26, 15, 0); // D26 is P0.15
        _FL_DEFPIN(27, 16, 0); // D27 is P0.16
        // _FL_DEFPIN(28, 17, 0); // D28 is P0.17 (QSPI !CS )
        // _FL_DEFPIN(29, 18, 0); // D29 is P0.18 (RESET)
        // _FL_DEFPIN(30, 19, 0); // D30 is P0.19 (QSPI CLK)
        // _FL_DEFPIN(31, 20, 0); // D31 is P0.20 (QSPI DIO0)
        // _FL_DEFPIN(32, 21, 0); // D32 is P0.21 (QSPI DIO1)
        // _FL_DEFPIN(33, 22, 0); // D33 is P0.22 (QSPI DIO2)
        // _FL_DEFPIN(34, 23, 0); // D34 is P0.23 (QSPI DIO3)
        _FL_DEFPIN(35, 24, 0); // D35 is P0.24
        _FL_DEFPIN(36, 25, 0); // D36 is P0.25
        _FL_DEFPIN(37,  0, 1); // D37 is P1.00
        _FL_DEFPIN(38,  9, 1); // D38 is P1.09
        // _FL_DEFPIN(39, , 0); // D39 is P0.


        // Arduino MEGA 2560 uses pins D54..D59 to map to header pins A0..A5
        // (it also has D60..D69 for A6..A15, which have no corresponding header on PCA10056)
        // AREF has no equivalent digital pin map on Arduino, would be P0.02
        _FL_DEFPIN(54,  3, 0); // D54 / A0 is P0.03
        _FL_DEFPIN(55,  4, 0); // D55 / A1 is P0.04
        _FL_DEFPIN(56, 28, 0); // D56 / A2 is P0.28
        _FL_DEFPIN(57, 29, 0); // D57 / A3 is P0.29
        _FL_DEFPIN(58, 30, 0); // D58 / A4 is P0.30
        _FL_DEFPIN(59, 31, 0); // D59 / A5 is P0.31

    #else // identity mapping of arduino pin to port/pin
        /* 48 pins, defined using natural mapping in Adafruit's variant.cpp (!) */
        _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00 (XL1 .. ensure SB4 bridged, SB2 cut)
        _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01 (XL2 .. ensure SB3 bridged, SB1 cut)
        _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (AIN0)
        _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (AIN1)
        _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (AIN2 / UART CTS option)
        _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (AIN3 / UART RTS)
        _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06 (UART TxD)
        _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07 (TRACECLK / UART CTS default)
        _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08 (UART RxD)
        _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09 (NFC1)
        _DEFPIN_ARM_IDENTITY_P0(10); // P0.10 (NFC2)
        _DEFPIN_ARM_IDENTITY_P0(11); // P0.11 (TRACEDATA2 / BUTTON1 default)
        _DEFPIN_ARM_IDENTITY_P0(12); // P0.12 (TRACEDATA1 / BUTTON2 default)
        _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (LED1)
        _DEFPIN_ARM_IDENTITY_P0(14); // P0.14 (LED2)
        _DEFPIN_ARM_IDENTITY_P0(15); // P0.15 (LED3)
        _DEFPIN_ARM_IDENTITY_P0(16); // P0.16 (LED4)
        //_DEFPIN_ARM_IDENTITY_P0(17); // P0.17 (QSPI !CS )
        //_DEFPIN_ARM_IDENTITY_P0(18); // P0.18 (RESET)
        //_DEFPIN_ARM_IDENTITY_P0(19); // P0.19 (QSPI CLK )
        //_DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (QSPI DIO0)
        //_DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (QSPI DIO1)
        //_DEFPIN_ARM_IDENTITY_P0(22); // P0.22 (QSPI DIO2)
        //_DEFPIN_ARM_IDENTITY_P0(23); // P0.23 (QSPI DIO3)
        _DEFPIN_ARM_IDENTITY_P0(24); // P0.24 (BUTTON3)
        _DEFPIN_ARM_IDENTITY_P0(25); // P0.25 (BUTTON4)
        _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
        _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
        _DEFPIN_ARM_IDENTITY_P0(28); // P0.28 (AIN4)
        _DEFPIN_ARM_IDENTITY_P0(29); // P0.29 (AIN5)
        _DEFPIN_ARM_IDENTITY_P0(30); // P0.30 (AIN6)
        _DEFPIN_ARM_IDENTITY_P0(31); // P0.31 (AIN7)
        _DEFPIN_ARM_IDENTITY_P0(32); // P1.00 (SWO / TRACEDATA0)
        _DEFPIN_ARM_IDENTITY_P0(33); // P1.01 
        _DEFPIN_ARM_IDENTITY_P0(34); // P1.02
        _DEFPIN_ARM_IDENTITY_P0(35); // P1.03
        _DEFPIN_ARM_IDENTITY_P0(36); // P1.04
        _DEFPIN_ARM_IDENTITY_P0(37); // P1.05
        _DEFPIN_ARM_IDENTITY_P0(38); // P1.06
        _DEFPIN_ARM_IDENTITY_P0(39); // P1.07 (BUTTON1 option)
        _DEFPIN_ARM_IDENTITY_P0(40); // P1.08 (BUTTON2 option)
        _DEFPIN_ARM_IDENTITY_P0(41); // P1.09 (TRACEDATA3)
        _DEFPIN_ARM_IDENTITY_P0(42); // P1.10
        _DEFPIN_ARM_IDENTITY_P0(43); // P1.11
        _DEFPIN_ARM_IDENTITY_P0(44); // P1.12
        _DEFPIN_ARM_IDENTITY_P0(45); // P1.13
        _DEFPIN_ARM_IDENTITY_P0(46); // P1.14
        _DEFPIN_ARM_IDENTITY_P0(47); // P1.15
    #endif
#endif // defined (ARDUINO_NRF52840_PCA10056)

// Adafruit ItsyBitsy nRF52840 Express
// From https://www.adafruit.com/package_adafruit_index.json
#if defined (ARDUINO_NRF52_ITSYBITSY)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Adafruit ItsyBitsy nRF52840 Express is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif

    //  [D0 .. D13] (digital)
    _FL_DEFPIN( 0, 25, 0); // D0  is P0.25 (UART RX)
    _FL_DEFPIN( 1, 24, 0); // D1  is P0.24 (UART TX)
    _FL_DEFPIN( 2,  2, 1); // D2  is P1.02
    _FL_DEFPIN( 3,  6, 0); // D3  is P0.06 LED
    _FL_DEFPIN( 4, 29, 0); // D4  is P0.29 Button
    _FL_DEFPIN( 5, 27, 0); // D5  is P0.27
    _FL_DEFPIN( 6,  9, 1); // D6  is P1.09 (DotStar Clock)
    _FL_DEFPIN( 7,  8, 1); // D7  is P1.08
    _FL_DEFPIN( 8,  8, 0); // D8  is P0.08 (DotStar Data)
    _FL_DEFPIN( 9,  7, 0); // D9  is P0.07
    _FL_DEFPIN(10,  5, 0); // D10 is P0.05
    _FL_DEFPIN(11, 26, 0); // D11 is P0.26
    _FL_DEFPIN(12, 11, 0); // D12 is P0.11
    _FL_DEFPIN(13, 12, 0); // D13 is P0.12

    //  [D14 .. D20] (analog [A0 .. A6])
    _FL_DEFPIN(14,  4, 0); // D14 is P0.04 (A0)
    _FL_DEFPIN(15, 30, 0); // D15 is P0.30 (A1)
    _FL_DEFPIN(16, 28, 0); // D16 is P0.28 (A2)
    _FL_DEFPIN(17, 31, 0); // D17 is P0.31 (A3)
    _FL_DEFPIN(18,  2, 0); // D18 is P0.02 (A4)
    _FL_DEFPIN(19,  3, 0); // D19 is P0.03 (A5)
    _FL_DEFPIN(20,  5, 0); // D20 is P0.05 (A6/D10)

    //  [D21 .. D22] (I2C)
    _FL_DEFPIN(21, 16, 0); // D21 is P0.16 (SDA)
    _FL_DEFPIN(22, 14, 0); // D22 is P0.14 (SCL)

    //  [D23 .. D25] (SPI)
    _FL_DEFPIN(23, 20, 0); // D23 is P0.20 (SPI MISO)
    _FL_DEFPIN(24, 15, 0); // D24 is P0.15 (SPI MOSI)
    _FL_DEFPIN(25, 13, 0); // D25 is P0.13 (SPI SCK )

    //  [D26 .. D31] (QSPI)
    _FL_DEFPIN(26, 19, 0); // D26 is P0.19 (QSPI CLK)
    _FL_DEFPIN(27, 23, 0); // D27 is P0.23 (QSPI CS)
    _FL_DEFPIN(28, 21, 0); // D28 is P0.21 (QSPI Data 0)
    _FL_DEFPIN(29, 22, 0); // D29 is P0.22 (QSPI Data 1)
    _FL_DEFPIN(30,  0, 1); // D30 is P1.00 (QSPI Data 2)
    _FL_DEFPIN(31, 17, 0); // D31 is P0.17 (QSPI Data 3)

#endif // defined (ARDUINO_NRF52_ITSYBITSY)

// Electronut labs bluey
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/bluey/variant.cpp
#if defined(ARDUINO_ELECTRONUT_BLUEY)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Electronut labs bluey is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 26, 0); // D0  is P0.26
    _FL_DEFPIN( 1, 27, 0); // D1  is P0.27
    _FL_DEFPIN( 2, 22, 0); // D2  is P0.22 (SPI SS  )
    _FL_DEFPIN( 3, 23, 0); // D3  is P0.23 (SPI MOSI)
    _FL_DEFPIN( 4, 24, 0); // D4  is P0.24 (SPI MISO, also A3)
    _FL_DEFPIN( 5, 25, 0); // D5  is P0.25 (SPI SCK )
    _FL_DEFPIN( 6, 16, 0); // D6  is P0.16 (Button)
    _FL_DEFPIN( 7, 19, 0); // D7  is P0.19 (R)
    _FL_DEFPIN( 8, 18, 0); // D8  is P0.18 (G)
    _FL_DEFPIN( 9, 17, 0); // D9  is P0.17 (B)
    _FL_DEFPIN(10, 11, 0); // D10 is P0.11 (SCL)
    _FL_DEFPIN(11, 12, 0); // D11 is P0.12 (DRDYn)
    _FL_DEFPIN(12, 13, 0); // D12 is P0.13 (SDA)
    _FL_DEFPIN(13, 14, 0); // D13 is P0.17 (INT)
    _FL_DEFPIN(14, 15, 0); // D14 is P0.15 (INT1)
    _FL_DEFPIN(15, 20, 0); // D15 is P0.20 (INT2)
    _FL_DEFPIN(16,  2, 0); // D16 is P0.02 (A0)
    _FL_DEFPIN(17,  3, 0); // D17 is P0.03 (A1)
    _FL_DEFPIN(18,  4, 0); // D18 is P0.04 (A2)
    _FL_DEFPIN(19, 24, 0); // D19 is P0.24 (A3, also D4/SPI MISO) -- is this right?
    _FL_DEFPIN(20, 29, 0); // D20 is P0.29 (A4)
    _FL_DEFPIN(21, 30, 0); // D21 is P0.30 (A5)
    _FL_DEFPIN(22, 31, 0); // D22 is P0.31 (A6)
    _FL_DEFPIN(23,  8, 0); // D23 is P0.08 (RX)
    _FL_DEFPIN(24,  6, 0); // D24 is P0.06 (TX)
    _FL_DEFPIN(25,  5, 0); // D25 is P0.05 (RTS)
    _FL_DEFPIN(26,  7, 0); // D26 is P0.07 (CTS)
#endif // defined(ARDUINO_ELECTRONUT_BLUEY)

// Electronut labs hackaBLE
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/hackaBLE/variant.cpp
#if defined(ARDUINO_ELECTRONUT_HACKABLE)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Electronut labs hackaBLE is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 14, 0); // D0  is P0.14 (RX)
    _FL_DEFPIN( 1, 13, 0); // D1  is P0.13 (TX)
    _FL_DEFPIN( 2, 12, 0); // D2  is P0.12
    _FL_DEFPIN( 3, 11, 0); // D3  is P0.11 (SPI MOSI)
    _FL_DEFPIN( 4,  8, 0); // D4  is P0.08 (SPI MISO)
    _FL_DEFPIN( 5,  7, 0); // D5  is P0.07 (SPI SCK )
    _FL_DEFPIN( 6,  6, 0); // D6  is P0.06
    _FL_DEFPIN( 7, 27, 0); // D7  is P0.27
    _FL_DEFPIN( 8, 26, 0); // D8  is P0.26
    _FL_DEFPIN( 9, 25, 0); // D9  is P0.25
    _FL_DEFPIN(10,  5, 0); // D10 is P0.05 (A3)
    _FL_DEFPIN(11,  4, 0); // D11 is P0.04 (A2)
    _FL_DEFPIN(12,  3, 0); // D12 is P0.03 (A1)
    _FL_DEFPIN(13,  2, 0); // D13 is P0.02 (A0 / AREF)
    _FL_DEFPIN(14, 23, 0); // D14 is P0.23
    _FL_DEFPIN(15, 22, 0); // D15 is P0.22
    _FL_DEFPIN(16, 18, 0); // D16 is P0.18
    _FL_DEFPIN(17, 16, 0); // D17 is P0.16
    _FL_DEFPIN(18, 15, 0); // D18 is P0.15
    _FL_DEFPIN(19, 24, 0); // D19 is P0.24
    _FL_DEFPIN(20, 28, 0); // D20 is P0.28 (A4)
    _FL_DEFPIN(21, 29, 0); // D21 is P0.29 (A5)
    _FL_DEFPIN(22, 30, 0); // D22 is P0.30 (A6)
    _FL_DEFPIN(23, 31, 0); // D23 is P0.31 (A7)
    _FL_DEFPIN(24, 19, 0); // D24 is P0.19 (RED LED)
    _FL_DEFPIN(25, 20, 0); // D25 is P0.20 (GREEN LED)
    _FL_DEFPIN(26, 17, 0); // D26 is P0.17 (BLUE LED)
#endif // defined(ARDUINO_ELECTRONUT_HACKABLE)

// Electronut labs hackaBLE_v2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/hackaBLE_v2/variant.cpp
// (32 pins, natural mapping)
#if defined(ARDUINO_ELECTRONUT_hackaBLE_v2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Electronut labs hackaBLE_v2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00
    _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01
    _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (A0 / SDA / AREF)
    _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (A1 / SCL )
    _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (A2)
    _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (A3)
    _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06
    _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07 (RX)
    _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08 (TX)
    _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09
    _DEFPIN_ARM_IDENTITY_P0(10); // P0.10
    _DEFPIN_ARM_IDENTITY_P0(11); // P0.11 (SPI MISO)
    _DEFPIN_ARM_IDENTITY_P0(12); // P0.12 (SPI MOSI)
    _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (SPI SCK )
    _DEFPIN_ARM_IDENTITY_P0(14); // P0.14 (SPI SS  )
    _DEFPIN_ARM_IDENTITY_P0(15); // P0.15
    _DEFPIN_ARM_IDENTITY_P0(16); // P0.16
    _DEFPIN_ARM_IDENTITY_P0(17); // P0.17 (BLUE LED)
    _DEFPIN_ARM_IDENTITY_P0(18); // P0.18
    _DEFPIN_ARM_IDENTITY_P0(19); // P0.19 (RED LED)
    _DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (GREEN LED)
    // _DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (RESET)
    _DEFPIN_ARM_IDENTITY_P0(22); // P0.22
    _DEFPIN_ARM_IDENTITY_P0(23); // P0.23
    _DEFPIN_ARM_IDENTITY_P0(24); // P0.24
    _DEFPIN_ARM_IDENTITY_P0(25); // P0.25
    _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
    _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
    _DEFPIN_ARM_IDENTITY_P0(28); // P0.28 (A4)
    _DEFPIN_ARM_IDENTITY_P0(29); // P0.29 (A5)
    _DEFPIN_ARM_IDENTITY_P0(30); // P0.30 (A6)
    _DEFPIN_ARM_IDENTITY_P0(31); // P0.31 (A7)
#endif // defined(ARDUINO_ELECTRONUT_hackaBLE_v2)

// RedBear Blend 2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/RedBear_Blend2/variant.cpp
#if defined(ARDUINO_RB_BLEND_2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "RedBear Blend 2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 11, 0); // D0  is P0.11
    _FL_DEFPIN( 1, 12, 0); // D1  is P0.12
    _FL_DEFPIN( 2, 13, 0); // D2  is P0.13
    _FL_DEFPIN( 3, 14, 0); // D3  is P0.14
    _FL_DEFPIN( 4, 15, 0); // D4  is P0.15
    _FL_DEFPIN( 5, 16, 0); // D5  is P0.16
    _FL_DEFPIN( 6, 17, 0); // D6  is P0.17
    _FL_DEFPIN( 7, 18, 0); // D7  is P0.18
    _FL_DEFPIN( 8, 19, 0); // D8  is P0.19
    _FL_DEFPIN( 9, 20, 0); // D9  is P0.20
    _FL_DEFPIN(10, 22, 0); // D10 is P0.22 (SPI SS  )
    _FL_DEFPIN(11, 23, 0); // D11 is P0.23 (SPI MOSI)
    _FL_DEFPIN(12, 24, 0); // D12 is P0.24 (SPI MISO)
    _FL_DEFPIN(13, 25, 0); // D13 is P0.25 (SPI SCK / LED)
    _FL_DEFPIN(14,  3, 0); // D14 is P0.03 (A0)
    _FL_DEFPIN(15,  4, 0); // D15 is P0.04 (A1)
    _FL_DEFPIN(16, 28, 0); // D16 is P0.28 (A2)
    _FL_DEFPIN(17, 29, 0); // D17 is P0.29 (A3)
    _FL_DEFPIN(18, 30, 0); // D18 is P0.30 (A4)
    _FL_DEFPIN(19, 31, 0); // D19 is P0.31 (A5)
    _FL_DEFPIN(20, 26, 0); // D20 is P0.26 (SDA)
    _FL_DEFPIN(21, 27, 0); // D21 is P0.27 (SCL)
    _FL_DEFPIN(22,  8, 0); // D22 is P0.08 (RX)
    _FL_DEFPIN(23,  6, 0); // D23 is P0.06 (TX)
    _FL_DEFPIN(24,  2, 0); // D24 is P0.02 (AREF)
#endif // defined(ARDUINO_RB_BLEND_2)

// RedBear BLE Nano 2
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/RedBear_BLENano2/variant.cpp
#if defined(ARDUINO_RB_BLE_NANO_2)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "RedBear BLE Nano 2 is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 30, 0); // D0  is P0.30 (A0 / RX)
    _FL_DEFPIN( 1, 29, 0); // D1  is P0.29 (A1 / TX)
    _FL_DEFPIN( 2, 28, 0); // D2  is P0.28 (A2 / SDA)
    _FL_DEFPIN( 3,  2, 0); // D3  is P0.02 (A3 / SCL)
    _FL_DEFPIN( 4,  5, 0); // D4  is P0.05 (A4)
    _FL_DEFPIN( 5,  4, 0); // D5  is P0.04 (A5)
    _FL_DEFPIN( 6,  3, 0); // D6  is P0.03 (SPI SS  )
    _FL_DEFPIN( 7,  6, 0); // D7  is P0.06 (SPI MOSI)
    _FL_DEFPIN( 8,  7, 0); // D8  is P0.07 (SPI MISO)
    _FL_DEFPIN( 9,  8, 0); // D9  is P0.08 (SPI SCK )
    // _FL_DEFPIN(10, 21, 0); // D10 is P0.21 (RESET)
    _FL_DEFPIN(13, 11, 0); // D11 is P0.11 (LED)
#endif // defined(ARDUINO_RB_BLE_NANO_2)

// Nordic Semiconductor nRF52 DK
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/nRF52DK/variant.cpp
#if defined(ARDUINO_NRF52_DK)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Nordic Semiconductor nRF52 DK is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    _FL_DEFPIN( 0, 11, 0); // D0  is P0.11
    _FL_DEFPIN( 1, 12, 0); // D1  is P0.12
    _FL_DEFPIN( 2, 13, 0); // D2  is P0.13 (BUTTON1)
    _FL_DEFPIN( 3, 14, 0); // D3  is P0.14 (BUTTON2)
    _FL_DEFPIN( 4, 15, 0); // D4  is P0.15 (BUTTON3)
    _FL_DEFPIN( 5, 16, 0); // D5  is P0.16 (BUTTON4)
    _FL_DEFPIN( 6, 17, 0); // D6  is P0.17 (LED1)
    _FL_DEFPIN( 7, 18, 0); // D7  is P0.18 (LED2)
    _FL_DEFPIN( 8, 19, 0); // D8  is P0.19 (LED3)
    _FL_DEFPIN( 9, 20, 0); // D9  is P0.20 (LED4)
    _FL_DEFPIN(10, 22, 0); // D10 is P0.22 (SPI SS  )
    _FL_DEFPIN(11, 23, 0); // D11 is P0.23 (SPI MOSI)
    _FL_DEFPIN(12, 24, 0); // D12 is P0.24 (SPI MISO)
    _FL_DEFPIN(13, 25, 0); // D13 is P0.25 (SPI SCK / LED)
    _FL_DEFPIN(14,  3, 0); // D14 is P0.03 (A0)
    _FL_DEFPIN(15,  4, 0); // D15 is P0.04 (A1)
    _FL_DEFPIN(16, 28, 0); // D16 is P0.28 (A2)
    _FL_DEFPIN(17, 29, 0); // D17 is P0.29 (A3)
    _FL_DEFPIN(18, 30, 0); // D18 is P0.30 (A4)
    _FL_DEFPIN(19, 31, 0); // D19 is P0.31 (A5)
    _FL_DEFPIN(20,  5, 0); // D20 is P0.05 (A6)
    _FL_DEFPIN(21,  2, 0); // D21 is P0.02 (A7 / AREF)
    _FL_DEFPIN(22, 26, 0); // D22 is P0.26 (SDA)
    _FL_DEFPIN(23, 27, 0); // D23 is P0.27 (SCL)
    _FL_DEFPIN(24,  8, 0); // D24 is P0.08 (RX)
    _FL_DEFPIN(25,  6, 0); // D25 is P0.06 (TX)
#endif // defined(ARDUINO_NRF52_DK)

// Taida Century nRF52 mini board
// https://github.com/sandeepmistry/arduino-nRF5/blob/master/variants/Taida_Century_nRF52_minidev/variant.cpp
#if defined(ARDUINO_STCT_NRF52_minidev)
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Taida Century nRF52 mini board is an untested board -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif
    //_FL_DEFPIN( 0, 25, 0); // D0  is P0.xx (near radio!)
    //_FL_DEFPIN( 1, 26, 0); // D1  is P0.xx (near radio!)
    //_FL_DEFPIN( 2, 27, 0); // D2  is P0.xx (near radio!)
    //_FL_DEFPIN( 3, 28, 0); // D3  is P0.xx (near radio!)
    //_FL_DEFPIN( 4, 29, 0); // D4  is P0.xx (Not connected, near radio!)
    //_FL_DEFPIN( 5, 30, 0); // D5  is P0.xx (LED1, near radio!)
    //_FL_DEFPIN( 6, 31, 0); // D6  is P0.xx (LED2, near radio!)
    _FL_DEFPIN( 7,  2, 0); // D7  is P0.xx (SDA)
    _FL_DEFPIN( 8,  3, 0); // D8  is P0.xx (SCL)
    _FL_DEFPIN( 9,  4, 0); // D9  is P0.xx (BUTTON1 / NFC1)
    _FL_DEFPIN(10,  5, 0); // D10 is P0.xx
    //_FL_DEFPIN(11,  0, 0); // D11 is P0.xx (Not connected)
    //_FL_DEFPIN(12,  1, 0); // D12 is P0.xx (Not connected)
    _FL_DEFPIN(13,  6, 0); // D13 is P0.xx
    _FL_DEFPIN(14,  7, 0); // D14 is P0.xx
    _FL_DEFPIN(15,  8, 0); // D15 is P0.xx
    //_FL_DEFPIN(16,  9, 0); // D16 is P0.xx (Not connected)
    //_FL_DEFPIN(17, 10, 0); // D17 is P0.xx (NFC2, Not connected)
    _FL_DEFPIN(18, 11, 0); // D18 is P0.xx (RXD)
    _FL_DEFPIN(19, 12, 0); // D19 is P0.xx (TXD)
    _FL_DEFPIN(20, 13, 0); // D20 is P0.xx (SPI SS  )
    _FL_DEFPIN(21, 14, 0); // D21 is P0.xx (SPI MISO)
    _FL_DEFPIN(22, 15, 0); // D22 is P0.xx (SPI MOSI)
    _FL_DEFPIN(23, 16, 0); // D23 is P0.xx (SPI SCK )
    _FL_DEFPIN(24, 17, 0); // D24 is P0.xx (A0)
    _FL_DEFPIN(25, 18, 0); // D25 is P0.xx (A1)
    _FL_DEFPIN(26, 19, 0); // D26 is P0.xx (A2)
    _FL_DEFPIN(27, 20, 0); // D27 is P0.xx (A3)
    //_FL_DEFPIN(28, 22, 0); // D28 is P0.xx (A4, near radio!)
    //_FL_DEFPIN(29, 23, 0); // D29 is P0.xx (A5, near radio!)
    _FL_DEFPIN(30, 24, 0); // D30 is P0.xx
    // _FL_DEFPIN(31, 21, 0); // D31 is P0.21 (RESET)
#endif // defined(ARDUINO_STCT_NRF52_minidev)

// Generic nRF52832
// See https://github.com/sandeepmistry/arduino-nRF5/blob/master/boards.txt
#if defined(ARDUINO_GENERIC) && (  defined(NRF52832_XXAA) || defined(NRF52832_XXAB)  )
    #if defined(__FASTPIN_ARM_NRF52_VARIANT_FOUND)
        #error "Cannot define more than one board at a time"
    #else
        #define __FASTPIN_ARM_NRF52_VARIANT_FOUND
    #endif
    #if !defined(FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING)
        #warning "Using `generic` NRF52832 board is an untested configuration -- test and let use know your results via https://github.com/FastLED/FastLED/issues"
    #endif

    _DEFPIN_ARM_IDENTITY_P0( 0); // P0.00 (    UART RX
    _DEFPIN_ARM_IDENTITY_P0( 1); // P0.01 (A0, UART TX)
    _DEFPIN_ARM_IDENTITY_P0( 2); // P0.02 (A1)
    _DEFPIN_ARM_IDENTITY_P0( 3); // P0.03 (A2)
    _DEFPIN_ARM_IDENTITY_P0( 4); // P0.04 (A3)
    _DEFPIN_ARM_IDENTITY_P0( 5); // P0.05 (A4)
    _DEFPIN_ARM_IDENTITY_P0( 6); // P0.06 (A5)
    _DEFPIN_ARM_IDENTITY_P0( 7); // P0.07
    _DEFPIN_ARM_IDENTITY_P0( 8); // P0.08
    _DEFPIN_ARM_IDENTITY_P0( 9); // P0.09
    _DEFPIN_ARM_IDENTITY_P0(10); // P0.10
    _DEFPIN_ARM_IDENTITY_P0(11); // P0.11
    _DEFPIN_ARM_IDENTITY_P0(12); // P0.12
    _DEFPIN_ARM_IDENTITY_P0(13); // P0.13 (LED)
    _DEFPIN_ARM_IDENTITY_P0(14); // P0.14
    _DEFPIN_ARM_IDENTITY_P0(15); // P0.15
    _DEFPIN_ARM_IDENTITY_P0(16); // P0.16
    _DEFPIN_ARM_IDENTITY_P0(17); // P0.17
    _DEFPIN_ARM_IDENTITY_P0(18); // P0.18
    _DEFPIN_ARM_IDENTITY_P0(19); // P0.19
    _DEFPIN_ARM_IDENTITY_P0(20); // P0.20 (I2C SDA)
    _DEFPIN_ARM_IDENTITY_P0(21); // P0.21 (I2C SCL)
    _DEFPIN_ARM_IDENTITY_P0(22); // P0.22 (SPI MISO)
    _DEFPIN_ARM_IDENTITY_P0(23); // P0.23 (SPI MOSI)
    _DEFPIN_ARM_IDENTITY_P0(24); // P0.24 (SPI SCK )
    _DEFPIN_ARM_IDENTITY_P0(25); // P0.25 (SPI SS  )
    _DEFPIN_ARM_IDENTITY_P0(26); // P0.26
    _DEFPIN_ARM_IDENTITY_P0(27); // P0.27
    _DEFPIN_ARM_IDENTITY_P0(28); // P0.28
    _DEFPIN_ARM_IDENTITY_P0(29); // P0.29
    _DEFPIN_ARM_IDENTITY_P0(30); // P0.30
    _DEFPIN_ARM_IDENTITY_P0(31); // P0.31
#endif // defined(ARDUINO_GENERIC)


#endif // __FASTPIN_ARM_NRF52_VARIANTS_H

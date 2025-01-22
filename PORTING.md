Platform Porting Guide
==========================

# Fast porting for a new board on existing hardware

Sometimes "porting" FastLED simply consists of supplying new pin definitions for the given platform.  For example, platforms/avr/fastpin_avr.h contains various pin definitions for all the AVR variant chipsets/boards that FastLED supports.  Defining a set of pins involves setting up a set of definitions - for example here's one full set from the avr fastpin file:

```
#elif defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644P__)

_FL_IO(A); _FL_IO(B); _FL_IO(C); _FL_IO(D);

#define MAX_PIN 31
_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 4, B); _FL_DEFPIN(5, 5, B); _FL_DEFPIN(6, 6, B); _FL_DEFPIN(7, 7, B);
_FL_DEFPIN(8, 0, D); _FL_DEFPIN(9, 1, D); _FL_DEFPIN(10, 2, D); _FL_DEFPIN(11, 3, D);
_FL_DEFPIN(12, 4, D); _FL_DEFPIN(13, 5, D); _FL_DEFPIN(14, 6, D); _FL_DEFPIN(15, 7, D);
_FL_DEFPIN(16, 0, C); _FL_DEFPIN(17, 1, C); _FL_DEFPIN(18, 2, C); _FL_DEFPIN(19, 3, C);
_FL_DEFPIN(20, 4, C); _FL_DEFPIN(21, 5, C); _FL_DEFPIN(22, 6, C); _FL_DEFPIN(23, 7, C);
_FL_DEFPIN(24, 0, A); _FL_DEFPIN(25, 1, A); _FL_DEFPIN(26, 2, A); _FL_DEFPIN(27, 3, A);
_FL_DEFPIN(28, 4, A); _FL_DEFPIN(29, 5, A); _FL_DEFPIN(30, 6, A); _FL_DEFPIN(31, 7, A);

#define HAS_HARDWARE_PIN_SUPPORT 1
```

The ```_FL_IO``` macro is used to define the port registers for the platform while the ```_FL_DEFPIN``` macro is used to define pins.  The parameters to the macro are the pin number, the bit on the port that represents that pin, and the port identifier itself.  On some platforms, like the AVR, ports are identified by letter.  On other platforms, like arm, ports are identified by number.

The ```HAS_HARDWARE_PIN_SUPPORT``` define tells the rest of the FastLED library that there is hardware pin support available.  There may be other platform specific defines for things like hardware SPI ports and such.

## Setting up the basic files/folders

* Create platform directory (e.g. platforms/arm/kl26)
* Create configuration header led_sysdefs_arm_kl26.h:
  * Define platform flags (like FASTLED_ARM/FASTLED_TEENSY)
  * Define configuration parameters re: interrupts, or clock doubling
  * Include extar system header files if needed
* Create main platform include, fastled_arm_kl26.h
  * Include the various other header files as needed
* Modify led_sysdefs.h to conditionally include platform sysdefs header file
* Modify platforms.h to conditionally include platform fastled header

## Porting fastpin.h

The heart of the FastLED library is the fast pin access.  This is a templated class that provides 1-2 cycle pin access, bypassing digital write and other such things.  As such, this will usually be the first bit of the library that you will want to port when moving to a new platform.  Once you have FastPIN up and running then you can do some basic work like testing toggles or running bit-bang'd SPI output.

There's two low level FastPin classes.  There's the base FastPIN template class, and then there is FastPinBB which is for bit-banded access on those MCUs that support bitbanding.  Note that the bitband class is optional and primarily useful in the implementation of other functionality internal to the platform.  This file is also where you would do the pin to port/bit mapping defines.

Explaining how the macros work and should be used is currently beyond the scope of this document.

## Porting fastspi.h

This is where you define the low level interface to the hardware SPI system (including a writePixels method that does a bunch of housekeeping for writing led data).  Use the fastspi_nop.h file as a reference for the methods that need to be implemented.  There are ofteh other useful methods that can help with the internals of the SPI code, I recommend taking a look at how the various platforms implement their SPI classes.

## Porting clockless.h

This is where you define the code for the clockless controllers.  Across ARM platforms this will usually be fairly similar - though different arm platforms will have different clock sources that you can/should use.

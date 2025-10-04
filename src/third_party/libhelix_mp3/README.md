
[![CMake on multiple platforms](https://github.com/chmorgan/libhelix-mp3/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/chmorgan/libhelix-mp3/actions/workflows/cmake-multi-platform.yml)

# Fixed-point MP3 decoder

Originally developed by RealNetworks, 2003

# Overview

This module contains a high-performance MPEG layer 3 audio decoder for 32-bit fixed-point 
processors. The following is a quick summary of what is and is not supported:

## Supported

 - layer 3
 - MPEG1, MPEG2, and MPEG2.5 (low sampling frequency extensions)
 - constant bitrate, variable bitrate, and free bitrate modes
 - mono and all stereo modes (normal stereo, joint stereo, dual-mono)

Not currently supported
 - layers 1 and 2

# Highlights

 - highly optimized for ARM processors (details in docs/ subdirectory)
 - reference x86 implementation
 - C and assembly code only (C++ not required)
 - reentrant, statically linkable
 - low memory (details in docs/ subdirectory)
 - option to use Intel Integrated Performance Primitives (details below)

# Supported platforms and toolchains

This codec should run on any 32-bit fixed-point processor which can perform a full 32x32-bit 
multiply (providing a 64-bit result). The following processors and toolchains are supported:
 - x86, Microsoft Visual C++
 - ARM, ARM Developer Suite (ADS)
 - ARM, Microsoft Embedded Visual C++
 - ARM, GNU toolchain (gcc)
 - RISC-V, GNU toolchain (gcc)
 - Xtensa (ESP32)

ARM refers to any processor supporting ARM architecture v.4 or above. Thumb is not required.
Typically this means an ARM7TDMI or better (including ARM9, StrongARM, XScale, etc.)

Generally ADS produces the fastest code. EVC 3.0 does not support inline assembly code for
ARM targets, so calls to MULSHIFT32 (smull on ARM) are left as function calls. For the
fastest code on targets which do not normally use ADS consider compiling with ADS, 
using the -S option to output assembly code, and feeding this assembly code to the assembler 
of your choice. This might require some syntax changes in the .S file.


# Project using this library

- [esp-audio-player](https://components.espressif.com/components/chmorgan/esp-audio-player) (esp-idf component)
- [esp-box](https://github.com/espressif/esp-box)
- Open an issue or a PR to add your project here!


# Adding support

Adding support for a new processor is fairly simple. Simply add a new block to the file 
real/assembly.h which implements the required inline assembly functions for your processor. 
Something like

```
#elif defined NEW_PROCESSOR

/* you implement MULSHIFT32() and so forth */

#else
#error Unsupported platform in assembly.h
#endif
```

Optionally you can rewrite or add assembly language files optimized for your platform. Note 
that many of the algorithms are designed for an ARM-type processor, so performance of the
unmodified C code might be noticeably worse on other architectures. 

Adding support for a new toolchain should be straightforward. Use the sample projects or the
CMakeLists.txt as a template for which source files to include.

Directory structure
-------------------
    docs       algorithm notes, memory and CPU usage figures, optimization suggestions
    src        library source code and header files
    testwrap   sample code to build a command-line test application

Code organization
-----------------
    /src/
        mp3dec.c              main decode functions, exports C-only API
        mp3tabs.c             common tables used by all implementations (bitrates, frame sizes, etc.)
        /pub/
            mp3common.h       defines low-level codec API which mp3dec.c calls
            mp3dec.h          defines high-level codec API which applications call
            mpadecobjfixpt.h  optional C++ shim API (only necessary if mpadecobj.cpp is used)
            statname.h        symbols which get name-mangled by C preprocessor to allow static linking
        /real                 full source code for RealNetworks MP3 decoder

To build an MP3 decoder library, you'll need to compile the top-level files and EITHER
real/*.c OR ipp/*.c and the correct IPP library.

Decoder using Real code: mp3dec.c + mp3tabs.c + real/*.c + real/arm/[files].s (if ARM)
Decoder using IPP code:  mp3dec.c + mp3tabs.c + ipp/*.c + ippac*.lib

Although the real/ and ipp/ source code follow the same top-level API (for Dequantize(),
Subband(), etc.) mixing and matching is not guaranteed to work. The outputs might
be ordered differently for optimization purposes, scaled differently, etc.

IPP 
--- 
For certain platforms Intel? has created highly-optimized object code libraries of DSP 
routines. These are called the Intel? Integrated Performance Primitives (IPP). If IPP 
libraries are available for a platform, this MP3 decoder can link them in and use them 
instead of the RealNetworks source code. To use IPP, you still need to build the top-level 
files (mp3dec.c, mp3tabs.c). You also build the files in ipp/*.c. These are just thin 
wrappers which provide the glue logic between the top-level decode functions in 
mp3dec.c and the optimized DSP primitives in the IPP libraries. IPP libraries are not 
included in this module. You must obtain them WITH A LICENSE directly from Intel. 
Further info on the latest versions of IPP (as of the date of this readme) is available 
from the URLs below

Intel? Integrated Performance Primitives for the 
Intel? PXA25x and PXA26x family of Processors, Version 3.0 
* http://www.intel.com/design/pca/applicationsprocessors/swsup/IPPv30.htm

Intel? Integrated Performance Primitives for 
Intel? Pentium? Processors and Intel? Itanium? Architectures 
* http://www.intel.com/software/products/ipp/ipp30/

These sites explain how to obtain IPP and the terms under which IPP libraries may be used.
The code in this module is merely wrapper code which calls IPP functions. You are fully 
responsible for adhering to the license agreement under which you obtain the IPP 
libraries from Intel.

To-Do list and status
---------------------
* faster/lower-memory dequantizer        (in progress)
* tighter fixed-point scaling            (in progress)
* remove all run-time math calls (/, %)  (in progress)
* ARM assembly code for DCT32, IMDCT     (if necessary)
* add test scripts and compliance docs
* write 32-bit C polyphase filter
  (where 64-bit MACC not available)

As these optimizations are completed, they will be added to the Helix codebase. Please 
continue to check there for updates. Changes will be noted in this readme. 

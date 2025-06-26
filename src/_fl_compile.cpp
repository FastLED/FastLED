// This file is necessary for avr on platformio, possibly arduino ide too.

// According to this report, all arduino files must be located at one folder.
// However this is only seen in the avr platforms.
//
// https://community.platformio.org/t/platformio-not-building-all-project-library-files/22606
// The solution then is to detect AVR + platformio and selectively compile in files

#ifndef FORCE_INCLUDE_FOR_AVR
#if defined(ARDUINO) && defined(__AVR__)
#define FORCE_INCLUDE_FOR_AVR 1
#endif
#else
#define FORCE_INCLUDE_FOR_AVR 0
#endif

#ifdef FORCE_INCLUDE_FOR_AVR
#include "fl/blur.cpp"
#include "fl/colorutils.cpp"
#include "fl/str.cpp"
#endif

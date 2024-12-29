/* Arduino DigitalIO Library
 * Copyright (C) 2013 by William Greiman
 *
 * This file is part of the Arduino DigitalIO Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino DigitalIO Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef GpioPinMap_h
#define GpioPinMap_h
#if defined(__AVR_ATmega168__)\
||defined(__AVR_ATmega168P__)\
||defined(__AVR_ATmega328P__)
// 168 and 328 Arduinos
#include "UnoGpioPinMap.h"
#elif defined(__AVR_ATmega1280__)\
|| defined(__AVR_ATmega2560__)
// Mega ADK
#include "MegaGpioPinMap.h"
#elif defined(__AVR_ATmega32U4__)
#ifdef CORE_TEENSY
#include "Teensy2GpioPinMap.h"
#else  // CORE_TEENSY
// Leonardo or Yun
#include "LeonardoGpioPinMap.h"
#endif  // CORE_TEENSY
#elif defined(__AVR_AT90USB646__)\
|| defined(__AVR_AT90USB1286__)
// Teensy++ 1.0 & 2.0
#include "Teensy2ppGpioPinMap.h"
#elif defined(__AVR_ATmega1284P__)\
|| defined(__AVR_ATmega1284__)\
|| defined(__AVR_ATmega644P__)\
|| defined(__AVR_ATmega644__)\
|| defined(__AVR_ATmega64__)\
|| defined(__AVR_ATmega32__)\
|| defined(__AVR_ATmega324__)\
|| defined(__AVR_ATmega16__)
#ifdef ARDUINO_1284P_AVR_DEVELOPERS
#include "AvrDevelopersGpioPinMap.h"
#elif defined(ARDUINO_1284P_BOBUINO)
#include "BobuinoGpioPinMap.h"
#elif defined(ARDUINO_1284P_SLEEPINGBEAUTY)
#include "SleepingBeautyGpioPinMap.h"
#elif defined(ARDUINO_1284P_STANDARD)
#include "Standard1284GpioPinMap.h"
#else // ARDUINO_1284P_SLEEPINGBEAUTY
#error Undefined variant 1284, 644, 324
#endif  // ARDUINO_1284P_SLEEPINGBEAUTY
#else  // 1284P, 1284, 644
#error Unknown board type.
#endif  // end all boards
#endif  // GpioPinMap_h

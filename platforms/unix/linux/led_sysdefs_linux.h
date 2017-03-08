#ifndef __INC_LED_SYSDEFS_LINUX_H
#define __INC_LED_SYSDEFS_LINUX_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>

#define FASTLED_UNIX
#define FASTLED_LINUX

#define FASTLED_SPI_BYTE_ONLY
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_HAS_MILLIS

#define FASTLED_NO_FASTPIN          // Do not declare a FastPIN class
#define HAS_HARDWARE_PIN_SUPPORT    // Only to supress warning message

#define F_CPU   16000000

typedef unsigned char byte;
typedef bool boolean;
typedef uint8_t RoReg;
typedef uint8_t RwReg;


unsigned long micros(void);
unsigned long millis(void);
unsigned long delay(unsigned long);

#endif

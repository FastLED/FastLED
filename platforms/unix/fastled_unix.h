#ifndef __INC_FASTLED_UNIX_H
#define __INC_FASTLED_UNIX_H

// Don't include pin and SPI headers;
// Unix has other ways of doing output.

// Default to not using PROGMEM - it doesn't exist on Unix.
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#endif

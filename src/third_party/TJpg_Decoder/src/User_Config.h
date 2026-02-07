#include "platforms/esp/is_esp.h"
#include "platforms/arm/rp/is_rp2040.h"

#if defined(FL_IS_ESP32) || defined(FL_IS_ESP8266) || defined(FL_IS_RP2040)
  #define TJPGD_LOAD_FFS
#endif

#define TJPGD_LOAD_SD_LIBRARY

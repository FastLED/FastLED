// @filter: (memory is large) and (board is not adafruit_feather_nrf52840_sense)
// The Adafruit Feather nRF52840 Sense marks D1 invalid in its variant table (UART RX),
// so the hardcoded pin-1 strip this sketch drives cannot be routed there. See #5300.

#include "FastLED.h"

#include "curr.h"

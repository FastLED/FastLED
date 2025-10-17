#if defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)

#include "fastled_esp8266_uart.h"

FASTLED_NAMESPACE_BEGIN

// ---- Explicit template instantiations for common orders --------------------
// Template implementations are in the header file (fastled_esp8266_uart.h)

template class UARTController_ESP8266<GRB>;
template class UARTController_ESP8266<RGB>;
template class UARTController_ESP8266<BRG>;
template class UARTController_ESP8266<RBG>;
template class UARTController_ESP8266<GBR>;
template class UARTController_ESP8266<BGR>;

FASTLED_NAMESPACE_END

#endif // ARDUINO_ARCH_ESP8266 || ESP8266

#if defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)

#include "fastled_esp8266_uart.h"
namespace fl {
// ---- Explicit template instantiations for common orders --------------------
// Template implementations are in the header file (fastled_esp8266_uart.h)

template class UARTController_ESP8266<GRB>;
template class UARTController_ESP8266<RGB>;
template class UARTController_ESP8266<BRG>;
template class UARTController_ESP8266<RBG>;
template class UARTController_ESP8266<GBR>;
template class UARTController_ESP8266<BGR>;
}  // namespace fl
#endif // ARDUINO_ARCH_ESP8266 || ESP8266

/// @file fl/stl/isr/critical_section.cpp.hpp
/// @brief critical_section implementation

#include "fl/stl/isr/critical_section.h"
#include "platforms/isr.h"

namespace fl {

void interrupt_disable() {
    interruptsDisable();
}

void interrupt_enable() {
    interruptsEnable();
}

namespace isr {

critical_section::critical_section() {
    fl::interrupt_disable();
}

critical_section::~critical_section() {
    fl::interrupt_enable();
}

} // namespace isr
} // namespace fl

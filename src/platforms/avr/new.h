// ok no namespace fl
#pragma once

// AVR placement new operator - in global namespace
// This file exists for organizational purposes but delegates to inplacenew.h
// which handles placement new definition properly without including <new> in headers

#include "avr/inplacenew.h"

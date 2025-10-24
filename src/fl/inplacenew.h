// ok no namespace fl
#pragma once

// Placement new operator support - delegates to platform-specific implementation
// This file exists for organizational purposes but delegates to platforms/inplacenew.h
// which handles platform detection and either includes <new> or provides manual definition

#include "platforms/inplacenew.h"

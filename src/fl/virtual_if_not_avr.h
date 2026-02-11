#pragma once

#include "platforms/is_platform.h"

#ifdef FL_IS_AVR
#define VIRTUAL_IF_NOT_AVR
#define OVERRIDE_IF_NOT_AVR
#else
#define VIRTUAL_IF_NOT_AVR virtual
#define OVERRIDE_IF_NOT_AVR override
#endif
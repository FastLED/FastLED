#pragma once

#ifdef __AVR__
#define VIRTUAL_IF_NOT_AVR
#else
#define VIRTUAL_IF_NOT_AVR virtual
#endif
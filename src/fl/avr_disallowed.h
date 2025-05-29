#pragma once

#ifdef __AVR__
#define AVR_DISALLOWED                                                         \
    [[deprecated("This function or class is deprecated on AVR.")]]
#else
#define AVR_DISALLOWED
#endif

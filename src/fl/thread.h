#pragma once

#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

#if FASTLED_MULTITHREADED and !defined(FASTLED_MULTITHREAD_SUPPRESS_WARNING)
#warning                                                                       \
    "Warning: FastLED has never been used in a multi threading environment and may not work."
#endif

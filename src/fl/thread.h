#pragma once

#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

#ifndef FASTLED_USE_THREAD_LOCAL
#define FASTLED_USE_THREAD_LOCAL 0
#endif

#if FASTLED_MULTITHREADED and !defined(FASTLED_MULTITHREAD_SUPPRESS_WARNING)
#warning                                                                       \
    "Warning: FastLED has never been used in a multi threading environment and may not work."
#endif

#if FASTLED_USE_THREAD_LOCAL and !defined(FASTLED_THREAD_LOCAL_SUPPRESS_WARNING)
#warning                                                                       \
    "Warning: Thread-local storage is enabled. This is experimental functionality."
#endif

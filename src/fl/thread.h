#pragma once

#ifndef FASTLED_MULTITHREADED
#if defined(FASTLED_TESTING) && (defined(__has_include) && __has_include(<pthread.h>))
#define FASTLED_MULTITHREADED 1
#else
#define FASTLED_MULTITHREADED 0
#endif
#endif  // FASTLED_MULTITHREADED

#ifndef FASTLED_USE_THREAD_LOCAL
#define FASTLED_USE_THREAD_LOCAL FASTLED_MULTITHREADED
#endif  // FASTLED_USE_THREAD_LOCAL

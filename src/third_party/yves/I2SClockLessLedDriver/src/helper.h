
#pragma once
#ifndef __HELPER__
#define __HELPER__
#define HOW_LONG(name, func)                                                                                                         \
    {                                                                                                                                \
        uint32_t __time1__ = ESP.getCycleCount();                                                                                    \
        func;                                                                                                                        \
        uint32_t __time2__ = ESP.getCycleCount() - __time1__;                                                                        \
        printf("The function *** %s *** took %.2f ms or %.2f fps\n", name, (float)__time2__ / 240000, (float)240000000 / __time2__); \
    }

#define RUN_SKETCH_FOR(name, duration, func)                                                      \
    {                                                                                             \
        printf("Start Sketch: %s\n", name);                                                       \
        uint32_t __timer1__ = ESP.getCycleCount();                                                \
        uint32_t __timer2__ = ESP.getCycleCount();                                                \
        while ((__timer2__ - __timer1__) / 240000 < duration)                                     \
        {                                                                                         \
            func;                                                                                 \
            __timer2__ = ESP.getCycleCount();                                                     \
        }                                                                                         \
        printf("End Sketch: %s after %.2fms\n", name, (float)(__timer2__ - __timer1__) / 240000); \
    }

#define RUN_SKETCH_N_TIMES(name, ntimes, func)                                    \
    {                                                                               \
        printf("Start Sketch: %s\n", name);                                         \
        uint32_t __timer1__ = 0;                                                    \
        uint32_t __timer2__ = 0;                                                    \
        while ((__timer2__ - __timer1__) < duration)                                \
        {                                                                           \
            func;                                                                   \
            __timer2__++;                                                           \
        }                                                                           \
        printf("End Sketch: %s after %d times\n", name, (__timer2__ - __timer1__)); \
    }

#endif
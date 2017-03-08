#define FASTLED_INTERNAL
#include "FastLED.h"

#ifdef FASTLED_UNIX
#include <time.h>

uint64_t fastled_epoch_time_us = 0;
uint64_t fastled_epoch_time_ms = 0;
int fastled_epoch_initted = 0;


static void fastled_setup_epoch(void) {
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    fastled_epoch_time_us = (((uint64_t)tp.tv_sec * (uint64_t)1000000ull) + ((uint64_t)tp.tv_nsec / 1000ull));
    fastled_epoch_time_ms = (((uint64_t)tp.tv_sec * (uint64_t)1000ull) + (uint64_t)(tp.tv_nsec / 1000000ull));
    fastled_epoch_initted = 1;
}

extern "C" unsigned long fastled_micros() {
    struct timespec tp;

    if (!fastled_epoch_initted)
        fastled_setup_epoch();

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (((uint64_t)tp.tv_sec * (uint64_t)1000000) + ((uint64_t)tp.tv_nsec / 1000)) - fastled_epoch_time_us;
}

extern "C" unsigned long fastled_millis() {
    struct timespec tp;

    if (!fastled_epoch_initted)
        fastled_setup_epoch();

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (((uint64_t)tp.tv_sec * (uint64_t)1000) + (uint64_t)(tp.tv_nsec / 1000000)) - fastled_epoch_time_ms;

}

extern "C" unsigned long fastled_delay(unsigned long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

unsigned long millis() __attribute__ ((weak, alias ("fastled_millis")));
unsigned long micros() __attribute__ ((weak, alias ("fastled_micros")));
unsigned long delay(unsigned long) __attribute__ ((weak, alias ("fastled_delay")));

#endif //FASTLED_UNIX

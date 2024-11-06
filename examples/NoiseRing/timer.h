#pragma once

#include <stdint.h>

class Timer {
  public:
    Timer() : start_time(0), duration(0), running(false) {}
    void start(uint32_t now, uint32_t duration) {
        start_time = now;
        this->duration = duration;
        running = true;
    }
    bool update(uint32_t now) {
        if (!running) {
            return false;
        }
        uint32_t elapsed = now - start_time;
        if (elapsed > duration) {
            running = false;
            return false;
        }
        return true;
    }

  private:
    uint32_t start_time;
    uint32_t duration;
    bool running;
};
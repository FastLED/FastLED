#pragma once

#include "fl/stdint.h"

/**
 * @brief A simple timer utility class for tracking timed events
 * 
 * This class provides basic timer functionality for animations and effects.
 * It can be used to track whether a specific duration has elapsed since
 * the timer was started.
 */
class Timer {
  public:
    /**
     * @brief Construct a new Timer object
     * 
     * Creates a timer in the stopped state with zero duration.
     */
    Timer() : start_time(0), duration(0), running(false) {}
    
    /**
     * @brief Start the timer with a specific duration
     * 
     * @param now Current time in milliseconds (typically from millis())
     * @param duration How long the timer should run in milliseconds
     */
    void start(uint32_t now, uint32_t duration) {
        start_time = now;
        this->duration = duration;
        running = true;
    }
    
    /**
     * @brief Update the timer state based on current time
     * 
     * Checks if the timer is still running based on the current time.
     * If the specified duration has elapsed, the timer will stop.
     * 
     * @param now Current time in milliseconds (typically from millis())
     * @return true if the timer is still running, false if stopped or elapsed
     */
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
    uint32_t start_time;  // When the timer was started (in milliseconds)
    uint32_t duration;    // How long the timer should run (in milliseconds)
    bool running;         // Whether the timer is currently active
};

#include <FastLED.h>
#include "Events.h"

// Example-only API usage
// Events events;
// events.add(fl::time() + 1000, []{ start_graphics(); });
// events.add(fl::time() + 2000, []{ play_next(); });
// In loop(): fl::async_run();

Events events;
EventsRunner* eventsRunner = nullptr;

void start_graphics() {
    // placeholder for user action
}

void play_next() {
    // placeholder for user action
}

void setup() {
    // Register the events runner so events fire when fl::async_run() is called
    static EventsRunner runner(events);
    eventsRunner = &runner;
    fl::AsyncManager::instance().register_runner(eventsRunner);

    // Schedule demo events
    events.add(fl::time() + 1000, []{ start_graphics(); });
    events.add(fl::time() + 2000, []{ play_next(); });

    // Or relative helper
    events.add_after(3000, []{ /* another action */ });
}

void loop() {
    // Pump async; will invoke EventsRunner::update() which fires ready events
    fl::async_run();

    // Do other work here...
}
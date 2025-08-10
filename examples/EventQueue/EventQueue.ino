#include <FastLED.h>
#include "Events.h"

// Example-only API usage
// Events events;
// EventsRunner runner(events);
// int id1 = runner.add(fl::time() + 1000, []{ start_graphics(); });
// int id2 = runner.add(fl::time() + 2000, []{ play_next(); });
// runner.cancel(id2); // Demonstrate cancellation
// In loop(): runner.processEvents(); FastLED.show();

Events events;
EventsRunner runner(events);

void start_graphics() {
    // placeholder for user action
}

void play_next() {
    // placeholder for user action
}

void setup() {
    // Schedule demo events
    int id1 = runner.add(fl::time() + 1000, []{ start_graphics(); });
    int id2 = runner.add(fl::time() + 2000, []{ play_next(); });
    (void)id1;

    // Cancel the second event as a demonstration
    runner.cancel(id2);

    // Relative helper
    runner.add_after(3000, []{ /* another action */ });
}

void loop() {
    // Manually pump events every iteration before rendering
    runner.processEvents();

    // Do other work here...

    // Update LEDs after processing events
    FastLED.show();
}
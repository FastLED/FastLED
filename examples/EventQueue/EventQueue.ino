#include <FastLED.h>
#include "Events.h"

// Example-only API usage
// Events events;
// events.add(fl::time() + 1000, []{ start_graphics(); });
// events.add(fl::time() + 2000, []{ play_next(); });
// In loop(): processEvents(); FastLED.show();

Events events;

void processEvents() {
    events.update();
}

void start_graphics() {
    // placeholder for user action
}

void play_next() {
    // placeholder for user action
}

void setup() {
    // Schedule demo events
    events.add(fl::time() + 1000, []{ start_graphics(); });
    events.add(fl::time() + 2000, []{ play_next(); });
    events.add_after(3000, []{ /* another action */ });
}

void loop() {
    // Manually pump events every iteration before rendering
    processEvents();

    // Do other work here...

    // Update LEDs after processing events
    FastLED.show();
}
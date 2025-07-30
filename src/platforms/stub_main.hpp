#pragma once

extern void setup();
extern void loop();

int main() {
    // Super simple main function that just calls the setup and loop functions.
    setup();
    while (1) {
        loop();
    }
}

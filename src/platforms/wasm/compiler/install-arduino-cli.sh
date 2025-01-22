#!/bin/bash

# this is a work in progress for install the arduino-cli tool so that
# ino sketches can be converted into cpp code in preparation for the
# wasm compilation.

wget https://github.com/arduino/arduino-cli/releases/download/v1.0.4/arduino-cli_1.0.4_Linux_64bit.tar.gz && \
    tar -xvf arduino-cli_1.0.4_Linux_64bit.tar.gz && \
    mv arduino-cli /usr/bin/arduino-cli && \
    rm -rf arduino-cli_1.0.4_Linux_64bit*

# COPY examples/Blink/Blink.ino /pre-warm/Blink/Blink.ino
arduino-cli core update-index
arduino-cli core install arduino:avr

mkdir -p /pre-warm/Blink && \
    cat <<EOF > /pre-warm/Blink/Blink.ino
void setup() {
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    do_it();
    // turn the LED on (HIGH is the voltage level)
    digitalWrite(LED_BUILTIN, HIGH);
    // wait for a second
    delay(1000);
    // turn the LED off by making the voltage LOW
    digitalWrite(LED_BUILTIN, LOW);
    // wait for a second
    delay(1000);
}

void do_it() {
    int i = 0;
    i++;
}
EOF

arduino-cli compile --preprocess --fqbn arduino:avr:uno /pre-warm/Blink/Blink.ino
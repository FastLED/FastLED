# This folder represents the different platforms that FastLED compiles to

Here are the top level descriptions:

## avr

This represents the original Arduino UNO and the avr variants like 32u and attiny.

## arm

The most popular chipset in this family is the Teensy, located at arm/k20.

Note that the name should be changed, but user are probably compiling against it so we are going
to keep it as-is.

## esp

Represents the esp line of chips. Most of the recent action has been happening in esp/32. Esp/8266 is the original esp chip.

## wasm

This is the web browser platform port of FastLED.

## Stub

This is for native compilation of FastLED. For example, the unit tests use this platform version.

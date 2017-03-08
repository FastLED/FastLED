# FastLED on Linux (RaspberryPI) README

# Caveats

There's a few limitation on using this FastLED port under Linux-

- Only 4-pin strips such as the WS2801 that uses a clock line are supported.
- Only hardware SPI lines are supported. On the PI this is GPIO10(SPI_MOSI) & GPIO11(SPI_CLK) lines. (Aka pins on the header 19 & 23).
- "Software" SPI (aka bitbanging) is not supported.
- The library does not support an Arduino like framework. (No setup(), loop(), digitalWrite/digitalRead, etc.) HOWEVER, ```micros()```, ```millis()``` and ```delay()``` are available, and weakly typed if you want to override them.
- The DATA and CLOCK arguments when declaring a led strip are redefined to mean the SPI bus number (B) and SPI check select (S). For the Raspberry PI, this will always be 0 & 0.
- FastLED.addLeds will throw an exception (std::system_error with an errno value and descriptive message) if the SPI bus cannot be found or initialized.

In theory, FastLED could work on any Linux box that has a SPI device driver (/dev/spidev*) available. 

# Compiling and Installing FastLED

To compile:

```shell
$ make -f Makefile.linux
```

Then to install:
```
$ sudo -s make -f Makefile.linux install
```

This will install the headers into /usr/local/include/FastLED, and the library into /usr/local/lib.

# RaspberryPi: make sure /dev/spidev0.0 exists

Before trying to run any programs, make sure the SPI interface exists on the RaspberryPi:

```sh
$ ls -l /dev/spidev0.0
crw-rw---- 1 root spi 153, 0 Dec 12 22:18 /dev/spidev0.0
```

Some Raspberry PI systems may not have the SPI device driver loaded. You can do this by running raspi-config.

```sh
$ sudo -s raspi-config
```

Then select "Advanced Options" > "SPI". Select "YES" to enable the SPI bus. You may need to reboot the system in order for the module to be loaded.

# Using the FastLED library

Using  FastLED under Linux is similar to using it under Arduino. The same APIs are available.

As stated above, the DATA and CLOCK arguments when declaring a strip are redefined to mean the SPI bus and Chip Select. Under a Raspberry PI, both values would be 0.

Here's an example that rapidly changes from white to blue:

```C

#include <FastLED.h>
#include <iostream>

#define NUM_LEDS    20

CRGB leds[NUM_LEDS];

int main(int argc, char **argv) {
    try {
        // addLeds may throw an exception if the SPI interface cannot be found.
        FastLED.addLeds<WS2801, 0, 0, RGB>(leds, NUM_LEDS);
    } catch (std::exception const &exc) {
        std::cerr << "Exception caught " << exc.what() << "\n";
        exit(0);
    } catch (...) {
        std::cerr << "Unknown exception caught\n";
        exit(0);
    }

    FastLED.setBrightness(25);

    for (;;) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::White;
        }
        FastLED.delay(20);
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Blue;
        }
        FastLED.delay(20);
    }
}
```

To compile this example:

```sh
$ g++ -o example example.cpp -I/usr/include/local/FastLED -L/usr/local/lib -lfastled
```

And then to run:

```sh
$ ./example
```

# Compiling and running the LinuxDemo

The directory, examples/LinuxDemo, in the FastLED source contains a version of the DemoReel100 example that runs under Linux.

To compile:

```sh
$ cd examples/LinuxDemo
$ make
```

Then to run:
```
$ ./LinuxDemo
```

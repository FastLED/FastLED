/*
 * ezWS2812.h - Simple WS2812 (NeoPixel) driver for Silicon Labs MGM240/MG24
 *
 * Based on Silicon Labs ezWS2812 library by Tamas Jozsi
 * Copyright (c) 2024 Silicon Laboratories Inc.
 *
 * Integrated into FastLED by FastLED Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#ifndef ezWS2812_h
#define ezWS2812_h

#include <Arduino.h>  // ok include - SPI driver hardware abstraction
#include <SPI.h>

namespace fl {
namespace third_party {

/// @brief ezWS2812 SPI driver for WS2812 LEDs
///
/// This driver uses the SPI peripheral to generate WS2812 timing signals.
/// Each WS2812 bit is encoded as 8 SPI bits to achieve the required timing.
///
/// Hardware Requirements:
/// - Consumes one SPI peripheral
/// - Requires SPI clock of 3.2MHz for proper WS2812 timing
/// - SPI MOSI pin must be connected to LED data line
///
/// Usage:
/// @code
/// ezWS2812 leds(60);  // 60 LEDs
/// leds.begin();
/// leds.set_all(255, 0, 0);  // All red
/// leds.end_transfer();
/// @endcode
class ezWS2812 {
private:
    uint16_t num_leds_;
    uint8_t brightness_;
    SPIClass* spi_;

    /// @brief Convert color bit to SPI signal for '1' bit
    /// @return SPI byte pattern for logical '1'
    inline uint8_t one() const {
        return 0xFC; // 11111100 - long high pulse for '1'
    }

    /// @brief Convert color bit to SPI signal for '0' bit
    /// @return SPI byte pattern for logical '0'
    inline uint8_t zero() const {
        return 0x80; // 10000000 - short high pulse for '0'
    }

    /// @brief Convert color byte to SPI bit pattern
    void color_to_spi(uint8_t color, uint8_t* buffer) const {
        for (int i = 7; i >= 0; i--) {
            buffer[7-i] = (color & (1 << i)) ? one() : zero();
        }
    }

public:
    /// @brief Constructor
    /// @param num_leds Number of LEDs in the strip
    /// @param spi SPI instance to use (default: SPI)
    ezWS2812(uint16_t num_leds, SPIClass& spi = SPI)
        : num_leds_(num_leds), brightness_(100), spi_(&spi) {}

    /// @brief Initialize SPI communication
    void begin() {
        spi_->begin();
        spi_->beginTransaction(SPISettings(3200000, MSBFIRST, SPI_MODE0));
    }

    /// @brief End SPI communication
    void end() {
        spi_->endTransaction();
    }

    /// @brief Set brightness (0-100%)
    void set_brightness(uint8_t brightness) {
        brightness_ = (brightness > 100) ? 100 : brightness;
    }

    /// @brief Set color for specific number of LEDs
    /// @param red Red value (0-255)
    /// @param green Green value (0-255)
    /// @param blue Blue value (0-255)
    /// @param count Number of LEDs to set (default: 1)
    void set_pixel(uint8_t red, uint8_t green, uint8_t blue, uint16_t count = 1) {
        uint8_t spi_buffer[24]; // 3 colors × 8 bits = 24 SPI bytes per pixel

        // Apply brightness
        red = (red * brightness_) / 100;
        green = (green * brightness_) / 100;
        blue = (blue * brightness_) / 100;

        // Convert to SPI bit patterns (GRB order for WS2812)
        color_to_spi(green, &spi_buffer[0]);  // Green first
        color_to_spi(red, &spi_buffer[8]);    // Red second
        color_to_spi(blue, &spi_buffer[16]);  // Blue third

        // Send pixel data
        for (uint16_t i = 0; i < count && i < num_leds_; i++) {
            for (int j = 0; j < 24; j++) {
                spi_->transfer(spi_buffer[j]);
            }
        }
    }

    /// @brief Set all LEDs to the same color
    /// @param red Red value (0-255)
    /// @param green Green value (0-255)
    /// @param blue Blue value (0-255)
    void set_all(uint8_t red, uint8_t green, uint8_t blue) {
        set_pixel(red, green, blue, num_leds_);
    }

    /// @brief Complete LED data transfer
    /// Sends reset signal to latch data into LEDs
    void end_transfer() {
        // WS2812 reset time (>50µs low)
        delayMicroseconds(300);
    }
};

/// @brief ezWS2812 GPIO driver for WS2812 LEDs
///
/// This driver uses direct GPIO manipulation with precise timing
/// to generate WS2812 signals. Optimized for 39MHz and 78MHz CPUs.
///
/// Hardware Requirements:
/// - Any GPIO pin can be used
/// - Interrupts are disabled during transmission
/// - CPU frequency must be 39MHz or 78MHz for accurate timing
///
/// Usage:
/// @code
/// ezWS2812gpio leds(60, 7);  // 60 LEDs on pin 7
/// leds.begin();
/// leds.set_all(0, 255, 0);  // All green
/// leds.end_transfer();
/// @endcode
class ezWS2812gpio {
private:
    uint16_t num_leds_;
    uint8_t pin_;
    uint8_t brightness_;
    uint32_t pin_mask_;
    volatile uint32_t* port_set_;
    volatile uint32_t* port_clear_;

    /// @brief Send a single bit using precise timing
    void send_bit(bool bit_value) const {
        if (F_CPU >= 78000000) {
            // Timing for 78MHz CPU
            if (bit_value) {
                // Send '1' bit: ~0.8µs high, ~0.45µs low
                *port_set_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                );
                *port_clear_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop;"
                );
            } else {
                // Send '0' bit: ~0.4µs high, ~0.85µs low
                *port_set_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop;"
                );
                *port_clear_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                );
            }
        } else {
            // Timing for 39MHz CPU (default)
            if (bit_value) {
                // Send '1' bit: ~0.8µs high, ~0.45µs low
                *port_set_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                );
                *port_clear_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop;"
                );
            } else {
                // Send '0' bit: ~0.4µs high, ~0.85µs low
                *port_set_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop;"
                );
                *port_clear_ = pin_mask_;
                __asm__ volatile(
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                    "nop; nop; nop; nop; nop; nop; nop; nop;"
                );
            }
        }
    }

    /// @brief Send a byte (8 bits) with MSB first
    void send_byte(uint8_t byte_value) const {
        for (int i = 7; i >= 0; i--) {
            send_bit((byte_value >> i) & 0x01);
        }
    }

public:
    /// @brief Constructor
    /// @param num_leds Number of LEDs in the strip
    /// @param pin GPIO pin number for data line
    ezWS2812gpio(uint16_t num_leds, uint8_t pin)
        : num_leds_(num_leds), pin_(pin), brightness_(100) {}

    /// @brief Initialize GPIO pin
    void begin() {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);

        // Cache port addresses for faster access
        // Note: This is platform-specific and needs proper implementation
        // for Silicon Labs MGM240/MG24
        pin_mask_ = digitalPinToBitMask(pin_);
        uint8_t port = digitalPinToPort(pin_);
        port_set_ = portOutputRegister(port);
        port_clear_ = port_set_ + 1; // Typically clear register is next
    }

    /// @brief End communication
    void end() {
        digitalWrite(pin_, LOW);
    }

    /// @brief Set brightness (0-100%)
    void set_brightness(uint8_t brightness) {
        brightness_ = (brightness > 100) ? 100 : brightness;
    }

    /// @brief Set color for specific number of LEDs
    void set_pixel(uint8_t red, uint8_t green, uint8_t blue, uint16_t count = 1) {
        // Apply brightness
        red = (red * brightness_) / 100;
        green = (green * brightness_) / 100;
        blue = (blue * brightness_) / 100;

        noInterrupts();
        for (uint16_t i = 0; i < count && i < num_leds_; i++) {
            // Send in GRB order for WS2812
            send_byte(green);
            send_byte(red);
            send_byte(blue);
        }
        interrupts();
    }

    /// @brief Set all LEDs to the same color
    void set_all(uint8_t red, uint8_t green, uint8_t blue) {
        set_pixel(red, green, blue, num_leds_);
    }

    /// @brief Complete LED data transfer
    void end_transfer() {
        // WS2812 reset time (>50µs low)
        delayMicroseconds(300);
    }
};

}  // namespace third_party
}  // namespace fl

#endif // ezWS2812_h
/// @file tests/fl/pin.cpp
/// Unit tests for fl::pin API (GPIO and unified PWM frequency)
/// and digitalMultiWrite bulk pin API
/// Implementation is in src/fl/pin.cpp.hpp

#include "fl/system/pin.h"
#include "test.h"
#include "fl/system/pins.h"
#include "fl/fltest.h"

FL_TEST_FILE(FL_FILEPATH) {

// ============================================================================
// PWM Frequency Tests
// ============================================================================

FL_TEST_CASE("pwm_basic_init_cleanup") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 100), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 100u);
    fl::analogWrite(5, 128);
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 0u);
}

FL_TEST_CASE("pwm_invalid_frequency") {
    // Zero frequency invalid
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 0), 0);

    // On stub platform (ISR fallback required), max is 500 Hz
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 1000), 0);
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 600), 0);

    // Valid frequencies should work
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 1), 0);     // Min
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);

    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 500), 0);   // Max for ISR
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);
}

FL_TEST_CASE("pwm_reconfigure") {
    // First init
    FL_REQUIRE_EQ(fl::setPwmFrequency(10, 100), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(10), 100u);
    fl::analogWrite(10, 128);

    // Reconfigure without explicit end (auto-releases and reconfigures)
    FL_REQUIRE_EQ(fl::setPwmFrequency(10, 200), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(10), 200u);
    fl::analogWrite(10, 200);

    FL_REQUIRE_EQ(fl::pwmEnd(10), 0);
}

FL_TEST_CASE("pwm_channel_allocation") {
    // Allocate all 8 channels
    for (int i = 0; i < 8; i++) {
        FL_REQUIRE_EQ(fl::setPwmFrequency(10 + i, 60 + i * 10), 0);
        fl::analogWrite(10 + i, 128);
    }

    // 9th should fail (all channels in use)
    FL_REQUIRE_LT(fl::setPwmFrequency(99, 60), 0);

    // Release one channel
    FL_REQUIRE_EQ(fl::pwmEnd(12), 0);

    // Now 9th should succeed (reusing freed channel)
    FL_REQUIRE_EQ(fl::setPwmFrequency(99, 60), 0);

    // Cleanup all
    for (int i = 0; i < 8; i++) {
        fl::pwmEnd(10 + i);
    }
    fl::pwmEnd(99);
}

FL_TEST_CASE("pwm_duty_cycle_via_analog_write") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(7, 100), 0);

    // Test full duty cycle range via analogWrite
    fl::analogWrite(7, 0);      // 0%
    fl::analogWrite(7, 64);     // 25%
    fl::analogWrite(7, 128);    // 50%
    fl::analogWrite(7, 192);    // 75%
    fl::analogWrite(7, 255);    // 100%

    FL_REQUIRE_EQ(fl::pwmEnd(7), 0);
}

FL_TEST_CASE("pwm_duty_cycle_via_set_pwm16") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(8, 100), 0);

    // Test duty cycle via setPwm16 (16-bit duty)
    fl::setPwm16(8, 0);         // 0%
    fl::setPwm16(8, 16384);     // 25%
    fl::setPwm16(8, 32768);     // 50%
    fl::setPwm16(8, 49152);     // 75%
    fl::setPwm16(8, 65535);     // 100%

    FL_REQUIRE_EQ(fl::pwmEnd(8), 0);
}

FL_TEST_CASE("pwm_multiple_frequencies") {
    // Test different frequencies on different channels
    FL_REQUIRE_EQ(fl::setPwmFrequency(10, 10), 0);    // 10 Hz
    FL_REQUIRE_EQ(fl::setPwmFrequency(11, 60), 0);    // 60 Hz
    FL_REQUIRE_EQ(fl::setPwmFrequency(12, 120), 0);   // 120 Hz
    FL_REQUIRE_EQ(fl::setPwmFrequency(13, 240), 0);   // 240 Hz

    FL_REQUIRE_EQ(fl::getPwmFrequency(10), 10u);
    FL_REQUIRE_EQ(fl::getPwmFrequency(11), 60u);
    FL_REQUIRE_EQ(fl::getPwmFrequency(12), 120u);
    FL_REQUIRE_EQ(fl::getPwmFrequency(13), 240u);

    fl::analogWrite(10, 100);
    fl::analogWrite(11, 150);
    fl::analogWrite(12, 200);
    fl::analogWrite(13, 250);

    FL_REQUIRE_EQ(fl::pwmEnd(10), 0);
    FL_REQUIRE_EQ(fl::pwmEnd(11), 0);
    FL_REQUIRE_EQ(fl::pwmEnd(12), 0);
    FL_REQUIRE_EQ(fl::pwmEnd(13), 0);
}

FL_TEST_CASE("pwm_end_uninitialized") {
    // Ending an uninitialized pin should fail gracefully
    FL_REQUIRE_LT(fl::pwmEnd(99), 0);
}

FL_TEST_CASE("pwm_reuse_after_end") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(15, 100), 0);
    fl::analogWrite(15, 128);
    FL_REQUIRE_EQ(fl::pwmEnd(15), 0);

    // After end, can re-initialize with different frequency
    FL_REQUIRE_EQ(fl::setPwmFrequency(15, 200), 0);
    fl::analogWrite(15, 200);
    FL_REQUIRE_EQ(fl::pwmEnd(15), 0);
}

FL_TEST_CASE("pwm_edge_frequencies") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(20, 1), 0);     // 1 Hz (minimum)
    FL_REQUIRE_EQ(fl::pwmEnd(20), 0);

    FL_REQUIRE_EQ(fl::setPwmFrequency(20, 500), 0);   // 500 Hz (ISR maximum)
    FL_REQUIRE_EQ(fl::pwmEnd(20), 0);
}

FL_TEST_CASE("pwm_zero_duty") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(25, 100), 0);
    fl::analogWrite(25, 0);  // 0% duty
    FL_REQUIRE_EQ(fl::pwmEnd(25), 0);
}

FL_TEST_CASE("pwm_full_duty") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(30, 100), 0);
    fl::analogWrite(30, 255);  // 100% duty
    FL_REQUIRE_EQ(fl::pwmEnd(30), 0);
}

FL_TEST_CASE("pwm_update_duty_multiple_times") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(35, 100), 0);

    fl::analogWrite(35, 50);
    fl::analogWrite(35, 100);
    fl::analogWrite(35, 150);
    fl::analogWrite(35, 200);

    FL_REQUIRE_EQ(fl::pwmEnd(35), 0);
}

FL_TEST_CASE("pwm_frequency_zero_rejected") {
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 0), 0);
}

FL_TEST_CASE("pwm_frequency_above_isr_max_rejected") {
    // On stub platform, all frequencies need ISR fallback, max is 500 Hz
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 501), 0);
    FL_REQUIRE_LT(fl::setPwmFrequency(5, 1000), 0);
}

FL_TEST_CASE("pwm_frequency_valid_range") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 1), 0);     // Min
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 1u);
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);

    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 500), 0);   // Max for ISR
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 500u);
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);
}

FL_TEST_CASE("pwm_frequency_get_unconfigured") {
    // Unconfigured pin returns 0
    FL_REQUIRE_EQ(fl::getPwmFrequency(77), 0u);
}

FL_TEST_CASE("pwm_frequency_analog_write_after_set") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(10, 60), 0);
    fl::analogWrite(10, 128);
    FL_REQUIRE_EQ(fl::pwmEnd(10), 0);
}

FL_TEST_CASE("pwm_frequency_set_pwm16_after_set") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(10, 60), 0);
    fl::setPwm16(10, 32768);
    FL_REQUIRE_EQ(fl::pwmEnd(10), 0);
}

FL_TEST_CASE("pwm_frequency_analog_write_no_set") {
    // Without setPwmFrequency, analogWrite forwards to platform (no crash)
    fl::analogWrite(10, 128);
}

FL_TEST_CASE("pwm_frequency_set_pwm16_no_set") {
    // Without setPwmFrequency, setPwm16 forwards to platform (no crash)
    fl::setPwm16(10, 32768);
}

FL_TEST_CASE("pwm_frequency_multiple_pins") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(1, 50), 0);
    FL_REQUIRE_EQ(fl::setPwmFrequency(2, 100), 0);
    FL_REQUIRE_EQ(fl::setPwmFrequency(3, 200), 0);

    FL_REQUIRE_EQ(fl::getPwmFrequency(1), 50u);
    FL_REQUIRE_EQ(fl::getPwmFrequency(2), 100u);
    FL_REQUIRE_EQ(fl::getPwmFrequency(3), 200u);

    fl::pwmEnd(1);
    fl::pwmEnd(2);
    fl::pwmEnd(3);
}

FL_TEST_CASE("pwm_frequency_pin_reuse") {
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 60), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 60u);
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);

    // Re-init with different frequency
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 120), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 120u);
    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);
}

FL_TEST_CASE("pwm_frequency_reconfigure_without_end") {
    // Calling setPwmFrequency again on same pin auto-releases and reconfigures
    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 60), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 60u);

    FL_REQUIRE_EQ(fl::setPwmFrequency(5, 200), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(5), 200u);

    FL_REQUIRE_EQ(fl::pwmEnd(5), 0);
}

FL_TEST_CASE("pwm_frequency_channel_exhaustion") {
    // Allocate all 8 channels
    for (int i = 0; i < 8; i++) {
        FL_REQUIRE_EQ(fl::setPwmFrequency(50 + i, 100), 0);
    }

    // 9th should fail
    FL_REQUIRE_LT(fl::setPwmFrequency(60, 100), 0);

    // Release one
    FL_REQUIRE_EQ(fl::pwmEnd(53), 0);

    // Now should succeed
    FL_REQUIRE_EQ(fl::setPwmFrequency(60, 100), 0);

    // Cleanup all
    for (int i = 0; i < 8; i++) {
        fl::pwmEnd(50 + i);
    }
    fl::pwmEnd(60);
}

// ============================================================================
// pinMode interaction with PWM
// ============================================================================

FL_TEST_CASE("pwm_pinmode_disables_pwm") {
    // Configure PWM on a pin
    FL_REQUIRE_EQ(fl::setPwmFrequency(20, 100), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(20), 100u);
    fl::analogWrite(20, 128);

    // Calling pinMode should release the PWM channel
    fl::pinMode(20, fl::PinMode::Output);

    // PWM should be released (frequency returns 0)
    FL_REQUIRE_EQ(fl::getPwmFrequency(20), 0u);

    // Should be able to reconfigure PWM after pinMode
    FL_REQUIRE_EQ(fl::setPwmFrequency(20, 200), 0);
    FL_REQUIRE_EQ(fl::getPwmFrequency(20), 200u);
    FL_REQUIRE_EQ(fl::pwmEnd(20), 0);
}

FL_TEST_CASE("pwm_pinmode_releases_channel") {
    // Allocate all 8 channels
    for (int i = 0; i < 8; i++) {
        FL_REQUIRE_EQ(fl::setPwmFrequency(30 + i, 100), 0);
    }

    // 9th should fail
    FL_REQUIRE_LT(fl::setPwmFrequency(99, 100), 0);

    // Call pinMode on one of the PWM pins - should release the channel
    fl::pinMode(33, fl::PinMode::Output);

    // Now 9th should succeed (channel was freed by pinMode)
    FL_REQUIRE_EQ(fl::setPwmFrequency(99, 100), 0);

    // Cleanup all
    for (int i = 0; i < 8; i++) {
        fl::pwmEnd(30 + i);
    }
    fl::pwmEnd(99);
}

FL_TEST_CASE("pwm_pinmode_different_modes") {
    // Test that all pinMode calls release PWM
    FL_REQUIRE_EQ(fl::setPwmFrequency(40, 100), 0);
    fl::pinMode(40, fl::PinMode::Input);
    FL_REQUIRE_EQ(fl::getPwmFrequency(40), 0u);

    FL_REQUIRE_EQ(fl::setPwmFrequency(40, 100), 0);
    fl::pinMode(40, fl::PinMode::InputPullup);
    FL_REQUIRE_EQ(fl::getPwmFrequency(40), 0u);

    FL_REQUIRE_EQ(fl::setPwmFrequency(40, 100), 0);
    fl::pinMode(40, fl::PinMode::InputPulldown);
    FL_REQUIRE_EQ(fl::getPwmFrequency(40), 0u);

    FL_REQUIRE_EQ(fl::setPwmFrequency(40, 100), 0);
    fl::pinMode(40, fl::PinMode::Output);
    FL_REQUIRE_EQ(fl::getPwmFrequency(40), 0u);
}

// ============================================================================
// digitalMultiWrite8 Tests
// ============================================================================

FL_TEST_CASE("digitalMultiWrite8_all_high") {
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    fl::u8 data[] = {0xFF};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 1));
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite8_all_low") {
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    fl::u8 data[] = {0x00};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 1));
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
    }
}

FL_TEST_CASE("digitalMultiWrite8_alternating") {
    // 0xAA = 0b10101010 -> pins[1,3,5,7] high, pins[0,2,4,6] low
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    fl::u8 data[] = {0xAA};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 1));
    FL_REQUIRE_EQ(fl::digitalRead(40), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(41), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(42), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(43), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(44), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(45), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(46), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(47), fl::PinValue::High);
}

FL_TEST_CASE("digitalMultiWrite8_multiple_bytes") {
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    fl::u8 data[] = {0xAA, 0x55};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 2));
    // Final state reflects the last byte (0x55 = 0b01010101)
    FL_REQUIRE_EQ(fl::digitalRead(40), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(41), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(42), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(43), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(44), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(45), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(46), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(47), fl::PinValue::Low);
}

FL_TEST_CASE("digitalMultiWrite8_empty_data") {
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>());
}

FL_TEST_CASE("digitalMultiWrite8_skip_pins") {
    fl::digitalWrite(50, fl::PinValue::Low);
    fl::digitalWrite(57, fl::PinValue::Low);
    fl::Pins8 pins = {{50, -1, -1, -1, -1, -1, -1, 57}};
    fl::u8 data[] = {0x81}; // bits 0 and 7 set
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 1));
    FL_REQUIRE_EQ(fl::digitalRead(50), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(57), fl::PinValue::High);
}

FL_TEST_CASE("digitalMultiWrite8_all_skipped") {
    fl::Pins8 pins = {{-1, -1, -1, -1, -1, -1, -1, -1}};
    fl::u8 data[] = {0xFF};
    fl::digitalMultiWrite8(pins, fl::span<const fl::u8>(data, 1));
}

// ============================================================================
// writeByte() Tests
// ============================================================================

FL_TEST_CASE("digitalMultiWrite8_writeByte_all_high") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    writer.init(pins);
    writer.writeByte(0xFF);
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite8_writeByte_all_low") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    writer.init(pins);
    writer.writeByte(0xFF);  // set high first
    writer.writeByte(0x00);  // then clear
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
    }
}

FL_TEST_CASE("digitalMultiWrite8_writeByte_alternating") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    writer.init(pins);
    writer.writeByte(0xAA);  // 0b10101010
    FL_REQUIRE_EQ(fl::digitalRead(40), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(41), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(42), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(43), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(44), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(45), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(46), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(47), fl::PinValue::High);
}

FL_TEST_CASE("digitalMultiWrite8_writeByte_skip_pins") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{50, -1, -1, -1, -1, -1, -1, 57}};
    fl::digitalWrite(50, fl::PinValue::Low);
    fl::digitalWrite(57, fl::PinValue::Low);
    writer.init(pins);
    writer.writeByte(0x81);  // bits 0 and 7 set
    FL_REQUIRE_EQ(fl::digitalRead(50), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(57), fl::PinValue::High);
}

FL_TEST_CASE("digitalMultiWrite8_writeByte_matches_write") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{40, 41, 42, 43, 44, 45, 46, 47}};
    writer.init(pins);

    // Use writeByte
    writer.writeByte(0xAA);
    fl::PinValue wb_results[8];
    for (int i = 0; i < 8; ++i) {
        wb_results[i] = fl::digitalRead(40 + i);
    }

    // Reset pins
    for (int i = 0; i < 8; ++i) {
        fl::digitalWrite(40 + i, fl::PinValue::Low);
    }

    // Use write(span)
    fl::u8 data[] = {0xAA};
    writer.write(fl::span<const fl::u8>(data, 1));

    // Both methods produce identical results
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), wb_results[i]);
    }
}

// ============================================================================
// allSamePort() and auto-disable Tests
// ============================================================================
// On stub platform, FastPin<N>::port() returns nullptr for all pins, so
// pinToPort() falls back to pin / 32 grouping. Pins 0-31 are port 0,
// pins 32-63 are port 1, etc.

FL_TEST_CASE("digitalMultiWrite8_same_port") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{0, 1, 2, 3, 4, 5, 6, 7}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());

    fl::u8 data[] = {0xFF};
    writer.write(fl::span<const fl::u8>(data, 1));
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite8_same_port_with_skips") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{32, -1, -1, -1, -1, -1, -1, 63}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());
}

FL_TEST_CASE("digitalMultiWrite8_minority_port_disabled") {
    // 5 pins on port 0 (0-4), 3 pins on port 1 (32-34)
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{0, 1, 2, 3, 4, 32, 33, 34}};
    writer.init(pins);

    FL_REQUIRE(writer.allSamePort());

    for (int i = 0; i < 5; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }
    for (int i = 32; i < 35; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }

    fl::u8 data[] = {0xFF};
    writer.write(fl::span<const fl::u8>(data, 1));
    FL_REQUIRE_EQ(fl::digitalRead(0), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(1), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(2), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(3), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(4), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(32), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(33), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(34), fl::PinValue::Low);
}

FL_TEST_CASE("digitalMultiWrite8_tie_goes_to_first_port") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{0, 1, 2, 3, 32, 33, 34, 35}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());

    for (int i = 0; i < 4; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }
    for (int i = 32; i < 36; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }

    fl::u8 data[] = {0xFF};
    writer.write(fl::span<const fl::u8>(data, 1));

    FL_REQUIRE_EQ(fl::digitalRead(0), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(1), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(2), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(3), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(32), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(33), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(34), fl::PinValue::Low);
    FL_REQUIRE_EQ(fl::digitalRead(35), fl::PinValue::Low);
}

FL_TEST_CASE("digitalMultiWrite8_all_skipped_same_port") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{-1, -1, -1, -1, -1, -1, -1, -1}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());
}

FL_TEST_CASE("digitalMultiWrite8_single_active_pin") {
    fl::DigitalMultiWrite8 writer;
    fl::Pins8 pins = {{-1, -1, -1, 20, -1, -1, -1, -1}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());
}

// ============================================================================
// pinToPort Tests
// ============================================================================

// ============================================================================
// digitalMultiWrite16 Tests
// ============================================================================

FL_TEST_CASE("digitalMultiWrite16_all_high") {
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::u16 data[] = {0xFFFF};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite16_all_low") {
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::u16 data[] = {0x0000};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
    }
}

FL_TEST_CASE("digitalMultiWrite16_alternating") {
    // 0xAAAA = 0b1010101010101010 -> odd bits high, even bits low
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::u16 data[] = {0xAAAA};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
    for (int i = 0; i < 16; ++i) {
        if (i & 1) {
            FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
        } else {
            FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
        }
    }
}

FL_TEST_CASE("digitalMultiWrite16_multiple_words") {
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::u16 data[] = {0xAAAA, 0x5555};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 2));
    // Final state reflects the last word (0x5555 = 0b0101010101010101)
    for (int i = 0; i < 16; ++i) {
        if (i & 1) {
            FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
        } else {
            FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
        }
    }
}

FL_TEST_CASE("digitalMultiWrite16_empty_data") {
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>());
}

FL_TEST_CASE("digitalMultiWrite16_skip_pins") {
    fl::digitalWrite(40, fl::PinValue::Low);
    fl::digitalWrite(55, fl::PinValue::Low);
    fl::Pins16 pins = {{40, -1, -1, -1, -1, -1, -1, -1,
                         -1, -1, -1, -1, -1, -1, -1, 55}};
    fl::u16 data[] = {0x8001}; // bits 0 and 15 set
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
    FL_REQUIRE_EQ(fl::digitalRead(40), fl::PinValue::High);
    FL_REQUIRE_EQ(fl::digitalRead(55), fl::PinValue::High);
}

FL_TEST_CASE("digitalMultiWrite16_all_skipped") {
    fl::Pins16 pins = {{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}};
    fl::u16 data[] = {0xFFFF};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
}

FL_TEST_CASE("digitalMultiWrite16_high_byte_only") {
    // 0xFF00 = upper 8 bits set, lower 8 bits clear
    fl::Pins16 pins = {{40, 41, 42, 43, 44, 45, 46, 47,
                         48, 49, 50, 51, 52, 53, 54, 55}};
    fl::u16 data[] = {0xFF00};
    fl::digitalMultiWrite16(pins, fl::span<const fl::u16>(data, 1));
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::Low);
    }
    for (int i = 8; i < 16; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(40 + i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite16_same_port") {
    fl::DigitalMultiWrite16 writer;
    fl::Pins16 pins = {{0, 1, 2, 3, 4, 5, 6, 7,
                         8, 9, 10, 11, 12, 13, 14, 15}};
    writer.init(pins);
    FL_REQUIRE(writer.allSamePort());

    fl::u16 data[] = {0xFFFF};
    writer.write(fl::span<const fl::u16>(data, 1));
    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(i), fl::PinValue::High);
    }
}

FL_TEST_CASE("digitalMultiWrite16_minority_port_disabled") {
    // 10 pins on port 0 (0-9), 6 pins on port 1 (32-37)
    fl::DigitalMultiWrite16 writer;
    fl::Pins16 pins = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                         32, 33, 34, 35, 36, 37}};
    writer.init(pins);

    FL_REQUIRE(writer.allSamePort());

    for (int i = 0; i < 10; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }
    for (int i = 32; i < 38; ++i) {
        fl::digitalWrite(i, fl::PinValue::Low);
    }

    fl::u16 data[] = {0xFFFF};
    writer.write(fl::span<const fl::u16>(data, 1));
    // Port 0 pins should be high
    for (int i = 0; i < 10; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(i), fl::PinValue::High);
    }
    // Port 1 pins should still be low (disabled)
    for (int i = 32; i < 38; ++i) {
        FL_REQUIRE_EQ(fl::digitalRead(i), fl::PinValue::Low);
    }
}

FL_TEST_CASE("pinToPort_same_port_group") {
    // On stub, pins 0-31 should all return the same port ID
    int port0 = fl::pinToPort(0);
    FL_REQUIRE(port0 >= 0);
    for (int i = 1; i < 32; ++i) {
        FL_REQUIRE_EQ(fl::pinToPort(i), port0);
    }
}

FL_TEST_CASE("pinToPort_negative_pin") {
    FL_REQUIRE_EQ(fl::pinToPort(-1), -1);
}

// ============================================================================
// pinMap Tests
// ============================================================================

FL_TEST_CASE("pinMap_resolves_ports") {
    fl::PinInfo infos[3];
    infos[0].pin = 0;
    infos[1].pin = 1;
    infos[2].pin = 32;
    fl::pinMap(fl::span<fl::PinInfo>(infos, 3));
    // Pins 0 and 1 are on the same port (stub: pin/32 = 0)
    FL_REQUIRE_EQ(infos[0].port, infos[1].port);
    // Pin 32 is on a different port (stub: pin/32 = 1)
    FL_REQUIRE(infos[2].port != infos[0].port);
}

FL_TEST_CASE("pinMap_skips_negative_pins") {
    fl::PinInfo infos[2];
    infos[0].pin = -1;
    infos[1].pin = 5;
    fl::pinMap(fl::span<fl::PinInfo>(infos, 2));
    FL_REQUIRE_EQ(infos[0].port, -1);
    FL_REQUIRE(infos[1].port >= 0);
}

FL_TEST_CASE("pinMap_empty_span") {
    fl::pinMap(fl::span<fl::PinInfo>());
}

} // FL_TEST_FILE

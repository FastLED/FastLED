/// @file tests/fl/pin.cpp
/// Unit tests for fl::pin API (GPIO and unified PWM frequency)
/// Implementation is in src/fl/pin.cpp.hpp

#include "fl/pin.h"
#include "fl/fltest.h"

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

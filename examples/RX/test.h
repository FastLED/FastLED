#pragma once

#include "fl/warn.h"
#include "fl/rx_device.h"

/**
 * @brief Pin toggle instruction for RX device testing
 */
struct PinToggle {
    bool is_high;       // Pin state (HIGH or LOW)
    uint32_t delay_us;  // Delay in microseconds after setting state
};

/**
 * @brief Verify jumper wire connection between TX and RX pins
 *
 * Tests that a physical jumper wire correctly connects the TX and RX pins
 * by setting TX HIGH/LOW and verifying RX reads the same values.
 *
 * @param pin_tx TX pin number (output pin)
 * @param pin_rx RX pin number (input pin)
 * @return true if jumper wire is working correctly, false otherwise
 */
bool verifyJumperWire(int pin_tx, int pin_rx);

/**
 * @brief Test RX device functionality with low-frequency pattern
 *
 * Validates the given RX device can capture edge transitions by generating
 * a simple test pattern (HIGH/LOW toggles) and verifying the captured
 * timing data matches expectations.
 *
 * @param rx Shared pointer to the RX device to test
 * @param pin_tx TX pin number to toggle
 * @return true if RX device captures expected edges, false otherwise
 */
bool testRxDevice(fl::shared_ptr<fl::RxDevice> rx, int pin_tx);

/**
 * @brief Execute pin toggles and initialize RX device for capture
 *
 * Configures the RX device with the given config, sets the TX pin to the
 * initial state, begins capture, and executes the sequence of pin toggles.
 *
 * @param rx RX device to initialize and use for capture
 * @param config Configuration for the RX device
 * @param toggles Sequence of pin state changes to execute
 * @param pin_tx TX pin number to toggle
 * @param wait_ms Unused parameter (kept for API compatibility)
 */
void executeToggles(fl::RxDevice& rx,
                    const fl::RxConfig& config,
                    fl::span<const PinToggle> toggles,
                    int pin_tx,
                    uint32_t wait_ms);

/**
 * @brief Validate captured edge timings against expected pattern
 *
 * Prints edge timing data, validates that edges alternate HIGH/LOW correctly,
 * and checks that timing values match the expected pattern within tolerance.
 *
 * @param edges Captured edge timing data
 * @param edge_count Number of edges captured
 * @param expected_pattern Expected pin toggle pattern to validate against
 * @param tolerance_percent Tolerance for timing validation (e.g., 15 for ±15%)
 * @return true if validation passes (edges alternate correctly and timing matches), false otherwise
 */
bool validateEdgeTiming(fl::span<const fl::EdgeTime> edges,
                        size_t edge_count,
                        fl::span<const PinToggle> expected_pattern,
                        uint32_t tolerance_percent);

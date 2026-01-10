/// @file rx_device.h
/// @brief Common interface for RX devices (cross-platform)

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/optional.h"
#include "fl/result.h"
#include "fl/stl/new.h"
#include "fl/slice.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"


namespace fl {

// Forward declarations
struct ChipsetTiming;

/**
 * @brief Universal edge timing representation (platform-agnostic)
 *
 * Represents a single edge transition with duration in nanoseconds.
 * RX devices convert their internal format (e.g., RMT ticks) to this
 * universal format for debugging and analysis.
 *
 * Memory layout: 32-bit packed bit field
 * - 31 bits: Duration in nanoseconds (max 2147483647ns ~= 2.1 seconds)
 * - 1 bit: High/low level flag
 *
 * Example sequence for WS2812B bit pattern:
 * - Bit 0: {high: true, ns: 400}, {high: false, ns: 850}
 * - Bit 1: {high: true, ns: 800}, {high: false, ns: 450}
 */
struct EdgeTime {
    uint32_t ns : 31;    ///< Duration in nanoseconds (31 bits, max ~2.1s)
    uint32_t high : 1;   ///< High/low level (1 bit: 1=high, 0=low)

    /// Default constructor (low, 0ns)
    constexpr EdgeTime() : ns(0), high(0) {}

    /// Construct from high/low state and duration
    constexpr EdgeTime(bool high_level, uint32_t ns_duration)
        : ns(ns_duration), high(high_level ? 1 : 0) {}
};

/**
 * @brief Edge range specification for getRawEdgeTimes() debugging
 *
 * Specifies a range of edges to extract from the RX buffer.
 * Used for printing edge timing context around specific locations.
 *
 * Example:
 * - EdgeRange{0, 256} = first 256 edges
 * - EdgeRange{100, 20} = 20 edges starting at offset 100
 */
struct EdgeRange {
    size_t offset;  ///< Starting edge index
    size_t count;   ///< Number of edges to extract

    /// Constructor
    constexpr EdgeRange(size_t offsetParam, size_t countParam)
        : offset(offsetParam), count(countParam) {}
};

/**
 * @brief Error codes for RX decoder operations
 */
enum class DecodeError : uint8_t {
    OK = 0,                  ///< No error (not typically used)
    HIGH_ERROR_RATE,         ///< Symbol decode error rate too high (>10%)
    BUFFER_OVERFLOW,         ///< Output buffer overflow
    INVALID_ARGUMENT         ///< Invalid input arguments
};

/**
 * @brief 4-phase RX timing thresholds for chipset detection
 *
 * Defines acceptable timing ranges for decoding symbols back to bits.
 * Uses min/max ranges to tolerate signal jitter and hardware variations.
 *
 * Thresholds should be ±150ns wider than nominal TX timing to account for:
 * - Clock drift between TX and RX
 * - Signal propagation delays
 * - LED capacitance effects
 * - GPIO sampling jitter
 */
struct ChipsetTiming4Phase {
    // Bit 0 timing thresholds
    uint32_t t0h_min_ns; ///< Bit 0 high time minimum (e.g., 250ns)
    uint32_t t0h_max_ns; ///< Bit 0 high time maximum (e.g., 550ns)
    uint32_t t0l_min_ns; ///< Bit 0 low time minimum (e.g., 700ns)
    uint32_t t0l_max_ns; ///< Bit 0 low time maximum (e.g., 1000ns)

    // Bit 1 timing thresholds
    uint32_t t1h_min_ns; ///< Bit 1 high time minimum (e.g., 650ns)
    uint32_t t1h_max_ns; ///< Bit 1 high time maximum (e.g., 950ns)
    uint32_t t1l_min_ns; ///< Bit 1 low time minimum (e.g., 300ns)
    uint32_t t1l_max_ns; ///< Bit 1 low time maximum (e.g., 600ns)

    // Reset pulse threshold
    uint32_t reset_min_us; ///< Reset pulse minimum duration (e.g., 50us)

    // Gap tolerance (optional, for handling transmission gaps like PARLIO DMA gaps)
    uint32_t gap_tolerance_ns = 0; ///< Maximum gap duration to tolerate (0 = no gap tolerance, treat as error)
                                    ///< Pulses longer than reset_min_us but shorter than gap_tolerance_ns
                                    ///< are skipped during decoding without triggering errors.
                                    ///< Useful for PARLIO ~20us DMA gaps between frames.
};

/**
 * @brief Create 4-phase RX timing from 3-phase chipset timing with tolerance
 *
 * Converts FastLED's 3-phase timing (T1, T2, T3) to 4-phase RX timing with
 * configurable tolerance for signal variations.
 *
 * 3-phase encoding (FastLED chipset timing):
 * - T1: High time for bit 0
 * - T2: Additional high time for bit 1 (T1H = T1 + T2)
 * - T3: Low time for bit 1 (T1L = T3)
 * Note: Bit 0 low time is derived as T0L = T2 + T3
 *
 * 4-phase decoding (actual encoder output):
 * - Bit 0: T0H (T1 high) + T0L ((T2+T3) low)
 * - Bit 1: T1H ((T1+T2) high) + T1L (T3 low)
 *
 * 4-phase decoding thresholds with tolerance:
 * - T0H: [T1 - tolerance_ns, T1 + tolerance_ns]
 * - T0L: [(T2+T3) - tolerance_ns, (T2+T3) + tolerance_ns]
 * - T1H: [(T1+T2) - tolerance_ns, (T1+T2) + tolerance_ns]
 * - T1L: [T3 - tolerance_ns, T3 + tolerance_ns]
 *
 * @param timing_3phase 3-phase chipset timing configuration
 * @param tolerance_ns Tolerance for all timings (default: 150ns, accounts for jitter/drift)
 * @return ChipsetTiming4Phase 4-phase RX timing structure
 *
 * Example:
 * @code
 * ChipsetTiming ws2812b_tx{250, 625, 375, 280, "WS2812B"};
 * auto rx_timing = make4PhaseTiming(ws2812b_tx, 150);
 * // Results in:
 * // T0H: [100ns, 400ns], T0L: [850ns, 1150ns]
 * // T1H: [725ns, 1025ns], T1L: [225ns, 525ns]
 * @endcode
 */
ChipsetTiming4Phase make4PhaseTiming(const ChipsetTiming& timing_3phase,
                                      uint32_t tolerance_ns = 150);

/**
 * @brief Result codes for RX wait() operations
 */
enum class RxWaitResult : uint8_t {
    SUCCESS = 0,             ///< Operation completed successfully
    TIMEOUT = 1,             ///< Operation timed out
    BUFFER_OVERFLOW = 2      ///< Buffer overflow
};

/**
 * @brief RX device type enumeration
 *
 * Defines available RX device implementations. Used with template-based
 * factory pattern for compile-time device selection.
 */
enum class RxDeviceType : uint8_t {
    ISR = 0,  ///< GPIO ISR-based receiver (ESP32)
    RMT = 1   ///< RMT-based receiver (ESP32)
};

/**
 * @brief Convert RxDeviceType to human-readable string
 * @param type RX device type
 * @return String name ("ISR" or "RMT")
 */
inline const char* toString(RxDeviceType type) {
    return type == RxDeviceType::RMT ? "RMT" : "ISR";
}

/**
 * @brief Configuration for RX device initialization
 *
 * Struct-based configuration allows future extensibility without breaking API compatibility.
 *
 * Hardware Parameters:
 * - buffer_size: Buffer size (symbols for RMT, edges for ISR)
 * - hz: Optional clock frequency (only used for RMT, defaults to platform default: 40MHz on ESP32)
 *
 * Edge Detection:
 * The edge detection feature solves the "spurious LOW capture" problem where RX devices
 * capture the idle pin state (LOW) before TX starts transmitting. By detecting the first
 * rising edge (LOW→HIGH) or falling edge (HIGH→LOW), we can skip pre-transmission noise
 * and start decoding from actual data.
 *
 * Example:
 * @code
 * RxConfig config;
 * config.buffer_size = 512;           // Buffer size
 * config.hz = 1000000;                // Optional: 1MHz clock (RMT only)
 * config.signal_range_min_ns = 100;
 * config.signal_range_max_ns = 100000;
 * config.skip_signals = 0;
 * config.start_low = true;  // Pin idle state is LOW (WS2812B default)
 * rx->begin(config);
 * @endcode
 */
struct RxConfig {
    // Hardware parameters
    size_t buffer_size = 512;               ///< Buffer size in symbols/edges (default: 512)
    fl::optional<uint32_t> hz = fl::nullopt; ///< Optional clock frequency (RMT only, default: 40MHz)

    // Signal detection parameters
    uint32_t signal_range_min_ns = 100;     ///< Minimum pulse width (glitch filter, default: 100ns)
    uint32_t signal_range_max_ns = 100000;  ///< Maximum pulse width (idle threshold, default: 100μs)
    uint32_t skip_signals = 0;              ///< Number of signals to skip before capturing (default: 0)
    bool start_low = true;                  ///< Pin idle state: true=LOW (WS2812B), false=HIGH (inverted)

    /// Default constructor with common WS2812B defaults
    constexpr RxConfig() = default;
};

/**
 * @brief Common interface for RX devices
 *
 * Provides a unified interface for platform-specific receivers:
 * - ESP32: RMT and GPIO ISR-based receivers
 * - Other platforms: Future implementations
 */
class RxDevice {


public:

    /**
     * @brief Template factory method to create RX device by type
     * @tparam TYPE Device type from RxDeviceType enum
     * @param pin GPIO pin number for receiving signals
     * @return Shared pointer to RxDevice
     *
     * Platform-specific implementations (e.g., ESP32) provide explicit
     * template specializations for ISR and RMT types in rx_device.cpp.
     * On unsupported platforms, returns a DummyRxDevice.
     *
     * Hardware parameters (buffer_size, hz) are passed via RxConfig in begin().
     *
     * Example:
     * @code
     * auto rx = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 512;
     * config.hz = 1000000;  // Optional: 1MHz clock
     * rx->begin(config);
     * @endcode
     */
    template <RxDeviceType TYPE>
    static fl::shared_ptr<RxDevice> create(int pin);

    /**
     * @brief Initialize (or re-arm) RX channel with configuration
     * @param config RX configuration (signal ranges, edge detection, skip count)
     * @return true on success, false on failure
     *
     * First call: Initializes hardware and arms receiver
     * Subsequent calls: Re-arms receiver for new capture (clears state)
     *
     * Edge Detection:
     * - start_low=true: Skip symbols until first rising edge (LOW→HIGH), default for WS2812B
     * - start_low=false: Skip symbols until first falling edge (HIGH→LOW), for inverted signals
     *
     * This solves the "spurious LOW capture" problem where RX captures the idle pin state
     * before TX starts transmitting.
     *
     * Example:
     * @code
     * RxConfig config;
     * config.signal_range_min_ns = 100;   // Glitch filter
     * config.signal_range_max_ns = 100000; // Idle timeout
     * config.start_low = true;             // WS2812B idle = LOW
     * rx->begin(config);
     * @endcode
     */
    virtual bool begin(const RxConfig& config) = 0;


    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool finished() const = 0;

    /**
     * @brief Wait for data with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return RxWaitResult - SUCCESS, TIMEOUT, or BUFFER_OVERFLOW
     */
    virtual RxWaitResult wait(uint32_t timeout_ms) = 0;

    /**
     * @brief Decode captured data to bytes into a span
     * @param timing Chipset timing thresholds for bit detection
     * @param out Output span to write decoded bytes
     * @return Result with total bytes decoded, or error
     */
    virtual fl::Result<uint32_t, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                                       fl::span<uint8_t> out) = 0;

    /**
     * @brief Get raw edge timings in universal format (for debugging)
     * @param out Output span to write EdgeTime entries (span.size() = max edges to read)
     * @param offset Starting edge index to read from (default: 0)
     * @return Number of EdgeTime entries written (may be less than out.size() if insufficient data)
     *
     * Converts internal platform-specific format (RMT ticks, ISR timestamps, etc.)
     * to universal EdgeTime format with nanosecond durations.
     *
     * For RMT devices: Each RMT symbol produces 2 EdgeTime entries (high/low phases)
     *
     * Example:
     * @code
     * auto rx = RxDevice::create<RxDeviceType::RMT>(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 512;
     * rx->begin(config);
     * rx->wait(100);
     *
     * // Get first 100 edges
     * EdgeTime edges[100];
     * size_t count = rx->getRawEdgeTimes(edges);  // offset defaults to 0
     *
     * // Get 10 edges starting at offset 50
     * EdgeTime edges[10];
     * size_t count = rx->getRawEdgeTimes(edges, 50);
     * @endcode
     */
    virtual size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) = 0;

    /**
     * @brief Get device type name
     * @return Device name (e.g., "dummy", "RMT", "ISR")
     */
    virtual const char* name() const = 0;

    /**
     * @brief Get GPIO pin number
     * @return Pin number this device is listening on
     */
    virtual int getPin() const = 0;

    /**
     * @brief Manually inject edge timings for testing (Phase 1 - PARLIO gap simulation)
     * @param edges Span of EdgeTime entries to inject (nanosecond timings)
     * @return true on success, false if not supported or failed
     *
     * This method allows programmatic injection of timing data into RX devices
     * for testing decoder behavior without actual hardware transmission. Primary
     * use case is simulating PARLIO transmission gaps (~20us LOW pulses) to
     * validate gap tolerance in LED decoders.
     *
     * IMPORTANT: EdgeTime entries must use nanosecond timings, NOT ticks.
     * - RMT devices: Will store edges as-is in internal buffer (no tick conversion)
     * - GPIO ISR devices: Will store edges directly in timestamp buffer
     *
     * After injection, use decode() to process the injected edges as if they
     * were captured from hardware.
     *
     * Example (simulate WS2812B with 20us gap):
     * @code
     * auto rx = RxDevice::create<RxDeviceType::RMT>(1);
     * rx->begin(config);
     *
     * // Generate EdgeTime sequence for 3 LEDs
     * fl::EdgeTime edges[144];  // 3 LEDs × 24 bits × 2 edges/bit
     * size_t count = generateLedEdges(leds, 3, edges);
     *
     * // Insert 20us gap (extend last LOW pulse)
     * edges[count-1].ns += 20000;  // Add 20us to last LOW
     *
     * // Inject edges into RX device
     * if (rx->injectEdges(edges, count)) {
     *     // Decode injected edges
     *     uint8_t buffer[9];  // 3 LEDs × 3 bytes
     *     auto result = rx->decode(timing, buffer);
     * }
     * @endcode
     *
     * @note Not all RX devices support injection. DummyRxDevice returns false.
     */
    virtual bool injectEdges(fl::span<const EdgeTime> edges) = 0;


protected:
    // Allow shared_ptr to access protected destructor
    friend class fl::shared_ptr<RxDevice>;
    RxDevice() = default;
    virtual ~RxDevice() = default;


private:
    /**
     * @brief Create dummy RxDevice instance (default fallback)
     * @return Shared pointer to DummyRxDevice
     *
     * Used by default template implementation when no platform-specific
     * specialization exists. Returns singleton DummyRxDevice instance.
     */
    static fl::shared_ptr<RxDevice> createDummy();
};

} // namespace fl

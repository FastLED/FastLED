/// @file channel_driver_rmt4.h
/// @brief RMT4 interface for ChannelEngine (ESP32 IDF 4.x)
///
/// This header defines both the public interface and implementation for RMT4
/// ChannelEngine. The SDK owns the shared RX/TX ISR and invokes an IRAM
/// byte translator for buffered asynchronous output.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

// Skip entirely if platform has no RMT hardware (ESP32-C2)
#if !FASTLED_ESP32_HAS_RMT
// No RMT hardware available
#elif FASTLED_ESP32_RMT5_ONLY_PLATFORM

// Detect misconfiguration: RMT5-only chip with FASTLED_RMT5=0
#if FASTLED_RMT5 == 0
#error                                                                         \
    "ESP32-C6/C5/P4/H2 only support RMT5 (new driver). These chips do not have RMT4 hardware. Remove -DFASTLED_RMT5=0 from build flags or set FASTLED_RMT5=1."
#endif

// Skip RMT4 implementation for RMT5-only chips
#elif !FASTLED_RMT5 // Only compile for RMT4 (IDF 4.x)

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/atomic.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "platforms/esp/32/core/clock_cycles.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/irmt4_peripheral.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_config.h"

FL_EXTERN_C_BEGIN
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR > 3
#include "esp_intr_alloc.h"
#else
#include "esp_intr.h"
#endif
// IWYU pragma: begin_keep
#include "driver/gpio.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "driver/rmt.h"
// IWYU pragma: end_keep
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
// IWYU pragma: begin_keep
#include "esp_private/periph_ctrl.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/gpio_periph.h"
// IWYU pragma: end_keep
#define gpio_matrix_out esp_rom_gpio_connect_out_signal
#else
// IWYU pragma: begin_keep
#include "driver/periph_ctrl.h"
// IWYU pragma: end_keep
#endif
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/semphr.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/rmt_struct.h"

// Flash lock support (optional feature)
// IWYU pragma: end_keep
#if FASTLED_ESP32_FLASH_LOCK == 1
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "esp_flash.h"
#else
#include "esp_spi_flash.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#endif
#endif

FL_EXTERN_C_END

// ═══════════════════════════════════════════════════════════════════════════
// Platform-specific RMT memory configuration
// ═══════════════════════════════════════════════════════════════════════════

// 64 for ESP32, ESP32-S2
// 48 for ESP32-S3, ESP32-C3, ESP32-H2, ESP32-C6
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#else
// ESP32 value (only chip variant supported on older IDF)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 64
#endif
#endif

// By default use two memory blocks for each RMT channel instead of 1.
#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2
#endif

#define MAX_PULSES_RMT4                                                        \
    (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define PULSES_PER_FILL_RMT4 (MAX_PULSES_RMT4 / 2)

// Clock divider
#define DIVIDER_RMT4 2

// Default transmission timeout (milliseconds)
// Set to 0 to disable timeout detection
#ifndef FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS
#define FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS 2000
#endif

// Flash lock configuration (default: disabled)
#ifndef FASTLED_ESP32_FLASH_LOCK
#define FASTLED_ESP32_FLASH_LOCK 0
#endif

// Max RMT TX channels (platform-specific)
#ifndef FASTLED_RMT_MAX_CHANNELS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FASTLED_RMT_MAX_CHANNELS SOC_RMT_TX_CANDIDATES_PER_GROUP
#else
#ifdef FL_IS_ESP_32S2
#define FASTLED_RMT_MAX_CHANNELS 4
#else
#define FASTLED_RMT_MAX_CHANNELS 8
#endif
#endif
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Timing Conversion Macros
// ═══════════════════════════════════════════════════════════════════════════

// Manual clock frequency override for ESP32-C6 and ESP32-H2 (40MHz vs 80MHz)
#ifndef F_CPU_RMT_CLOCK_MANUALLY_DEFINED
#if defined(FL_IS_ESP_32C6)
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#elif defined(FL_IS_ESP_32H2)
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#endif

#ifdef F_CPU_RMT_CLOCK_MANUALLY_DEFINED
#define F_CPU_RMT (F_CPU_RMT_CLOCK_MANUALLY_DEFINED)
#else
#define F_CPU_RMT (APB_CLK_FREQ)
#endif
#endif

// Convert nanoseconds to ESP32 CPU cycles
#define NS_TO_ESP_CYCLES(ns) (((ns) * (F_CPU / 1000000UL) + 500) / 1000)

// Convert ESP32 CPU cycles to RMT device cycles (accounting for divider)
#define RMT_CYCLES_PER_SEC (F_CPU_RMT / DIVIDER_RMT4)
#define RMT_CYCLES_PER_ESP_CYCLE (F_CPU / RMT_CYCLES_PER_SEC)
#define ESP_TO_RMT_CYCLES(n) ((n) / (RMT_CYCLES_PER_ESP_CYCLE))

namespace fl {

struct Rmt4TxDiagnostics {
    u32 translatedBytes = 0;
    u32 cumulativeTranslatedBytes = 0;
    u32 latchChecks = 0;
    u32 latchViolations = 0;
    u32 minLatchHeldUs = 0;
    u32 maxRequiredResetUs = 0;
    u32 lateAborts = 0;
    u32 maxDeltaCycles = 0;
    u32 abortDeltaCycles = 0;
    u32 allowedDeltaCycles = 0;
    u32 preloadCycles = 0;
    u32 firstIsrCycles = 0;
    u32 postStartCycles = 0;
    u32 postStartUs = 0;
    u32 firstIsrUs = 0;
    u32 maxDeltaUs = 0;
    u32 abortDeltaUs = 0;
    u32 allowedDeltaUs = 0;
    int preloadCore = -1;
    int isrCore = -1;
};

// Forward declarations
class ChannelEngineRMT4;
FASTLED_SHARED_PTR(ChannelEngineRMT4);

/// @brief RMT4-based ChannelEngine interface
///
/// Factory-based interface for RMT4 hardware driver.
/// Use create() to instantiate the concrete implementation.
///
/// Key Features:
/// - SDK-managed ISR-driven refill with bounded symbol buffers
/// - Time-multiplexing support (>8 strips via channel sharing)
/// - Direct hardware memory access for performance
/// - Lifecycle calls routed through `IRMT4Peripheral` (#3458)
class ChannelEngineRMT4 : public IChannelDriver {
  public:
    /// @brief Create RMT4 driver instance with the default real-hardware
    ///        peripheral.
    /// @return Shared pointer to driver implementation
    static ChannelEngineRMT4Ptr create() FL_NO_EXCEPT;

    /// @brief Create RMT4 driver instance with an injected peripheral
    ///        (test seam — pass a mock to drive the engine without
    ///        touching real ESP-IDF APIs).
    static ChannelEngineRMT4Ptr
    create(fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT;

    virtual ~ChannelEngineRMT4() = default;

    /// @brief Check if driver can handle channel data (clockless only)
    /// @param data Channel data to check
    /// @return true if clockless channel (rejects SPI), false otherwise
    virtual bool canHandle(const ChannelDataPtr &data) const FL_NO_EXCEPT = 0;

    /// @brief Get the driver name for affinity binding
    /// @return "RMT"
    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("RMT");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

  protected:
    ChannelEngineRMT4() = default;
};

// ═══════════════════════════════════════════════════════════════════════════
// Concrete Implementation Class
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Concrete RMT4 driver implementation
///
/// This class contains all implementation details including:
/// - Channel state tracking
/// - SDK-managed buffered transmission
/// - Source buffer lifetime management
/// - Time-multiplexing queue
class ChannelEngineRMT4Impl final : public ChannelEngineRMT4 {
  public:
    /// @brief Default constructor uses the real-hardware peripheral.
    ///        Kept for the legacy `idf4_clockless_rmt_esp32.h` path that
    ///        instantiates this class through `fl::make_shared` without
    ///        threading a peripheral through.
    ChannelEngineRMT4Impl() FL_NO_EXCEPT;

    /// @brief Inject a peripheral (real or mock). Production use is via
    ///        the factory; tests construct directly with a mock.
    explicit ChannelEngineRMT4Impl(
        fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT;

    ~ChannelEngineRMT4Impl() override;

    bool canHandle(const ChannelDataPtr &data) const FL_NO_EXCEPT override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;

    /// @brief Trigger transmission of enqueued data
    void show() FL_NO_EXCEPT override;

    /// @brief Query driver state (hardware polling implementation)
    DriverState poll() FL_NO_EXCEPT override;

    Rmt4TxDiagnostics txDiagnostics(int channel = -1) const FL_NO_EXCEPT;

  private:
    struct InternalSourceDeleter {
        void operator()(u8 *pointer) const FL_NO_EXCEPT;
    };
    using InternalSource = fl::unique_ptr<u8, InternalSourceDeleter>;

    struct ChannelState {
        rmt_channel_t channel = static_cast<rmt_channel_t>(0);
        gpio_num_t pin = static_cast<gpio_num_t>(0);
        u8 memBlocks = 0;
        bool configured = false;
        bool inUse = false;
        bool resetWaitStarted = false;
        u32 resetStartTimeUs = 0;
        u32 resetDurationUs = 0;
        u32 transmissionStartTime = 0;
        ChannelDataPtr sourceData;
        fl::shared_ptr<InternalSource> internalSource;
    };

    fl::vector_inlined<ChannelState, FASTLED_RMT_MAX_CHANNELS> mChannels;
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;
    fl::vector_inlined<ChannelDataPtr, 16> mPendingChannels;
    fl::shared_ptr<detail::IRMT4Peripheral> mPeripheral;
    bool mError = false;
    bool mFlashLocked = false;

    void lockFlash() FL_NO_EXCEPT;
    void unlockFlash() FL_NO_EXCEPT;

    ChannelState *acquireChannel(gpio_num_t pin,
                                const ChipsetTimingConfig &timing) FL_NO_EXCEPT;
    void releaseChannel(ChannelState *state) FL_NO_EXCEPT;
    void destroyChannel(ChannelState *state) FL_NO_EXCEPT;
    bool configureChannel(ChannelState *state, gpio_num_t pin,
                          const ChipsetTimingConfig &timing) FL_NO_EXCEPT;
    void processPendingChannels() FL_NO_EXCEPT;
    void clearPendingChannels() FL_NO_EXCEPT;
    void startTransmission(ChannelState *state,
                           const ChannelDataPtr &data) FL_NO_EXCEPT;
    static rmt_item32_t makeZeroSymbol(const ChipsetTimingConfig &timing) FL_NO_EXCEPT;
    static rmt_item32_t makeOneSymbol(const ChipsetTimingConfig &timing) FL_NO_EXCEPT;
};

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip

#endif // FL_IS_ESP32

FL_DISABLE_WARNING_POP

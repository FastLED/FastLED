/// @file manager.h
/// @brief Unified manager for channel drivers with priority-based fallback
///
/// The ChannelManager coordinates multiple channel drivers (e.g., PARLIO, SPI, RMT)
/// and automatically selects the best available driver based on priority.
/// It acts as a transparent proxy using the Proxy/Decorator pattern - strip drivers
/// interact with it through the ChannelDriver interface without knowing about
/// the manager's existence.
///
/// Design Pattern: Proxy/Decorator
/// - Manager IS A ChannelDriver (inheritance for polymorphic use)
/// - Forwards calls to active driver
/// - Handles fallback automatically
/// - Strip drivers unaware of manager's existence
///
/// Usage Pattern:
/// 1. Platform-specific code builds and configures manager with drivers
/// 2. Strip drivers obtain manager reference: mDriver = &ChannelManager::instance()
/// 3. Manager transparently selects best available driver
/// 4. Automatic fallback when driver allocation fails
/// 5. Per-frame reset allows driver re-evaluation

#pragma once

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/system/engine_events.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"
#include "platforms/channel_poll_signal.h"

namespace fl {

/// @brief Driver state information for channel manager
struct DriverInfo {
    fl::string name;  ///< Driver name (empty for unnamed drivers)
    int priority;     ///< Driver priority (higher = preferred)
    bool enabled;     ///< Whether driver is currently enabled
};

/// @brief Unified channel manager with priority-based driver selection
///
/// This manager acts as a registry for concrete driver implementations (RMT, SPI, PARLIO).
/// Channels select drivers via selectDriverForChannel() and bind directly to them via weak_ptr.
///
/// Platform-specific code registers drivers during static initialization.
class ChannelManager : public EngineEvents::Listener {
public:
    /// @brief Get the global singleton instance
    /// @return Reference to the singleton ChannelManager
    /// @note Thread-safe singleton initialization
    static ChannelManager& instance() FL_NOEXCEPT;

    /// @brief Constructor
    ChannelManager() FL_NOEXCEPT;

    /// @brief Destructor - cleanup shared drivers (automatic via shared_ptr)
    ~ChannelManager() FL_NOEXCEPT override;

    /// @brief Add a driver with priority (higher priority = preferred)
    /// @param priority Driver priority (higher values = higher priority)
    /// @param driver Shared driver implementation (allows caller to retain reference for testing)
    /// @note Platform-specific code calls this during static initialization
    /// @note Drivers are automatically sorted by priority on each insertion
    /// @note Driver name is obtained via driver->getName()
    /// @note If driver->getName() returns empty string, driver is rejected (emits FL_WARN and returns)
    /// @note If a driver with the same name already exists, it will be replaced:
    ///       1. FL_WARN emitted about replacement
    ///       2. All drivers polled until READY state (1 second timeout)
    ///       3. Old driver removed (shared_ptr may trigger deletion)
    ///       4. New driver added with specified priority
    void addDriver(int priority, fl::shared_ptr<IChannelDriver> driver) FL_NOEXCEPT;

    /// @brief Remove a driver from the manager
    /// @param driver Shared pointer to the driver to remove
    /// @return true if driver was found and removed, false if not found
    /// @note Emits FL_WARN if driver is not found
    /// @note Useful for test cleanup
    bool removeDriver(fl::shared_ptr<IChannelDriver> driver) FL_NOEXCEPT;

    /// @brief Remove all drivers from the manager
    /// @note Clears the entire driver registry
    /// @note Useful for FastLED.reset() with CHANNEL_DRIVERS flag
    /// @note Waits for all drivers to become READY before clearing (1 second timeout)
    void clearAllDrivers() FL_NOEXCEPT;

    /// @brief Enable or disable a driver by name at runtime
    /// @param name Driver name to control (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @param enabled true to enable, false to disable
    /// @note Disabled drivers are skipped during selection
    /// @note Changes take effect immediately on next enqueue()
    /// @note If name is not found, this is a no-op (does not warn)
    void setDriverEnabled(const char* name, bool enabled) FL_NOEXCEPT;

    /// @brief Register a single driver at a priority above the platform default
    ///        and disable all others (compile-time TU-linking variant).
    ///
    /// Post-#2428 the default build does NOT auto-register every driver --
    /// only the platform-default driver TU links (via the legacy clockless
    /// controller's Phase 5b pre-bind). This template provides the opt-in
    /// path to add another driver and have it win priority dispatch.
    ///
    /// Naming `BusTraits<B>::instancePtr()` here is the ODR-use that links
    /// the driver TU; the registration with the manager happens at
    /// `kExclusivePriority` (a value above any platform default) so the
    /// new driver wins in `selectDriverForChannel()`.
    ///
    /// @tparam B  Bus identifier (must have a `BusTraits<B>` specialization
    ///            visible at the call site).
    /// @note **Must be called before `addLeds<>` / `FastLED.add()`** so
    ///       newly-constructed channels see the override during driver
    ///       resolution. Legacy clockless controllers that already pre-bound
    ///       to the platform default via Phase 5b will continue to use that
    ///       pre-bind -- this template does not rewrite existing controllers.
    template<fl::Bus B>
    void setExclusiveDriver() FL_NOEXCEPT;

    /// @brief Enable only one driver exclusively, by `fl::Bus` enum (runtime form).
    /// @param bus Bus enum identifying the driver to enable (typed, typo-safe)
    /// @return true if driver was found and set as exclusive, false otherwise
    /// @note Typed overload — prefer this over the by-name variant for
    ///       built-in drivers. Delegates to `setExclusiveDriverByName(busName(bus))`.
    /// @note Does NOT ODR-use `BusTraits<bus>::instancePtr()` — for compile-time
    ///       TU-linking, use the `setExclusiveDriver<fl::Bus B>()` template overload.
    bool setExclusiveDriver(Bus bus) FL_NOEXCEPT;

    /// @brief Enable only one driver exclusively (disables all others) — by-name escape hatch
    /// @param name Driver name to enable exclusively (case-sensitive, e.g., "MOCK_SPI")
    /// @return true if driver was found and set as exclusive, false if name not found
    ///
    /// Test / mock helper: works with drivers that have ALREADY been registered
    /// via `addDriver()` (typically `ChannelEngineMock` instances under unit tests),
    /// or for drivers whose names are NOT in the `fl::Bus` enum (custom
    /// third-party drivers, RPC-resolved names). Does NOT link or register new
    /// drivers.
    ///
    /// For built-in drivers, prefer either:
    ///   - `setExclusiveDriver(fl::Bus)` (typed runtime form), or
    ///   - `setExclusiveDriver<fl::Bus B>()` (compile-time form that links the
    ///     driver TU via ODR-use).
    /// @note Atomically disables all drivers, then enables the specified one
    /// @note Changes take effect immediately on next enqueue()
    /// @note If name is not found, all drivers remain disabled (defensive behavior)
    /// @note Use nullptr or empty string to disable all drivers (returns false)
    /// @warning This will disable ALL other registered drivers, including future additions
    bool setExclusiveDriverByName(const char* name) FL_NOEXCEPT;

    /// @brief Change the priority of a registered driver
    /// @param name Driver name (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @param priority New priority value (higher = preferred, e.g., 9000 > 5000)
    /// @return true if driver was found and priority updated, false if name not found
    /// @note Triggers automatic re-sort of driver list by priority (descending)
    /// @note Changes take effect immediately on next enqueue()
    /// @note Higher priority drivers are selected first during canHandle() iteration
    bool setDriverPriority(const fl::string& name, int priority) FL_NOEXCEPT;

    /// @brief Check if a driver is enabled by name
    /// @param name Driver name to query (case-sensitive)
    /// @return true if enabled, false if disabled or not registered
    bool isDriverEnabled(const char* name) const FL_NOEXCEPT;

    /// @brief Registration status of a driver by name (silent lookup)
    /// @note Values are prefixed (`STATUS_*`) because Arduino-ESP32's
    ///       `esp32-hal-gpio.h` defines `#define DISABLED 0x00` for pinMode
    ///       and would textually rewrite the unprefixed labels at every call
    ///       site (`DriverStatus::DISABLED` -> `DriverStatus::0x00`).
    enum class DriverStatus {
        NOT_REGISTERED,    ///< No driver with that name is registered
        STATUS_DISABLED,   ///< Driver is registered but disabled
        STATUS_ENABLED,    ///< Driver is registered and enabled
    };

    /// @brief Look up a driver's registration status without logging on miss.
    /// @param name Driver name (case-sensitive)
    /// @return DriverStatus tri-state (NOT_REGISTERED / STATUS_DISABLED / STATUS_ENABLED)
    /// @note Companion to `findDriverByName()` — same silent-on-miss behavior.
    ///       Used by `Channel::showPixels()` to detect the #2517 silent-drop
    ///       case (enqueued data routed to a disabled / unregistered driver).
    DriverStatus driverStatus(const fl::string& name) const FL_NOEXCEPT;

    /// @brief Get the currently-active exclusive-driver selection (if any).
    /// @return Driver name set via `setExclusiveDriver(...)`, or empty string
    ///         if no exclusive mode is active.
    /// @note Diagnostic accessor — used by the #2517 silent-drop FL_ERROR to
    ///       name the current exclusive-driver setting in its remediation hint.
    const fl::string& exclusiveDriverName() const FL_NOEXCEPT { return mExclusiveDriver; }

    /// @brief Get count of registered drivers (including unnamed ones)
    /// @return Total number of registered drivers
    fl::size getDriverCount() const FL_NOEXCEPT;

    /// @brief Get full state of all registered drivers
    /// @return Span of driver info (sorted by priority descending)
    /// @note Returned span is valid until next call to any non-const method
    /// @note This method updates an internal cache - not thread-safe
    /// @note fl::string copies are cheap (shared pointer internally, no heap allocation)
    fl::span<const DriverInfo> getDriverInfos() const FL_NOEXCEPT;

    /// @brief Get driver by name for affinity binding
    /// @param name Engine name to look up (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @return Shared pointer to driver if found and enabled, nullptr otherwise
    /// @note Used by Channel affinity system to bind channels to specific drivers
    /// @note Logs `FL_ERROR` on miss — call `findDriverByName()` for silent lookup.
    fl::shared_ptr<IChannelDriver> getDriverByName(const fl::string& name) const FL_NOEXCEPT;

    /// @brief Silent counterpart to `getDriverByName()`.
    ///
    /// Same lookup, no logging on miss. Lets callers that handle the "missing
    /// driver" case themselves (e.g. `Channel::showPixels()`'s one-shot
    /// affinity-miss diagnostic from #2455) probe the registry without
    /// triggering the per-frame `FL_ERROR` stream that `getDriverByName()` emits.
    fl::shared_ptr<IChannelDriver> findDriverByName(const fl::string& name) const FL_NOEXCEPT;

    /// @brief Select best driver for channel data (used by Channel::showPixels)
    /// @param data Channel data to route (chipset configuration)
    /// @param affinity Engine affinity name (empty = no affinity, dynamic selection)
    /// @return Shared pointer to selected driver, or nullptr if none compatible
    /// @note Iterates drivers by priority (already sorted descending) and returns first that canHandle()
    /// @note This is called lazily in Channel::showPixels() if no driver is bound
    fl::shared_ptr<IChannelDriver> selectDriverForChannel(const ChannelDataPtr& data, const fl::string& affinity) FL_NOEXCEPT;

    /// @brief Poll all registered drivers and return aggregate state
    /// @return Aggregate state (READY if all ready, BUSY if any busy, ERROR if any error)
    /// @note This is NOT an IChannelDriver override - it's a diagnostic/test helper
    /// @note Polls all drivers and returns the "worst" state (ERROR > BUSY > READY)
    IChannelDriver::DriverState poll() FL_NOEXCEPT;

    /// @brief Wait for all drivers to become READY
    /// @note Polls drivers in a loop, calling async_run() and yielding with minimal delay
    /// @note Uses time-based delays to avoid busy-waiting while allowing async tasks to run
    bool waitForReady(u32 timeoutMs = 1000) FL_NOEXCEPT;
    bool waitForReadyOrDraining(u32 timeoutMs = 1000) FL_NOEXCEPT;

    /// @brief Poll drivers before frame starts to clear previous frame state
    /// @note Called at the beginning of each frame to ensure buffers from previous frame are released
    void onBeginFrame() FL_NOEXCEPT override;

    /// @brief Trigger transmission of batched channel data
    /// @note Called at frame boundaries to flush enqueued channels
    void onEndFrame() FL_NOEXCEPT override;

    /// @brief Reset bus manager state, clearing all enqueued and transmitting channels
    /// @note Call this between test cases or when reinitializing the LED system
    void reset() FL_NOEXCEPT;

private:
    /// @brief Wait until a condition is met, with check-pump-delay logic
    /// @param condition Function that returns true when waiting should stop
    /// @param timeoutMs Optional timeout in milliseconds (0 = no timeout)
    /// @return true if condition was met, false if timeout occurred
    /// @note Runs async_run() on each iteration and delays intelligently to avoid busy-waiting
    template<typename Condition>
    bool waitForCondition(Condition condition, u32 timeoutMs = 1000) FL_NOEXCEPT;

    void notifyPollNeeded() FL_NOEXCEPT;
    bool waitForPollNeededSignal(u32 timeoutMs) FL_NOEXCEPT;
    u32 pollNeededWaitSliceMs(u32 startTime, u32 timeoutMs) const FL_NOEXCEPT;
    static void notifyPollNeededThunk(void* context) FL_NOEXCEPT;

private:
    /// @brief Engine registry entry (priority + shared pointer + runtime control)
    struct EngineEntry {
        int priority;
        fl::shared_ptr<IChannelDriver> driver;
        fl::string name;  ///< Engine name for runtime identification (e.g., "RMT", "SPI")
        bool enabled;     ///< Runtime enable/disable flag

        /// @brief Sort by priority descending (higher numbers first)
        /// @note Higher priority values = higher precedence (e.g., 50 > 10)
        bool operator<(const EngineEntry& other) const FL_NOEXCEPT {
            return priority > other.priority;  // Sort descending: higher values first
        }
    };

    /// @brief Select driver for current operation
    /// @param data Channel data to check compatibility (optional, for predicate filtering)
    /// @return Pointer to selected driver, or nullptr if none available
    /// @note Selects highest priority driver from registry that can handle the channel data
    /// @note If data is nullptr, selects based on priority only (no predicate filtering)
    IChannelDriver* selectDriver(const ChannelDataPtr& data = nullptr) FL_NOEXCEPT;

    /// @brief Shared drivers sorted by priority descending (higher values first)
    /// @note Each entry contains priority value and shared_ptr to driver
    fl::vector<EngineEntry> mDrivers;

    /// @brief Cached driver info for getDriverInfos() to avoid allocations
    /// @note Marked mutable to allow caching in const method
    mutable fl::vector<DriverInfo> mCachedDriverInfo;

    /// @brief Exclusive driver name (empty if no exclusive mode)
    /// @note When non-empty, new drivers are auto-disabled if name doesn't match.
    ///       Set by `setExclusiveDriverByName()` / cleared on empty name.
    fl::string mExclusiveDriver;

    /// @brief Shared callback installed on drivers that can signal poll-needed events.
    IChannelDriver::PollNeededCallback mPollNeededCallback;

    /// @brief Platform wait primitive owned by the manager.
    platforms::ChannelPollSignal mPollNeededSignal;

    // Non-copyable, non-movable
    ChannelManager(const ChannelManager&) FL_NOEXCEPT = delete;
    ChannelManager& operator=(const ChannelManager&) FL_NOEXCEPT = delete;
    ChannelManager(ChannelManager&&) FL_NOEXCEPT = delete;
    ChannelManager& operator=(ChannelManager&&) FL_NOEXCEPT = delete;
};

/// @brief Get the global ChannelManager singleton instance
/// @return Reference to the singleton ChannelManager
/// @note Available on all platforms (has 0 drivers on non-ESP32)
ChannelManager& channelManager() FL_NOEXCEPT;

// Inline template definition for ChannelManager::setExclusiveDriver<B>().
// Defined here so the ODR-use of `BusTraits<B>::instancePtr()` happens in the
// caller's TU, which links the driver translation unit. The caller must have
// the per-driver `bus_traits.h` (which provides `BusTraits<B>::instancePtr`)
// visible at the call site.
template<fl::Bus B>
inline void ChannelManager::setExclusiveDriver() FL_NOEXCEPT {
    // Priority above any platform default — see `bus_priorities.h` for the
    // default-priority constants (highest there is ~10 for native LCD/I2S SPI).
    constexpr int kExclusivePriority = 10000;
    addDriver(kExclusivePriority, fl::BusTraits<B>::instancePtr());
}

} // namespace fl

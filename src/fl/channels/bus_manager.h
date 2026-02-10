/// @file bus_manager.h
/// @brief Unified bus manager for channel engines with priority-based fallback
///
/// The ChannelBusManager coordinates multiple channel engines (e.g., PARLIO, SPI, RMT)
/// and automatically selects the best available engine based on priority.
/// It acts as a transparent proxy using the Proxy/Decorator pattern - strip drivers
/// interact with it through the ChannelEngine interface without knowing about
/// the manager's existence.
///
/// Design Pattern: Proxy/Decorator
/// - Manager IS A ChannelEngine (inheritance for polymorphic use)
/// - Forwards calls to active engine
/// - Handles fallback automatically
/// - Strip drivers unaware of manager's existence
///
/// Usage Pattern:
/// 1. Platform-specific code builds and configures manager with engines
/// 2. Strip drivers obtain manager reference: mEngine = &ChannelBusManager::instance()
/// 3. Manager transparently selects best available engine
/// 4. Automatic fallback when engine allocation fails
/// 5. Per-frame reset allows engine re-evaluation

#pragma once

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/engine_events.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/string.h"

namespace fl {

/// @brief Driver state information for channel bus manager
struct DriverInfo {
    fl::string name;  ///< Driver name (empty for unnamed engines)
    int priority;     ///< Engine priority (higher = preferred)
    bool enabled;     ///< Whether driver is currently enabled
};

/// @brief Unified channel bus manager with priority-based engine selection
///
/// This manager acts as a registry for concrete engine implementations (RMT, SPI, PARLIO).
/// Channels select engines via selectEngineForChannel() and bind directly to them via weak_ptr.
///
/// Platform-specific code registers engines during static initialization.
class ChannelBusManager : public EngineEvents::Listener {
public:
    /// @brief Get the global singleton instance
    /// @return Reference to the singleton ChannelBusManager
    /// @note Thread-safe singleton initialization
    static ChannelBusManager& instance();

    /// @brief Constructor
    ChannelBusManager();

    /// @brief Destructor - cleanup shared engines (automatic via shared_ptr)
    ~ChannelBusManager() override;

    /// @brief Add an engine with priority (higher priority = preferred)
    /// @param priority Engine priority (higher values = higher priority)
    /// @param engine Shared engine implementation (allows caller to retain reference for testing)
    /// @note Platform-specific code calls this during static initialization
    /// @note Engines are automatically sorted by priority on each insertion
    /// @note Engine name is obtained via engine->getName()
    /// @note If engine->getName() returns empty string, engine is rejected (emits FL_WARN and returns)
    /// @note If an engine with the same name already exists, it will be replaced:
    ///       1. FL_WARN emitted about replacement
    ///       2. All engines polled until READY state (1 second timeout)
    ///       3. Old engine removed (shared_ptr may trigger deletion)
    ///       4. New engine added with specified priority
    void addEngine(int priority, fl::shared_ptr<IChannelEngine> engine);

    /// @brief Remove an engine from the manager
    /// @param engine Shared pointer to the engine to remove
    /// @return true if engine was found and removed, false if not found
    /// @note Emits FL_WARN if engine is not found
    /// @note Useful for test cleanup
    bool removeEngine(fl::shared_ptr<IChannelEngine> engine);

    /// @brief Remove all engines from the manager
    /// @note Clears the entire engine registry
    /// @note Useful for FastLED.reset() with CHANNEL_ENGINES flag
    /// @note Waits for all engines to become READY before clearing (1 second timeout)
    void clearAllEngines();

    /// @brief Enable or disable a driver by name at runtime
    /// @param name Driver name to control (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @param enabled true to enable, false to disable
    /// @note Disabled drivers are skipped during selection
    /// @note Changes take effect immediately on next enqueue()
    /// @note If name is not found, this is a no-op (does not warn)
    void setDriverEnabled(const char* name, bool enabled);

    /// @brief Enable only one driver exclusively (disables all others)
    /// @param name Driver name to enable exclusively (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @return true if driver was found and set as exclusive, false if name not found
    /// @note Atomically disables all drivers, then enables the specified one
    /// @note Changes take effect immediately on next enqueue()
    /// @note If name is not found, all drivers remain disabled (defensive behavior)
    /// @note Use nullptr or empty string to disable all drivers (returns false)
    /// @warning This will disable ALL other registered drivers, including future additions
    /// @warning This ensures forward compatibility - new drivers are automatically excluded
    bool setExclusiveDriver(const char* name);

    /// @brief Change the priority of a registered driver
    /// @param name Driver name (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @param priority New priority value (higher = preferred, e.g., 9000 > 5000)
    /// @return true if driver was found and priority updated, false if name not found
    /// @note Triggers automatic re-sort of engine list by priority (descending)
    /// @note Changes take effect immediately on next enqueue()
    /// @note Higher priority engines are selected first during canHandle() iteration
    bool setDriverPriority(const fl::string& name, int priority);

    /// @brief Check if a driver is enabled by name
    /// @param name Driver name to query (case-sensitive)
    /// @return true if enabled, false if disabled or not registered
    bool isDriverEnabled(const char* name) const;

    /// @brief Get count of registered drivers (including unnamed ones)
    /// @return Total number of registered engines
    fl::size getDriverCount() const;

    /// @brief Get full state of all registered drivers
    /// @return Span of driver info (sorted by priority descending)
    /// @note Returned span is valid until next call to any non-const method
    /// @note This method updates an internal cache - not thread-safe
    /// @note fl::string copies are cheap (shared pointer internally, no heap allocation)
    fl::span<const DriverInfo> getDriverInfos() const;

    /// @brief Get engine by name for affinity binding
    /// @param name Engine name to look up (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @return Shared pointer to engine if found and enabled, nullptr otherwise
    /// @note Used by Channel affinity system to bind channels to specific engines
    fl::shared_ptr<IChannelEngine> getEngineByName(const fl::string& name) const;

    /// @brief Select best engine for channel data (used by Channel::showPixels)
    /// @param data Channel data to route (chipset configuration)
    /// @param affinity Engine affinity name (empty = no affinity, dynamic selection)
    /// @return Shared pointer to selected engine, or nullptr if none compatible
    /// @note Iterates engines by priority (already sorted descending) and returns first that canHandle()
    /// @note This is called lazily in Channel::showPixels() if no engine is bound
    fl::shared_ptr<IChannelEngine> selectEngineForChannel(const ChannelDataPtr& data, const fl::string& affinity);

    /// @brief Poll all registered engines and return aggregate state
    /// @return Aggregate state (READY if all ready, BUSY if any busy, ERROR if any error)
    /// @note This is NOT an IChannelEngine override - it's a diagnostic/test helper
    /// @note Polls all engines and returns the "worst" state (ERROR > BUSY > READY)
    IChannelEngine::EngineState poll();

    /// @brief Wait for all engines to become READY
    /// @note Polls engines in a loop, calling async_run() and yielding with minimal delay
    /// @note Uses time-based delays to avoid busy-waiting while allowing async tasks to run
    bool waitForReady(u32 timeoutMs = 1000);
    bool waitForReadyOrDraining(u32 timeoutMs = 1000);

    /// @brief Poll engines before frame starts to clear previous frame state
    /// @note Called at the beginning of each frame to ensure buffers from previous frame are released
    void onBeginFrame() override;

    /// @brief Trigger transmission of batched channel data
    /// @note Called at frame boundaries to flush enqueued channels
    void onEndFrame() override;

    /// @brief Reset bus manager state, clearing all enqueued and transmitting channels
    /// @note Call this between test cases or when reinitializing the LED system
    void reset();

private:
    /// @brief Wait until a condition is met, with check-pump-delay logic
    /// @param condition Function that returns true when waiting should stop
    /// @param timeoutMs Optional timeout in milliseconds (0 = no timeout)
    /// @return true if condition was met, false if timeout occurred
    /// @note Runs async_run() on each iteration and delays intelligently to avoid busy-waiting
    template<typename Condition>
    bool waitForCondition(Condition condition, u32 timeoutMs = 1000);

private:
    /// @brief Engine registry entry (priority + shared pointer + runtime control)
    struct EngineEntry {
        int priority;
        fl::shared_ptr<IChannelEngine> engine;
        fl::string name;  ///< Engine name for runtime identification (e.g., "RMT", "SPI")
        bool enabled;     ///< Runtime enable/disable flag

        /// @brief Sort by priority descending (higher numbers first)
        /// @note Higher priority values = higher precedence (e.g., 50 > 10)
        bool operator<(const EngineEntry& other) const {
            return priority > other.priority;  // Sort descending: higher values first
        }
    };

    /// @brief Select engine for current operation
    /// @param data Channel data to check compatibility (optional, for predicate filtering)
    /// @return Pointer to selected engine, or nullptr if none available
    /// @note Selects highest priority engine from registry that can handle the channel data
    /// @note If data is nullptr, selects based on priority only (no predicate filtering)
    IChannelEngine* selectEngine(const ChannelDataPtr& data = nullptr);

    /// @brief Shared engines sorted by priority descending (higher values first)
    /// @note Each entry contains priority value and shared_ptr to engine
    fl::vector<EngineEntry> mEngines;

    /// @brief Cached driver info for getDriverInfos() to avoid allocations
    /// @note Marked mutable to allow caching in const method
    mutable fl::vector<DriverInfo> mCachedDriverInfo;

    /// @brief Exclusive driver name (empty if no exclusive mode)
    /// @note When non-empty, new engines are auto-disabled if name doesn't match
    fl::string mExclusiveDriver;

    // Non-copyable, non-movable
    ChannelBusManager(const ChannelBusManager&) = delete;
    ChannelBusManager& operator=(const ChannelBusManager&) = delete;
    ChannelBusManager(ChannelBusManager&&) = delete;
    ChannelBusManager& operator=(ChannelBusManager&&) = delete;
};

/// @brief Get the global ChannelBusManager singleton instance
/// @return Reference to the singleton ChannelBusManager
/// @note Available on all platforms (has 0 drivers on non-ESP32)
/// @note On ESP32, platform code registers drivers during static initialization
ChannelBusManager& channelBusManager();

} // namespace fl

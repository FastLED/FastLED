/// @file channel_bus_manager.h
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

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/engine_events.h"
#include "ftl/vector.h"
#include "ftl/shared_ptr.h"

namespace fl {

/// @brief Unified channel bus manager with priority-based engine selection
///
/// This manager inherits from IChannelEngine and acts as a transparent proxy
/// to concrete engine implementations (RMT, SPI, PARLIO). Strip drivers use
/// it polymorphically through the IChannelEngine interface.
///
/// Platform-specific code registers engines during static initialization.
class ChannelBusManager : public IChannelEngine, public EngineEvents::Listener {
public:
    /// @brief Constructor
    ChannelBusManager();

    /// @brief Destructor - cleanup shared engines (automatic via shared_ptr)
    ~ChannelBusManager() override;

    /// @brief Add an engine with priority and name (higher priority = preferred)
    /// @param priority Engine priority (higher values = higher priority)
    /// @param engine Shared engine implementation (allows caller to retain reference for testing)
    /// @param name Engine name for runtime enable/disable control (e.g., "RMT", "SPI", "PARLIO")
    /// @note Platform-specific code calls this during static initialization
    /// @note Engines are automatically sorted by priority on each insertion
    /// @note Empty name or nullptr engine will be ignored
    void addEngine(int priority, fl::shared_ptr<IChannelEngine> engine, const char* name = nullptr);

    /// @brief Enable or disable a driver by name at runtime
    /// @param name Driver name to control (case-sensitive, e.g., "RMT", "SPI", "PARLIO")
    /// @param enabled true to enable, false to disable
    /// @note Disabled drivers are skipped during selection
    /// @note Changes take effect immediately on next enqueue()
    /// @note If name is not found, this is a no-op (does not warn)
    void setDriverEnabled(const char* name, bool enabled);

    /// @brief Check if a driver is enabled by name
    /// @param name Driver name to query (case-sensitive)
    /// @return true if enabled, false if disabled or not registered
    bool isDriverEnabled(const char* name) const;

    /// @brief Get count of registered drivers (including unnamed ones)
    /// @return Total number of registered engines
    fl::size getDriverCount() const;

    /// @brief Driver state information
    struct DriverInfo {
        fl::string name;  ///< Driver name (empty for unnamed engines)
        int priority;     ///< Engine priority (higher = preferred)
        bool enabled;     ///< Whether driver is currently enabled
    };

    /// @brief Get full state of all registered drivers
    /// @return Span of driver info (sorted by priority descending)
    /// @note Returned span is valid until next call to any non-const method
    /// @note This method updates an internal cache - not thread-safe
    /// @note fl::string copies are cheap (shared pointer internally, no heap allocation)
    fl::span<const DriverInfo> getDriverInfo() const;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    /// @note Selects engine on first call, then batches channel data
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    EngineState poll() override;

    /// @brief Reset to highest priority engine for next frame
    /// @note Called at frame boundaries to allow engine re-evaluation
    void onEndFrame() override;

private:
    /// @brief Begin transmission using active engine with fallback
    /// @param channelData Span of channel data to transmit
    /// @note Tries active engine, falls back to next priority on failure
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

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
    /// @return Pointer to selected engine, or nullptr if none available
    /// @note Selects highest priority engine from registry (highest priority value)
    IChannelEngine* selectEngine();

    /// @brief Get next lower priority engine for fallback
    /// @return Pointer to next engine, or nullptr if none available
    /// @note Searches for engine with lower priority value than current active engine
    IChannelEngine* getNextLowerPriorityEngine();

    /// @brief Shared engines sorted by priority descending (higher values first)
    /// @note Each entry contains priority value and shared_ptr to engine
    fl::vector<EngineEntry> mEngines;

    /// @brief Currently active engine (cached for performance)
    IChannelEngine* mActiveEngine = nullptr;

    /// @brief Priority of active engine (for fallback logic)
    int mActiveEnginePriority = -1;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr> mEnqueuedChannels;  ///< Channels enqueued via enqueue(), waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels;  ///< Channels currently transmitting (for cleanup)

    /// @brief Error message storage
    fl::string mLastError;

    /// @brief Cached driver info for getDriverInfo() to avoid allocations
    /// @note Marked mutable to allow caching in const method
    mutable fl::vector<DriverInfo> mCachedDriverInfo;

    // Non-copyable, non-movable
    ChannelBusManager(const ChannelBusManager&) = delete;
    ChannelBusManager& operator=(const ChannelBusManager&) = delete;
    ChannelBusManager(ChannelBusManager&&) = delete;
    ChannelBusManager& operator=(ChannelBusManager&&) = delete;
};

} // namespace fl

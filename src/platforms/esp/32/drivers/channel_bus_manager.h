/// @file channel_bus_manager.h
/// @brief Unified bus manager for ESP32 channel engines with priority-based fallback
///
/// The ChannelBusManager coordinates multiple channel engines (PARLIO, SPI, RMT)
/// and automatically selects the best available engine based on priority.
/// It acts as a transparent proxy using the Proxy/Decorator pattern - strip drivers
/// interact with it through the ChannelEngine interface without knowing about
/// the manager's existence.
///
/// Priority Order:
/// - PARLIO (100): Highest performance, best timing (ESP32-P4, C6, H2, C5)
/// - SPI (50): Good performance, reliable (ESP32-S3, others)
/// - RMT (10): Fallback, lower performance (all ESP32 variants)
///
/// Usage Pattern:
/// 1. Each engine registers itself during static initialization
/// 2. Strip drivers obtain manager reference: mEngine = &ChannelBusManager::instance()
/// 3. Manager transparently selects best available engine
/// 4. Automatic fallback when engine allocation fails
/// 5. Per-frame reset allows engine re-evaluation

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/engine_events.h"
#include "ftl/vector.h"
#include "ftl/unique_ptr.h"
#include "platforms/esp/32/feature_flags/enabled.h"

namespace fl {

/// @brief Unified channel bus manager with priority-based engine selection
///
/// This manager inherits from ChannelEngine and acts as a transparent proxy
/// to concrete engine implementations (RMT, SPI, PARLIO). Strip drivers use
/// it polymorphically through the ChannelEngine interface.
///
/// Design Pattern: Proxy/Decorator
/// - Manager IS A ChannelEngine (inheritance for polymorphic use)
/// - Forwards calls to active engine
/// - Handles fallback automatically
/// - Strip drivers unaware of manager's existence
class ChannelBusManager : public ChannelEngine, public EngineEvents::Listener {
public:
    /// @brief Engine priority constants
    static constexpr int PRIORITY_PARLIO = 100;  ///< Highest (PARLIO engine - ESP32-P4/C6/H2/C5)
    static constexpr int PRIORITY_SPI = 50;      ///< Medium (SPI engine)
    static constexpr int PRIORITY_RMT = 10;      ///< Lowest (RMT fallback)

    /// @brief Singleton accessor (returns reference for polymorphic usage)
    /// @return Reference to global ChannelBusManager instance
    /// @note Returns ChannelBusManager& which implicitly converts to ChannelEngine&
    static ChannelBusManager& instance();

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    /// @note Selects engine on first call, then forwards to base class enqueue()
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Reset to highest priority engine for next frame
    /// @note Called at frame boundaries to allow engine re-evaluation
    void onEndFrame() override;

protected:
    /// @brief Query engine state (polls active engine)
    /// @return Current engine state from active engine, or READY if no engine
    EngineState pollDerived() override;

    /// @brief Begin transmission using active engine with fallback
    /// @param channelData Span of channel data to transmit
    /// @note Tries active engine, falls back to next priority on failure
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) override;

private:
    /// @brief Engine registry entry (priority + owned pointer)
    struct EngineEntry {
        int priority;
        fl::unique_ptr<ChannelEngine> engine;

        /// @brief Sort by priority descending (highest first)
        bool operator<(const EngineEntry& other) const {
            return priority > other.priority;  // Reverse: higher priority = earlier
        }
    };

    /// @brief Select engine for current operation
    /// @return Pointer to selected engine, or nullptr if none available
    /// @note Selects highest priority engine from registry
    ChannelEngine* selectEngine();

    /// @brief Get next lower priority engine for fallback
    /// @return Pointer to next engine, or nullptr if none available
    /// @note Searches for engine with priority lower than current active engine
    ChannelEngine* getNextLowerPriorityEngine();

    /// @brief Owned engines sorted by priority descending
    /// @note Each entry contains priority and owned unique_ptr to engine
    fl::vector<EngineEntry> mEngines;

    /// @brief Currently active engine (cached for performance)
    ChannelEngine* mActiveEngine = nullptr;

    /// @brief Priority of active engine (for fallback logic)
    int mActiveEnginePriority = -1;

    /// @brief Private constructor (singleton pattern)
    ChannelBusManager();

    /// @brief Destructor - cleanup owned engines (automatic via unique_ptr)
    ~ChannelBusManager() override;

    // Non-copyable, non-movable
    ChannelBusManager(const ChannelBusManager&) = delete;
    ChannelBusManager& operator=(const ChannelBusManager&) = delete;
    ChannelBusManager(ChannelBusManager&&) = delete;
    ChannelBusManager& operator=(ChannelBusManager&&) = delete;
};

} // namespace fl

#endif // ESP32

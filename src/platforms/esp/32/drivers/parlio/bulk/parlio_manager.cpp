/// @file parlio_manager.cpp
/// @brief Implementation of PARLIO group coordination manager

#if defined(ESP32)
#include "sdkconfig.h"

// PARLIO driver is only available on ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_manager.h"
#include "parlio_group.h"
#include "fl/singleton.h"
#include "fl/log.h"
#include "fl/vector.h"

namespace fl {

// Concrete implementation - all platform-specific details hidden in cpp
class ParlioManager : public IParlioManager {
public:
    ParlioManager() = default;
    ~ParlioManager() override = default;

    void registerGroup(void* groupPtr, void (*flushFunc)(void*)) override;
    void unregisterGroup(void* groupPtr) override;
    void flushAll() override;
    void flushAllExcept(void* exceptPtr) override;
    void onEndShow(IParlioGroup* group) override;
    void write_all_groups() override;
    void engineAcquire(void* owner) override;
    void engineRelease(void* owner) override;
    bool engineConfigure(const ParlioHardwareConfig& config) override;
    bool engineWrite(const uint8_t* data, size_t bits, void* callback_arg) override;
    void engineShutdown() override;
    bool engineIsConfigured() const override;
    void engineWaitWriteDone() override;

private:
    /// Group entry in manager
    struct GroupEntry {
        void* groupPtr;        ///< Opaque pointer to group
        void (*flushFunc)(void*);    ///< Function to flush this group

        bool operator==(const GroupEntry& other) const {
            return groupPtr == other.groupPtr;
        }
    };

    /// List of registered groups
    fl::vector_fixed<GroupEntry, 16> mGroups;

    /// List of groups pending transmission (batching support)
    fl::vector_fixed<IParlioGroup*, 16> mPendingGroups;

    /// Check if entry already exists in manager
    /// @param entry Entry to search for
    /// @returns true if entry is already registered
    bool contains(const GroupEntry& entry);
};

// Static method - returns interface but creates concrete singleton
IParlioManager& IParlioManager::getInstance() {
    return fl::Singleton<ParlioManager>::instance();
}

void ParlioManager::registerGroup(void* groupPtr, void (*flushFunc)(void*)) {
    GroupEntry entry{groupPtr, flushFunc};

    // Only add if not already registered
    if (!contains(entry)) {
        mGroups.push_back(entry);
        FL_DBG("PARLIO Manager: Registered group " << groupPtr);
    }
}

void ParlioManager::unregisterGroup(void* groupPtr) {
    // Find and remove the group from the manager
    for (auto it = mGroups.begin(); it != mGroups.end(); ++it) {
        if (it->groupPtr == groupPtr) {
            mGroups.erase(it);
            FL_DBG("PARLIO Manager: Unregistered group " << groupPtr);
            return;
        }
    }
}

void ParlioManager::flushAll() {
    FL_DBG("PARLIO Manager: Flushing all " << mGroups.size() << " groups");
    for (auto& entry : mGroups) {
        if (entry.flushFunc != nullptr) {
            entry.flushFunc(entry.groupPtr);
        }
    }

    // After all groups have queued their data, write all pending groups
    write_all_groups();
}

void ParlioManager::flushAllExcept(void* exceptPtr) {
    FL_DBG("PARLIO Manager: Flushing all groups except " << exceptPtr);
    for (auto& entry : mGroups) {
        if (entry.groupPtr != exceptPtr && entry.flushFunc != nullptr) {
            FL_DBG("PARLIO Manager: Flushing group " << entry.groupPtr);
            entry.flushFunc(entry.groupPtr);
        }
    }
}

bool ParlioManager::contains(const GroupEntry& entry) {
    for (const auto& existing : mGroups) {
        if (existing == entry) {
            return true;
        }
    }
    return false;
}

// ===== ENGINE GATEKEEPER METHODS =====
// These methods proxy all engine access through the manager.
// This ensures centralized control and prevents multiple components
// from directly accessing the engine singleton.

void ParlioManager::engineAcquire(void* owner) {
    auto& engine = IParlioEngine::getInstance();
    engine.acquire(owner);
}

void ParlioManager::engineRelease(void* owner) {
    auto& engine = IParlioEngine::getInstance();
    engine.release(owner);
}

bool ParlioManager::engineConfigure(const ParlioHardwareConfig& config) {
    auto& engine = IParlioEngine::getInstance();
    return engine.configure(config);
}

bool ParlioManager::engineWrite(const uint8_t* data, size_t bits, void* callback_arg) {
    auto& engine = IParlioEngine::getInstance();
    return engine.write(data, bits, callback_arg);
}

void ParlioManager::engineShutdown() {
    auto& engine = IParlioEngine::getInstance();
    engine.shutdown();
}

bool ParlioManager::engineIsConfigured() const {
    auto& engine = IParlioEngine::getInstance();
    return engine.is_configured();
}

void ParlioManager::engineWaitWriteDone() {
    auto& engine = IParlioEngine::getInstance();
    engine.wait_write_done();
}

// ===== BATCHING METHODS (Task 2.1) =====

void ParlioManager::onEndShow(IParlioGroup* group) {
    if (!group) {
        FL_WARN("PARLIO Manager: onEndShow called with null group");
        return;
    }

    // Check if group already pending (avoid duplicates)
    for (const auto* pending : mPendingGroups) {
        if (pending == group) {
            FL_DBG("PARLIO Manager: Group " << group << " already pending");
            return;
        }
    }

    FL_DBG("PARLIO Manager: Registered group " << group << " for batched transmission");
    mPendingGroups.push_back(group);
}

void ParlioManager::write_all_groups() {
    if (mPendingGroups.empty()) {
        FL_DBG("PARLIO Manager: No pending groups to write");
        return;
    }

    FL_DBG("PARLIO Manager: Writing " << mPendingGroups.size() << " groups sequentially");

    auto& engine = IParlioEngine::getInstance();

    // Write each pending group sequentially
    for (auto* group : mPendingGroups) {
        if (!group) {
            FL_WARN("PARLIO Manager: Null group in pending list, skipping");
            continue;
        }

        FL_DBG("PARLIO Manager: Processing group " << group);

        // 1. Acquire engine (blocks if busy)
        engine.acquire(group);

        // 2. Let group prepare DMA buffer
        group->prepare_write();

        // 3. Get hardware config from group
        ParlioHardwareConfig hw_config = group->get_hardware_config();

        // 4. Configure hardware for this group's parameters
        if (!engine.configure(hw_config)) {
            FL_WARN("PARLIO Manager: Engine configuration failed for group " << group);
            engine.release(group);
            continue;  // Skip this group
        }

        // 5. Get write parameters from group
        const uint8_t* data = group->get_dma_buffer();
        uint32_t total_bits = group->get_total_bits();
        uint32_t bits_per_component = group->get_bits_per_component();

        // 6. Write via engine (chunked if needed)
        if (!engine.write_chunked(data, total_bits, bits_per_component, group)) {
            FL_WARN("PARLIO Manager: Engine write failed for group " << group);
            engine.release(group);
            continue;
        }

        // 7. Wait for completion
        engine.wait_write_done();

        // 8. Release for next group
        engine.release(group);

        FL_DBG("PARLIO Manager: Group " << group << " write complete");
    }

    // Clear pending list
    mPendingGroups.clear();
    FL_DBG("PARLIO Manager: All groups written, pending list cleared");
}

} // namespace fl

#endif  // CONFIG_IDF_TARGET_ESP32P4
#endif  // ESP32

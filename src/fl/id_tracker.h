#pragma once

#include "fl/hash_map.h"
#include "fl/mutex.h"
#include "fl/namespace.h"

namespace fl {

/**
 * Thread-safe ID tracker that maps void* pointers to unique integer IDs.
 * 
 * Features:
 * - Auto-incrementing ID counter for new entries
 * - Thread-safe operations with mutex protection
 * - Instantiable class - create as many trackers as needed
 * - Support for removal of tracked pointers
 * 
 * Usage:
 *   IdTracker tracker;  // Create instance
 *   int id = tracker.getOrCreateId(ptr);
 *   bool found = tracker.getId(ptr, &id);
 *   tracker.removeId(ptr);
 * 
 * For singleton behavior, wrap in your own singleton:
 *   static IdTracker& getGlobalTracker() {
 *       static IdTracker instance;
 *       return instance;
 *   }
 */
class IdTracker {
public:
    /**
     * Default constructor - creates a new ID tracker instance
     */
    IdTracker() = default;

    /**
     * Get existing ID for pointer, or create a new one if not found.
     * Thread-safe.
     * 
     * @param ptr Pointer to track
     * @return Unique integer ID for this pointer
     */
    int getOrCreateId(void* ptr);

    /**
     * Get existing ID for pointer without creating a new one.
     * Thread-safe.
     * 
     * @param ptr Pointer to look up
     * @param outId Pointer to store the ID if found
     * @return true if ID was found, false if pointer not tracked
     */
    bool getId(void* ptr, int* outId);

    /**
     * Remove tracking for a pointer.
     * Thread-safe.
     * 
     * @param ptr Pointer to stop tracking
     * @return true if pointer was being tracked and removed, false if not found
     */
    bool removeId(void* ptr);

    /**
     * Get the current number of tracked pointers.
     * Thread-safe.
     * 
     * @return Number of currently tracked pointers
     */
    size_t size();

    /**
     * Clear all tracked pointers and reset ID counter.
     * Thread-safe.
     */
    void clear();

    // Non-copyable and non-movable for thread safety
    // (Each instance should have its own independent state)
    IdTracker(const IdTracker&) = delete;
    IdTracker& operator=(const IdTracker&) = delete;
    IdTracker(IdTracker&&) = delete;
    IdTracker& operator=(IdTracker&&) = delete;

private:

    // Thread synchronization
    mutable fl::mutex mMutex;
    
    // ID mapping and counter
    fl::hash_map<void*, int> mPointerToId;
    int mNextId = 0;  // Start IDs at 0 to match StripIdMap behavior
};

} // namespace fl 

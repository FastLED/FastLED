#include "fl/channels/id_tracker.h"
#include "fl/stl/noexcept.h"

namespace fl {

int IdTracker::getOrCreateId(void* ptr) FL_NOEXCEPT {
    if (!ptr) {
        return -1;  // Invalid pointer gets invalid ID
    }

    // Lock for thread safety
    mMutex.lock();

    // Check if ID already exists
    auto it = mPointerToId.find(ptr);
    if (it != mPointerToId.end()) {
        int id = it->second;
        mMutex.unlock();
        return id;
    }

    // Create new ID
    int newId = mNextId++;
    mPointerToId[ptr] = newId;

    mMutex.unlock();
    return newId;
}

bool IdTracker::getId(void* ptr, int* outId) FL_NOEXCEPT {
    if (!ptr || !outId) {
        return false;
    }

    // Lock for thread safety
    mMutex.lock();

    auto it = mPointerToId.find(ptr);
    bool found = (it != mPointerToId.end());
    if (found) {
        *outId = it->second;
    }

    mMutex.unlock();
    return found;
}

bool IdTracker::removeId(void* ptr) FL_NOEXCEPT {
    if (!ptr) {
        return false;
    }

    // Lock for thread safety
    mMutex.lock();

    bool removed = mPointerToId.erase(ptr);

    mMutex.unlock();
    return removed;
}

size_t IdTracker::size() FL_NOEXCEPT {
    // Lock for thread safety
    mMutex.lock();

    size_t currentSize = mPointerToId.size();

    mMutex.unlock();
    return currentSize;
}

void IdTracker::clear() FL_NOEXCEPT {
    // Lock for thread safety
    mMutex.lock();

    mPointerToId.clear();
    mNextId = 0;  // Reset ID counter to start at 0

    mMutex.unlock();
}

} // namespace fl

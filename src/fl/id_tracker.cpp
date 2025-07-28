#include "fl/id_tracker.h"

namespace fl {

int IdTracker::getOrCreateId(void* ptr) {
    if (!ptr) {
        return -1;  // Invalid pointer gets invalid ID
    }
    
    // Lock for thread safety
    mMutex.lock();
    
    // Check if ID already exists
    const int* existingId = mPointerToId.find_value(ptr);
    if (existingId) {
        int id = *existingId;
        mMutex.unlock();
        return id;
    }
    
    // Create new ID
    int newId = mNextId++;
    mPointerToId[ptr] = newId;
    
    mMutex.unlock();
    return newId;
}

bool IdTracker::getId(void* ptr, int* outId) {
    if (!ptr || !outId) {
        return false;
    }
    
    // Lock for thread safety
    mMutex.lock();
    
    const int* existingId = mPointerToId.find_value(ptr);
    bool found = (existingId != nullptr);
    if (found) {
        *outId = *existingId;
    }
    
    mMutex.unlock();
    return found;
}

bool IdTracker::removeId(void* ptr) {
    if (!ptr) {
        return false;
    }
    
    // Lock for thread safety
    mMutex.lock();
    
    bool removed = mPointerToId.erase(ptr);
    
    mMutex.unlock();
    return removed;
}

size_t IdTracker::size() {
    // Lock for thread safety
    mMutex.lock();
    
    size_t currentSize = mPointerToId.size();
    
    mMutex.unlock();
    return currentSize;
}

void IdTracker::clear() {
    // Lock for thread safety
    mMutex.lock();
    
    mPointerToId.clear();
    mNextId = 0;  // Reset ID counter to start at 0
    
    mMutex.unlock();
}

} // namespace fl 

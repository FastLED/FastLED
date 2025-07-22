#pragma once
#ifdef __EMSCRIPTEN__

// ================================================================================================
// FASTLED WASM THREAD-SAFE FRAME BUFFER MANAGER
// ================================================================================================
//
// This class manages thread-safe access to frame data between the continuous C++ main() loop
// and asynchronous JavaScript frame data requests. It replaces the old synchronous timing
// system with a lock-based approach that allows JavaScript to pull frame data at any time.
//
// Key Features:
// - Thread-safe frame buffer with read/write locks
// - Atomic frame data capture during FastLED.show()
// - Async JavaScript access via getFrameData() without blocking the C++ loop
// - Double-buffering to prevent data corruption during updates
// - Frame versioning to detect when new data is available
//
// Architecture:
// - C++ loop continuously updates the "active" frame buffer
// - JavaScript async requests get a snapshot copy of the "stable" frame buffer  
// - Buffer swapping happens atomically during frame end events
// - Locks ensure data consistency but don't block the main loop for long
//
// This solves the timing problem created by moving from extern_loop() calls to main() loop.
// ================================================================================================

#include <emscripten/threading.h>
#include <mutex>
#include <atomic>

#include "active_strip_data.h"
#include "fl/engine_events.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/singleton.h"

namespace fl {

/**
 * Thread-safe frame buffer manager for WASM builds
 * 
 * Manages frame data synchronization between:
 * - Continuous C++ main() loop (producer)
 * - Async JavaScript frame requests (consumer)
 */
class FrameBufferManager : public fl::EngineEvents::Listener {
public:
    static FrameBufferManager& Instance();
    
    /**
     * Called by JavaScript via getFrameData() - gets atomic snapshot of current frame
     * This is thread-safe and non-blocking for the C++ loop
     * @param dataSize Output parameter for JSON string size
     * @return Malloc'd buffer containing frame data JSON (caller must free with freeFrameData)
     */
    void* getFrameDataCopy(int* dataSize);
    
    /**
     * Called by JavaScript via getScreenMapData() - gets atomic snapshot of screen maps
     * This is thread-safe and non-blocking for the C++ loop
     * @param dataSize Output parameter for JSON string size  
     * @return Malloc'd buffer containing screen map JSON (caller must free with freeFrameData)
     */
    void* getScreenMapDataCopy(int* dataSize);
    
    /**
     * Get current frame version number
     * JavaScript can use this to detect when new frame data is available
     * @return Current frame version (increments on each frame)
     */
    uint32_t getFrameVersion() const { return mFrameVersion.load(); }
    
    /**
     * Check if new frame data is available since last request
     * @param lastKnownVersion Last frame version JavaScript received
     * @return True if new data is available
     */
    bool hasNewFrameData(uint32_t lastKnownVersion) const {
        return mFrameVersion.load() > lastKnownVersion;
    }

    // EngineEvents::Listener implementation
    void onBeginFrame() override;
    void onEndFrame() override;

private:
    friend class fl::Singleton<FrameBufferManager>;
    FrameBufferManager();
    ~FrameBufferManager();
    
    // Double-buffered frame data for atomic swapping
    struct FrameBuffer {
        fl::Str frameDataJson;      // Serialized ActiveStripData
        fl::Str screenMapJson;      // Serialized screen map data
        uint32_t version;           // Frame version number
        bool isValid;               // Whether this buffer contains valid data
        
        FrameBuffer() : version(0), isValid(false) {}
    };
    
    // Double buffers for atomic frame swapping
    FrameBuffer mActiveBuffer;      // Currently being written by C++ loop
    FrameBuffer mStableBuffer;      // Currently readable by JavaScript
    
    // Thread synchronization
    mutable std::mutex mBufferMutex;            // Protects buffer swapping
    mutable std::mutex mActiveBufferMutex;      // Protects active buffer updates
    
    // Frame tracking
    std::atomic<uint32_t> mFrameVersion{0};     // Current frame version
    
    // Helper methods
    void updateActiveBuffer();      // Update active buffer with current frame data
    void swapBuffers();            // Atomically swap active and stable buffers
    fl::Str generateFrameDataJson();     // Generate frame data JSON from ActiveStripData
    fl::Str generateScreenMapJson();     // Generate screen map JSON
};

} // namespace fl

#endif // __EMSCRIPTEN__ 

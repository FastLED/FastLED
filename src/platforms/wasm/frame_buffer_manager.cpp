#ifdef __EMSCRIPTEN__

#include "frame_buffer_manager.h"

#include <cstring>
#include <cstdlib>
#include <cfloat>

#include "fl/dbg.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include <stdio.h>

namespace fl {

FrameBufferManager& FrameBufferManager::Instance() {
    return fl::Singleton<FrameBufferManager>::instance();
}

FrameBufferManager::FrameBufferManager() {
    printf("FrameBufferManager: Initializing thread-safe frame buffer system\n");
    
    // Register for engine events to track frame lifecycle
    fl::EngineEvents::addListener(this);
    
    // Initialize both buffers as invalid
    mActiveBuffer.isValid = false;
    mStableBuffer.isValid = false;
}

FrameBufferManager::~FrameBufferManager() {
    fl::EngineEvents::removeListener(this);
}

void FrameBufferManager::onBeginFrame() {
    // Called at the start of each frame - prepare active buffer for updates
    std::lock_guard<std::mutex> lock(mActiveBufferMutex);
    
    // Mark active buffer as being updated
    mActiveBuffer.isValid = false;
}

void FrameBufferManager::onEndFrame() {
    // Called at the end of each frame - capture frame data and make it available
    printf("FrameBufferManager: Frame end - capturing frame data\n");
    
    // Update active buffer with current frame data
    updateActiveBuffer();
    
    // Atomically swap buffers to make new data available to JavaScript
    swapBuffers();
    
    printf("FrameBufferManager: Frame %u ready for JavaScript access\n", mFrameVersion.load());
}

void FrameBufferManager::updateActiveBuffer() {
    std::lock_guard<std::mutex> lock(mActiveBufferMutex);
    
    // Generate current frame data
    mActiveBuffer.frameDataJson = generateFrameDataJson();
    mActiveBuffer.screenMapJson = generateScreenMapJson();
    mActiveBuffer.version = mFrameVersion.load() + 1;
    mActiveBuffer.isValid = true;
    
    printf("FrameBufferManager: Active buffer updated with frame data (%zu bytes frame data, %zu bytes screen map)\n",
           mActiveBuffer.frameDataJson.length(), mActiveBuffer.screenMapJson.length());
}

void FrameBufferManager::swapBuffers() {
    std::lock_guard<std::mutex> lock(mBufferMutex);
    
    // Only swap if active buffer has valid data
    if (mActiveBuffer.isValid) {
        // Atomic swap of buffer contents
        std::swap(mActiveBuffer, mStableBuffer);
        
        // Update frame version counter
        mFrameVersion.store(mStableBuffer.version);
        
        printf("FrameBufferManager: Buffers swapped - frame %u now stable\n", mStableBuffer.version);
    }
}

fl::Str FrameBufferManager::generateFrameDataJson() {
    // Get current strip data and generate JSON
    fl::ActiveStripData& active_strips = fl::ActiveStripData::Instance();
    
    // Fill in any missing screen maps
    extern void jsFillInMissingScreenMaps(fl::ActiveStripData &active_strips);
    jsFillInMissingScreenMaps(active_strips);
    
    return active_strips.infoJsonString();
}

fl::Str FrameBufferManager::generateScreenMapJson() {
    // Get current screen map data and generate legacy-compatible JSON
    fl::ActiveStripData& active_strips = fl::ActiveStripData::Instance();
    const auto& screenMaps = active_strips.getScreenMaps();
    
    // Create screenMap JSON with expected structure (legacy-compatible)
    FLArduinoJson::JsonDocument doc;
    auto root = doc.to<FLArduinoJson::JsonObject>();
    auto stripsObj = root["strips"].to<FLArduinoJson::JsonObject>();
    
    // Track global bounds for absMax/absMin calculation
    float globalMinX = FLT_MAX, globalMinY = FLT_MAX;
    float globalMaxX = -FLT_MAX, globalMaxY = -FLT_MAX;
    bool hasData = false;
    
    // Get screenMap data
    for (const auto &[stripIndex, screenMap] : screenMaps) {
        // Create strip object with expected structure (legacy-compatible)
        // Use numeric keys instead of string keys for compatibility
        auto stripMapObj = stripsObj[std::to_string(stripIndex)].to<FLArduinoJson::JsonObject>();
        
        auto mapObj = stripMapObj["map"].to<FLArduinoJson::JsonObject>();
        auto xArray = mapObj["x"].to<FLArduinoJson::JsonArray>();
        auto yArray = mapObj["y"].to<FLArduinoJson::JsonArray>();
        
        // Track strip-specific bounds for min/max arrays
        float stripMinX = FLT_MAX, stripMinY = FLT_MAX;
        float stripMaxX = -FLT_MAX, stripMaxY = -FLT_MAX;
        
        for (uint32_t i = 0; i < screenMap.getLength(); i++) {
            float x = screenMap[i].x;
            float y = screenMap[i].y;
            
            xArray.add(x);
            yArray.add(y);
            
            // Update strip bounds
            if (x < stripMinX) stripMinX = x;
            if (x > stripMaxX) stripMaxX = x;
            if (y < stripMinY) stripMinY = y;
            if (y > stripMaxY) stripMaxY = y;
            
            // Update global bounds
            if (x < globalMinX) globalMinX = x;
            if (x > globalMaxX) globalMaxX = x;
            if (y < globalMinY) globalMinY = y;
            if (y > globalMaxY) globalMaxY = y;
            hasData = true;
        }
        
        // Add legacy-compatible min/max arrays for this strip
        if (screenMap.getLength() > 0) {
            auto minArray = stripMapObj["min"].to<FLArduinoJson::JsonArray>();
            auto maxArray = stripMapObj["max"].to<FLArduinoJson::JsonArray>();
            
            minArray.add(stripMinX);
            minArray.add(stripMinY);
            maxArray.add(stripMaxX);
            maxArray.add(stripMaxY);
        }
        
        // Add diameter
        stripMapObj["diameter"] = screenMap.getDiameter();
    }
    
    // Add global absMin and absMax arrays if we have data
    if (hasData) {
        auto absMinArray = root["absMin"].to<FLArduinoJson::JsonArray>();
        auto absMaxArray = root["absMax"].to<FLArduinoJson::JsonArray>();
        
        absMinArray.add(globalMinX);
        absMinArray.add(globalMinY);
        absMaxArray.add(globalMaxX);
        absMaxArray.add(globalMaxY);
    } else {
        // Provide default bounds if no data
        auto absMinArray = root["absMin"].to<FLArduinoJson::JsonArray>();
        auto absMaxArray = root["absMax"].to<FLArduinoJson::JsonArray>();
        
        absMinArray.add(0.0f);
        absMinArray.add(0.0f);
        absMaxArray.add(0.0f);
        absMaxArray.add(0.0f);
    }
    
    fl::Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
}

void* FrameBufferManager::getFrameDataCopy(int* dataSize) {
    std::lock_guard<std::mutex> lock(mBufferMutex);
    
    if (!mStableBuffer.isValid || mStableBuffer.frameDataJson.empty()) {
        printf("FrameBufferManager: No valid frame data available\n");
        if (dataSize) *dataSize = 0;
        return nullptr;
    }
    
    // Create a copy of the stable frame data for JavaScript
    const fl::Str& jsonStr = mStableBuffer.frameDataJson;
    char* buffer = (char*)malloc(jsonStr.length() + 1);
    if (!buffer) {
        printf("FrameBufferManager: Failed to allocate frame data buffer\n");
        if (dataSize) *dataSize = 0;
        return nullptr;
    }
    
    strcpy(buffer, jsonStr.c_str());
    if (dataSize) *dataSize = jsonStr.length();
    
    printf("FrameBufferManager: Copied frame data (%zu bytes) for JavaScript - frame version %u\n",
           jsonStr.length(), mStableBuffer.version);
    
    return buffer;
}

void* FrameBufferManager::getScreenMapDataCopy(int* dataSize) {
    std::lock_guard<std::mutex> lock(mBufferMutex);
    
    if (!mStableBuffer.isValid || mStableBuffer.screenMapJson.empty()) {
        printf("FrameBufferManager: No valid screen map data available\n");
        if (dataSize) *dataSize = 0;
        return nullptr;
    }
    
    // Create a copy of the stable screen map data for JavaScript
    const fl::Str& jsonStr = mStableBuffer.screenMapJson;
    char* buffer = (char*)malloc(jsonStr.length() + 1);
    if (!buffer) {
        printf("FrameBufferManager: Failed to allocate screen map buffer\n");
        if (dataSize) *dataSize = 0;
        return nullptr;
    }
    
    strcpy(buffer, jsonStr.c_str());
    if (dataSize) *dataSize = jsonStr.length();
    
    printf("FrameBufferManager: Copied screen map data (%zu bytes) for JavaScript - frame version %u\n",
           jsonStr.length(), mStableBuffer.version);
    
    return buffer;
}

} // namespace fl

#endif // __EMSCRIPTEN__ 

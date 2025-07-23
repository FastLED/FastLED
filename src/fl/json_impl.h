#pragma once

#include "fl/compiler_control.h"
#include "fl/str.h"
#include "fl/vector.h"

// Forward declarations for PIMPL pattern
namespace fl {
    class JsonDocumentImpl; // Wrapper around FLArduinoJson::JsonDocument
}

namespace fl {

/**
 * @brief Internal implementation class for JSON PIMPL pattern
 * 
 * This class hides ArduinoJSON implementation details from public headers
 * while providing support for both object and array root types.
 */
class JsonImpl {
public:
    JsonImpl();
    // JsonVariant constructor temporarily removed due to namespace versioning issues
    ~JsonImpl();
    
    // Copy constructor and assignment
    JsonImpl(const JsonImpl& other);
    JsonImpl& operator=(const JsonImpl& other);
    
    // Root type detection and support
    bool isArray() const;
    bool isObject() const;
    bool isNull() const;
    
    // Array-specific operations
    size_t getSize() const;
    JsonImpl getArrayElement(int index) const;
    void appendArrayElement(const JsonImpl& element);
    
    // Object-specific operations  
    JsonImpl getObjectField(const char* key) const;
    void setObjectField(const char* key, const JsonImpl& value);
    bool hasField(const char* key) const;
    
    // Convenience methods for setting object field values directly
    void setObjectFieldValue(const char* key, int value);
    void setObjectFieldValue(const char* key, const char* value);
    void setObjectFieldValue(const char* key, const fl::string& value);
    void setObjectFieldValue(const char* key, float value);
    void setObjectFieldValue(const char* key, bool value);
    
    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> getObjectKeys() const;
    
    // Value operations - use explicit overloads instead of templates to keep header minimal
    void setValue(const char* value);
    void setValue(const fl::string& value);
    void setValue(int value);
    void setValue(float value);
    void setValue(bool value);
    void setNull();
    
    // Value getting
    fl::string getStringValue() const;
    int getIntValue() const;
    float getFloatValue() const;
    bool getBoolValue() const;
    
    // Parsing with root type detection
    bool parseWithRootDetection(const char* jsonStr, fl::string* error = nullptr);
    
    // Serialization
    fl::string serialize() const;
    
    // Create new containers
    static JsonImpl createArray();
    static JsonImpl createObject();
    
private:
    JsonDocumentImpl* mDocument; // Wrapper around FLArduinoJson::JsonDocument
    void* mVariant; // JsonVariant* - using void* to avoid namespace issues
    bool mIsRootArray;
    bool mOwnsDocument; // Track if this instance owns the document
    
    void copyFrom(const JsonImpl& other);
    void cleanup();
};

} // namespace fl 

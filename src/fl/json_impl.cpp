#include "fl/json_impl.h"
#include "fl/json.h"

#if FASTLED_ENABLE_JSON
#include "third_party/arduinojson/json.h"
#endif

namespace fl {

// Wrapper class that inherits from FLArduinoJson::JsonDocument for clean forward declaration
class JsonDocumentImpl {
#if FASTLED_ENABLE_JSON
public:
    ::FLArduinoJson::JsonDocument doc;
    JsonDocumentImpl() = default;
    ~JsonDocumentImpl() = default;
#endif
};

// Proxy class that handles all ArduinoJSON operations
class ProxyVariant {
#if FASTLED_ENABLE_JSON
public:
    fl::shared_ptr<JsonDocumentImpl> document;
    ::FLArduinoJson::JsonVariant variant;
    bool isRootArray;
    
    ProxyVariant() : isRootArray(false) {}
    
    // Factory methods
    static fl::shared_ptr<ProxyVariant> createArray() {
        auto proxy = fl::make_shared<ProxyVariant>();
        proxy->document = fl::make_shared<JsonDocumentImpl>();
        proxy->document->doc.to<::FLArduinoJson::JsonArray>();
        proxy->variant = proxy->document->doc.as<::FLArduinoJson::JsonVariant>();
        proxy->isRootArray = true;
        return proxy;
    }
    
    static fl::shared_ptr<ProxyVariant> createObject() {
        auto proxy = fl::make_shared<ProxyVariant>();
        proxy->document = fl::make_shared<JsonDocumentImpl>();
        proxy->document->doc.to<::FLArduinoJson::JsonObject>();
        proxy->variant = proxy->document->doc.as<::FLArduinoJson::JsonVariant>();
        proxy->isRootArray = false;
        return proxy;
    }
    
    static fl::shared_ptr<ProxyVariant> fromParsed(const char* jsonStr) {
        auto proxy = fl::make_shared<ProxyVariant>();
        proxy->document = fl::make_shared<JsonDocumentImpl>();
        
        auto result = ::FLArduinoJson::deserializeJson(proxy->document->doc, jsonStr);
        if (result == ::FLArduinoJson::DeserializationError::Ok) {
            proxy->variant = proxy->document->doc.as<::FLArduinoJson::JsonVariant>();
            proxy->isRootArray = proxy->variant.is<::FLArduinoJson::JsonArray>();
            return proxy;
        }
        return nullptr;
    }
    
    // Delegate all operations to the variant
    bool isArray() const { return variant.is<::FLArduinoJson::JsonArray>(); }
    bool isObject() const { return variant.is<::FLArduinoJson::JsonObject>(); }
    bool isNull() const { return variant.isNull(); }
    
    // Additional type checks
    bool isString() const { return variant.is<const char*>() || variant.is<::FLArduinoJson::JsonString>(); }
    bool isInt() const { 
        return variant.is<int>() || variant.is<long>() || variant.is<long long>() || 
               variant.is<unsigned int>() || variant.is<unsigned long>() || variant.is<unsigned long long>();
    }
    bool isFloat() const { return variant.is<float>() || variant.is<double>(); }
    bool isBool() const { return variant.is<bool>(); }
    size_t size() const {
        if (variant.is<::FLArduinoJson::JsonArray>()) {
            return variant.as<::FLArduinoJson::JsonArray>().size();
        } else if (variant.is<::FLArduinoJson::JsonObject>()) {
            return variant.as<::FLArduinoJson::JsonObject>().size();
        }
        return 0;
    }
    
    fl::shared_ptr<ProxyVariant> getField(const char* key) const {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key) return nullptr;
        
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        auto childVariant = obj[key];
        if (childVariant.isNull()) return nullptr;
        
        auto childProxy = fl::make_shared<ProxyVariant>();
        childProxy->document = document; // Share the document
        childProxy->variant = childVariant;
        childProxy->isRootArray = childVariant.is<::FLArduinoJson::JsonArray>();
        return childProxy;
    }
    
    fl::shared_ptr<ProxyVariant> getElement(int index) const {
        if (!variant.is<::FLArduinoJson::JsonArray>() || index < 0) return nullptr;
        
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        if (index >= static_cast<int>(arr.size())) return nullptr;
        
        auto childVariant = arr[index];
        auto childProxy = fl::make_shared<ProxyVariant>();
        childProxy->document = document; // Share the document
        childProxy->variant = childVariant;
        childProxy->isRootArray = childVariant.is<::FLArduinoJson::JsonArray>();
        return childProxy;
    }
    
    fl::string getStringValue() const {
        if (variant.is<const char*>()) {
            return fl::string(variant.as<const char*>());
        }
        return "";
    }
    
    int getIntValue() const { return variant.as<int>(); }
    float getFloatValue() const { return variant.as<float>(); }
    bool getBoolValue() const { return variant.as<bool>(); }
    
    fl::string serialize() const {
        if (!document) return "{}";
        fl::string result;
        ::FLArduinoJson::serializeJson(document->doc, result);
        return result;
    }
    
    void setField(const char* key, int value) {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key) return;
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        obj[key] = value;
    }
    
    void setField(const char* key, const char* value) {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key || !value) return;
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        obj[key] = value;
    }
    
    void setField(const char* key, const fl::string& value) {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key) return;
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        obj[key] = value.c_str();
    }
    
    void setField(const char* key, float value) {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key) return;
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        obj[key] = value;
    }
    
    void setField(const char* key, bool value) {
        if (!variant.is<::FLArduinoJson::JsonObject>() || !key) return;
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        obj[key] = value;
    }
    
    void appendElement(fl::shared_ptr<ProxyVariant> element) {
        if (!variant.is<::FLArduinoJson::JsonArray>() || !element) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(element->variant);
    }
    
    void appendElement(int value) {
        if (!variant.is<::FLArduinoJson::JsonArray>()) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(value);
    }
    
    void appendElement(float value) {
        if (!variant.is<::FLArduinoJson::JsonArray>()) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(value);
    }
    
    void appendElement(bool value) {
        if (!variant.is<::FLArduinoJson::JsonArray>()) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(value);
    }
    
    void appendElement(const char* value) {
        if (!variant.is<::FLArduinoJson::JsonArray>() || !value) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(value);
    }
    
    void appendElement(const fl::string& value) {
        if (!variant.is<::FLArduinoJson::JsonArray>()) return;
        auto arr = variant.as<::FLArduinoJson::JsonArray>();
        arr.add(value.c_str());
    }
    
    fl::vector<fl::string> getObjectKeys() const {
        fl::vector<fl::string> keys;
        if (!variant.is<::FLArduinoJson::JsonObject>()) return keys;
        
        auto obj = variant.as<::FLArduinoJson::JsonObject>();
        for (auto kv : obj) {
            keys.push_back(fl::string(kv.key().c_str()));
        }
        return keys;
    }

    void setValue(const char* value) { variant.set(value); }
    void setValue(const fl::string& value) { variant.set(value.c_str()); }
    void setValue(int value) { variant.set(value); }
    void setValue(float value) { variant.set(value); }
    void setValue(bool value) { variant.set(value); }
    void setNull() { variant.set(nullptr); }
#else
public:
    bool isRootArray = false;
    static fl::shared_ptr<ProxyVariant> createArray() { 
        auto proxy = fl::make_shared<ProxyVariant>();
        proxy->isRootArray = true;
        return proxy;
    }
    static fl::shared_ptr<ProxyVariant> createObject() { 
        auto proxy = fl::make_shared<ProxyVariant>();
        proxy->isRootArray = false;
        return proxy;
    }
    static fl::shared_ptr<ProxyVariant> fromParsed(const char*) { return nullptr; }
    bool isArray() const { return isRootArray; }
    bool isObject() const { return !isRootArray; }
    bool isNull() const { return false; }
    bool isString() const { return false; }
    bool isInt() const { return false; }
    bool isFloat() const { return false; }
    bool isBool() const { return false; }
    size_t size() const { return 0; }
    fl::shared_ptr<ProxyVariant> getField(const char*) const { return nullptr; }
    fl::shared_ptr<ProxyVariant> getElement(int) const { return nullptr; }
    fl::string getStringValue() const { return ""; }
    int getIntValue() const { return 0; }
    float getFloatValue() const { return 0.0f; }
    bool getBoolValue() const { return false; }
    fl::string serialize() const { return isRootArray ? "[]" : "{}"; }
    void setField(const char*, int) {}
    void setField(const char*, const char*) {}
    void setField(const char*, const fl::string&) {}
    void setField(const char*, float) {}
    void setField(const char*, bool) {}
    void appendElement(fl::shared_ptr<ProxyVariant>) {}
    void appendElement(int) {}
    void appendElement(float) {}
    void appendElement(bool) {}
    void appendElement(const char*) {}
    void appendElement(const fl::string&) {}
    fl::vector<fl::string> getObjectKeys() const { return fl::vector<fl::string>(); }
#endif
};

JsonImpl::JsonImpl() : mProxy(nullptr) {}

JsonImpl::~JsonImpl() {
    cleanup();
}

void JsonImpl::cleanup() {
    // ProxyVariant manages its own memory via shared_ptr
    mProxy = nullptr;
}

JsonImpl::JsonImpl(const JsonImpl& other) {
    copyFrom(other);
}

JsonImpl& JsonImpl::operator=(const JsonImpl& other) {
    if (this != &other) {
        cleanup();
        copyFrom(other);
    }
    return *this;
}

void JsonImpl::copyFrom(const JsonImpl& other) {
    mProxy = other.mProxy; // shared_ptr copy
}

// Core functionality implementation
bool JsonImpl::parseWithRootDetection(const char* jsonStr, fl::string* error) {
    mProxy = ProxyVariant::fromParsed(jsonStr);
    if (!mProxy) {
        if (error) *error = "Parse failed";
        return false;
    }
    return true;
}

bool JsonImpl::isArray() const { 
    return mProxy ? mProxy->isArray() : false;
}

bool JsonImpl::isObject() const { 
    return mProxy ? mProxy->isObject() : false;
}

bool JsonImpl::isNull() const { 
    return !mProxy || mProxy->isNull();
}

bool JsonImpl::isString() const { 
    return mProxy ? mProxy->isString() : false;
}

bool JsonImpl::isInt() const { 
    return mProxy ? mProxy->isInt() : false;
}

bool JsonImpl::isFloat() const { 
    return mProxy ? mProxy->isFloat() : false;
}

bool JsonImpl::isBool() const { 
    return mProxy ? mProxy->isBool() : false;
}

size_t JsonImpl::getSize() const { 
    return mProxy ? mProxy->size() : 0;
}

JsonImpl JsonImpl::getObjectField(const char* key) const {
    JsonImpl result;
    if (mProxy) {
        auto childProxy = mProxy->getField(key);
        if (childProxy) {
            result.mProxy = childProxy;
        }
    }
    return result;
}

fl::vector<fl::string> JsonImpl::getObjectKeys() const {
    return mProxy ? mProxy->getObjectKeys() : fl::vector<fl::string>();
}

JsonImpl JsonImpl::getArrayElement(int index) const {
    JsonImpl result;
    if (mProxy) {
        auto childProxy = mProxy->getElement(index);
        if (childProxy) {
            result.mProxy = childProxy;
        }
    }
    return result;
}

fl::string JsonImpl::getStringValue() const {
    return mProxy ? mProxy->getStringValue() : fl::string("");
}

int JsonImpl::getIntValue() const {
    return mProxy ? mProxy->getIntValue() : 0;
}

bool JsonImpl::getBoolValue() const {
    return mProxy ? mProxy->getBoolValue() : false;
}

fl::string JsonImpl::serialize() const {
    return mProxy ? mProxy->serialize() : fl::string("{}");
}

// Factory methods implementation - use ProxyVariant factory methods
JsonImpl JsonImpl::createArray() { 
    JsonImpl result;
    result.mProxy = ProxyVariant::createArray();
    return result;
}

JsonImpl JsonImpl::createObject() { 
    JsonImpl result;
    result.mProxy = ProxyVariant::createObject();
    return result;
}

// Remaining stub methods - now delegate to ProxyVariant
void JsonImpl::appendArrayElement(const JsonImpl& element) {
    if (mProxy && element.mProxy) {
        mProxy->appendElement(element.mProxy);
    }
}

void JsonImpl::appendArrayElement(int value) {
    if (mProxy) {
        mProxy->appendElement(value);
    }
}

void JsonImpl::appendArrayElement(float value) {
    if (mProxy) {
        mProxy->appendElement(value);
    }
}

void JsonImpl::appendArrayElement(bool value) {
    if (mProxy) {
        mProxy->appendElement(value);
    }
}

void JsonImpl::appendArrayElement(const char* value) {
    if (mProxy) {
        mProxy->appendElement(value);
    }
}

void JsonImpl::appendArrayElement(const fl::string& value) {
    if (mProxy) {
        mProxy->appendElement(value);
    }
}

void JsonImpl::setObjectField(const char* key, const JsonImpl& value) {
    // For now, this is complex since we need to set a nested JSON value
    // We'll implement the basic setters first
}

bool JsonImpl::hasField(const char* key) const { 
    if (!mProxy) return false;
    auto field = mProxy->getField(key);
    return field != nullptr;
}

void JsonImpl::setValue(const char* value) {
    if (mProxy) {
        mProxy->setValue(value);
    } else {
        // If no proxy, create a new one for this value
        mProxy = ProxyVariant::fromParsed(fl::string(value).c_str());
    }
}

void JsonImpl::setValue(const fl::string& value) {
    if (mProxy) {
        mProxy->setValue(value);
    } else {
        mProxy = ProxyVariant::fromParsed(value.c_str());
    }
}

void JsonImpl::setValue(int value) {
    if (mProxy) {
        mProxy->setValue(value);
    } else {
        mProxy = ProxyVariant::fromParsed(fl::to_string(value).c_str());
    }
}

void JsonImpl::setValue(float value) {
    if (mProxy) {
        mProxy->setValue(value);
    } else {
        mProxy = ProxyVariant::fromParsed(fl::to_string(value).c_str());
    }
}

void JsonImpl::setValue(bool value) {
    if (mProxy) {
        mProxy->setValue(value);
    } else {
        mProxy = ProxyVariant::fromParsed(fl::to_string(value).c_str());
    }
}

void JsonImpl::setNull() {
    if (mProxy) {
        mProxy->setNull();
    } else {
        // If no proxy, it's already null
    }
}

// Convenience methods for setting object field values directly
void JsonImpl::setObjectFieldValue(const char* key, int value) {
    if (mProxy) {
        mProxy->setField(key, value);
    }
}

void JsonImpl::setObjectFieldValue(const char* key, const char* value) {
    if (mProxy) {
        mProxy->setField(key, value);
    }
}

void JsonImpl::setObjectFieldValue(const char* key, const fl::string& value) {
    if (mProxy) {
        mProxy->setField(key, value);
    }
}

void JsonImpl::setObjectFieldValue(const char* key, float value) {
    if (mProxy) {
        mProxy->setField(key, value);
    }
}

void JsonImpl::setObjectFieldValue(const char* key, bool value) {
    if (mProxy) {
        mProxy->setField(key, value);
    }
}

float JsonImpl::getFloatValue() const {
    return mProxy ? mProxy->getFloatValue() : 0.0f;
}

// Json class implementation with real parsing
Json::Json() : mImpl(fl::make_shared<JsonImpl>()) {}

Json Json::parse(const char* jsonStr) {
    Json result;
    fl::string error;
    if (!result.mImpl->parseWithRootDetection(jsonStr, &error)) {
        // Return null Json on parse error
        result.mImpl = fl::make_shared<JsonImpl>();
    }
    return result;
}

bool Json::has_value() const { return mImpl && !mImpl->isNull(); }
bool Json::is_object() const { return mImpl && mImpl->isObject(); }
bool Json::is_array() const { return mImpl && mImpl->isArray(); }

bool Json::is_string() const { return mImpl && mImpl->isString(); }
bool Json::is_int() const { return mImpl && mImpl->isInt(); }
bool Json::is_float() const { return mImpl && mImpl->isFloat(); }
bool Json::is_bool() const { return mImpl && mImpl->isBool(); }

Json Json::operator[](const char* key) const {
    Json result;
    if (mImpl) {
        result.mImpl = fl::make_shared<JsonImpl>(mImpl->getObjectField(key));
    }
    return result;
}

Json Json::operator[](int index) const {
    Json result;
    if (mImpl) {
        result.mImpl = fl::make_shared<JsonImpl>(mImpl->getArrayElement(index));
    }
    return result;
}

// Value getters
fl::string Json::getStringValue() const {
    return mImpl ? mImpl->getStringValue() : fl::string("");
}

int Json::getIntValue() const {
    return mImpl ? mImpl->getIntValue() : 0;
}

float Json::getFloatValue() const {
    return mImpl ? mImpl->getFloatValue() : 0.0f;
}

bool Json::getBoolValue() const {
    return mImpl ? mImpl->getBoolValue() : false;
}

bool Json::isNull() const {
    return !mImpl || mImpl->isNull();
}

// Array/Object size
size_t Json::getSize() const {
    return mImpl ? mImpl->getSize() : 0;
}

// Object iteration support
fl::vector<fl::string> Json::getObjectKeys() const {
    return mImpl ? mImpl->getObjectKeys() : fl::vector<fl::string>();
}

// Serialization
fl::string Json::serialize() const {
    return mImpl ? mImpl->serialize() : fl::string("{}");
}

// Static factory methods
Json Json::createArray() {
    Json result;
    result.mImpl = fl::make_shared<JsonImpl>(JsonImpl::createArray());
    return result;
}

Json Json::createObject() {
    Json result;
    result.mImpl = fl::make_shared<JsonImpl>(JsonImpl::createObject());
    return result;
}

// Modification methods for creating JSON
void Json::push_back(const Json& element) {
    if (mImpl && element.mImpl) {
        mImpl->appendArrayElement(*element.mImpl);
    }
}

void Json::set(const char* key, const Json& value) {
    if (mImpl && value.mImpl) {
        mImpl->setObjectField(key, *value.mImpl);
    }
}

void Json::set(const char* key, int value) {
    if (mImpl) {
        mImpl->setObjectFieldValue(key, value);
    }
}

void Json::set(const char* key, const char* value) {
    if (mImpl) {
        mImpl->setObjectFieldValue(key, value);
    }
}

void Json::set(const char* key, const fl::string& value) {
    if (mImpl) {
        mImpl->setObjectFieldValue(key, value);
    }
}

void Json::set(const char* key, float value) {
    if (mImpl) {
        mImpl->setObjectFieldValue(key, value);
    }
}

void Json::set(const char* key, bool value) {
    if (mImpl) {
        mImpl->setObjectFieldValue(key, value);
    }
}

// Additional array methods for compatibility
void Json::push_back(int value) {
    if (mImpl) {
        mImpl->appendArrayElement(value);
    }
}

void Json::push_back(float value) {
    if (mImpl) {
        mImpl->appendArrayElement(value);
    }
}

void Json::push_back(bool value) {
    if (mImpl) {
        mImpl->appendArrayElement(value);
    }
}

void Json::push_back(const char* value) {
    if (mImpl) {
        mImpl->appendArrayElement(value);
    }
}

void Json::push_back(const fl::string& value) {
    if (mImpl) {
        mImpl->appendArrayElement(value);
    }
}

// Nested object/array creation (FLArduinoJson compatibility)
Json Json::createNestedObject(const char* key) {
    Json nested = createObject();
    if (mImpl && key) {
        mImpl->setObjectField(key, *nested.mImpl);
    }
    return nested;
}

Json Json::createNestedArray(const char* key) {
    Json nested = createArray();
    if (mImpl && key) {
        mImpl->setObjectField(key, *nested.mImpl);
    }
    return nested;
}

Json Json::createNestedObject() {
    Json nested = createObject();
    push_back(nested);
    return nested;
}

Json Json::createNestedArray() {
    Json nested = createArray();
    push_back(nested);
    return nested;
}

} // namespace fl 

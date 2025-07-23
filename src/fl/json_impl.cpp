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

JsonImpl::JsonImpl() : mDocument(nullptr), mVariant(nullptr), mIsRootArray(false), mOwnsDocument(false) {}

JsonImpl::~JsonImpl() {
    cleanup();
}

void JsonImpl::cleanup() {
#if FASTLED_ENABLE_JSON
    if (mOwnsDocument && mDocument) {
        delete mDocument;
        mDocument = nullptr;
    }
#endif
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
    mIsRootArray = other.mIsRootArray;
    mOwnsDocument = false;  // Shared reference, don't own
    mDocument = other.mDocument;
    mVariant = other.mVariant;
}

// Core functionality implementation
bool JsonImpl::parseWithRootDetection(const char* jsonStr, fl::string* error) {
#if FASTLED_ENABLE_JSON
    if (!jsonStr) {
        if (error) *error = "null input string";
        return false;
    }
    
    // Create new document
    cleanup();
    mDocument = new JsonDocumentImpl();
    mOwnsDocument = true;
    
    // Parse JSON
    auto result = ::FLArduinoJson::deserializeJson(mDocument->doc, jsonStr);
    if (result != ::FLArduinoJson::DeserializationError::Ok) {
        if (error) {
            *error = result.c_str();
        }
        cleanup();
        return false;
    }
    
    // Detect root type and set up variant pointer
    auto root = mDocument->doc.as<::FLArduinoJson::JsonVariant>();
    mVariant = new ::FLArduinoJson::JsonVariant(root);
    mIsRootArray = root.is<::FLArduinoJson::JsonArray>();
    
    return true;
#else
    if (error) *error = "JSON support disabled";
    return false;
#endif
}

bool JsonImpl::isArray() const { 
#if FASTLED_ENABLE_JSON
    if (!mVariant) return mIsRootArray;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->is<::FLArduinoJson::JsonArray>();
#else
    return mIsRootArray; 
#endif
}

bool JsonImpl::isObject() const { 
#if FASTLED_ENABLE_JSON
    if (!mVariant) return !mIsRootArray && mDocument != nullptr;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->is<::FLArduinoJson::JsonObject>();
#else
    return !mIsRootArray && mDocument != nullptr;
#endif
}

bool JsonImpl::isNull() const { 
#if FASTLED_ENABLE_JSON
    if (!mVariant) return mDocument == nullptr;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->isNull();
#else
    return mDocument == nullptr;
#endif
}

size_t JsonImpl::getSize() const { 
#if FASTLED_ENABLE_JSON
    if (!mVariant) return 0;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (variant->is<::FLArduinoJson::JsonArray>()) {
        return variant->as<::FLArduinoJson::JsonArray>().size();
    } else if (variant->is<::FLArduinoJson::JsonObject>()) {
        return variant->as<::FLArduinoJson::JsonObject>().size();
    }
#endif
    return 0; 
}

JsonImpl JsonImpl::getObjectField(const char* key) const {
#if FASTLED_ENABLE_JSON
    JsonImpl result;
    if (!mVariant || !key) return result;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return result;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    auto childVariant = obj[key];
    if (!childVariant.isNull()) {
        result.mDocument = mDocument;  // Share document
        result.mOwnsDocument = false;
        result.mVariant = new ::FLArduinoJson::JsonVariant(childVariant);
        result.mIsRootArray = childVariant.is<::FLArduinoJson::JsonArray>();
    }
    return result;
#else
    return JsonImpl();
#endif
}

fl::vector<fl::string> JsonImpl::getObjectKeys() const {
#if FASTLED_ENABLE_JSON
    fl::vector<fl::string> keys;
    if (!mVariant) return keys;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return keys;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    for (auto kv : obj) {
        keys.push_back(fl::string(kv.key().c_str()));
    }
    return keys;
#else
    return fl::vector<fl::string>();
#endif
}

JsonImpl JsonImpl::getArrayElement(int index) const {
#if FASTLED_ENABLE_JSON
    JsonImpl result;
    if (!mVariant || index < 0) return result;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonArray>()) return result;
    
    auto arr = variant->as<::FLArduinoJson::JsonArray>();
    if (index < static_cast<int>(arr.size())) {
        result.mDocument = mDocument;  // Share document
        result.mOwnsDocument = false;
        auto childVariant = arr[index];
        result.mVariant = new ::FLArduinoJson::JsonVariant(childVariant);
        result.mIsRootArray = childVariant.is<::FLArduinoJson::JsonArray>();
    }
    return result;
#else
    return JsonImpl();
#endif
}

fl::string JsonImpl::getStringValue() const {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return "";
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (variant->is<const char*>()) {
        return fl::string(variant->as<const char*>());
    }
#endif
    return "";
}

int JsonImpl::getIntValue() const {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return 0;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->as<int>();
#endif
    return 0;
}

bool JsonImpl::getBoolValue() const {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return false;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->as<bool>();
#endif
    return false;
}

fl::string JsonImpl::serialize() const {
#if FASTLED_ENABLE_JSON
    if (!mDocument) return "{}";
    fl::string result;
    ::FLArduinoJson::serializeJson(mDocument->doc, result);
    return result;
#endif
    return "{}";
}

// Factory methods implementation
JsonImpl JsonImpl::createArray() { 
    JsonImpl impl;
#if FASTLED_ENABLE_JSON
    impl.mDocument = new JsonDocumentImpl();
    impl.mOwnsDocument = true;
    auto root = impl.mDocument->doc.to<::FLArduinoJson::JsonArray>();
    impl.mVariant = new ::FLArduinoJson::JsonVariant(root);
    impl.mIsRootArray = true;
#else
    impl.mIsRootArray = true;
#endif
    return impl;
}

JsonImpl JsonImpl::createObject() { 
    JsonImpl impl;
#if FASTLED_ENABLE_JSON
    impl.mDocument = new JsonDocumentImpl();
    impl.mOwnsDocument = true;
    auto root = impl.mDocument->doc.to<::FLArduinoJson::JsonObject>();
    impl.mVariant = new ::FLArduinoJson::JsonVariant(root);
    impl.mIsRootArray = false;
#endif
    return impl;
}

// Remaining stub methods (can be implemented incrementally)
void JsonImpl::appendArrayElement(const JsonImpl& element) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || !mIsRootArray) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonArray>()) return;
    
    auto array = variant->as<::FLArduinoJson::JsonArray>();
    
    if (element.mVariant) {
        auto* elementVariant = static_cast<::FLArduinoJson::JsonVariant*>(element.mVariant);
        array.add(*elementVariant);
    }
#endif
}

void JsonImpl::setObjectField(const char* key, const JsonImpl& value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    
    if (value.mVariant) {
        auto* valueVariant = static_cast<::FLArduinoJson::JsonVariant*>(value.mVariant);
        obj[key] = *valueVariant;
    }
#endif
}

bool JsonImpl::hasField(const char* key) const { 
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return false;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return false;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    return !obj[key].isNull();
#else
    return false;
#endif
}

void JsonImpl::setValue(const char* value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || !value) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(value);
#endif
}

void JsonImpl::setValue(const fl::string& value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(value.c_str());
#endif
}

void JsonImpl::setValue(int value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(value);
#endif
}

void JsonImpl::setValue(float value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(value);
#endif
}

void JsonImpl::setValue(bool value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(value);
#endif
}

void JsonImpl::setNull() {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    variant->set(nullptr);
#endif
}

// Convenience methods for setting object field values directly
void JsonImpl::setObjectFieldValue(const char* key, int value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    obj[key] = value;
#endif
}

void JsonImpl::setObjectFieldValue(const char* key, const char* value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key || !value) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    obj[key] = value;
#endif
}

void JsonImpl::setObjectFieldValue(const char* key, const fl::string& value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    obj[key] = value.c_str();
#endif
}

void JsonImpl::setObjectFieldValue(const char* key, float value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    obj[key] = value;
#endif
}

void JsonImpl::setObjectFieldValue(const char* key, bool value) {
#if FASTLED_ENABLE_JSON
    if (!mVariant || mIsRootArray || !key) return;
    
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    if (!variant->is<::FLArduinoJson::JsonObject>()) return;
    
    auto obj = variant->as<::FLArduinoJson::JsonObject>();
    obj[key] = value;
#endif
}

float JsonImpl::getFloatValue() const {
#if FASTLED_ENABLE_JSON
    if (!mVariant) return 0.0f;
    auto* variant = static_cast<::FLArduinoJson::JsonVariant*>(mVariant);
    return variant->as<float>();
#endif
    return 0.0f;
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

} // namespace fl 

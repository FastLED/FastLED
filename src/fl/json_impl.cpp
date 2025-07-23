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

// Minimal JsonImpl implementation - just stubs to make compilation work
JsonImpl::JsonImpl() : mDocument(nullptr), mVariant(nullptr), mIsRootArray(false), mOwnsDocument(false) {}

// JsonVariant constructor removed due to namespace versioning issues

JsonImpl::~JsonImpl() {
    cleanup();
}

void JsonImpl::cleanup() {
    // Minimal cleanup for now
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
    mOwnsDocument = false;
    mDocument = other.mDocument;
    mVariant = other.mVariant;
}

// Minimal stubs for all methods
bool JsonImpl::isArray() const { return mIsRootArray; }
bool JsonImpl::isObject() const { return !mIsRootArray && mDocument != nullptr; }
bool JsonImpl::isNull() const { return mDocument == nullptr; }
size_t JsonImpl::getSize() const { return 0; }
JsonImpl JsonImpl::getArrayElement(int index) const { return JsonImpl(); }
void JsonImpl::appendArrayElement(const JsonImpl& element) {}
JsonImpl JsonImpl::getObjectField(const char* key) const { return JsonImpl(); }
void JsonImpl::setObjectField(const char* key, const JsonImpl& value) {}
bool JsonImpl::hasField(const char* key) const { return false; }
void JsonImpl::setValue(const char* value) {}
void JsonImpl::setValue(const fl::string& value) {}
void JsonImpl::setValue(int value) {}
void JsonImpl::setValue(float value) {}
void JsonImpl::setValue(bool value) {}
void JsonImpl::setNull() {}
fl::string JsonImpl::getStringValue() const { return ""; }
int JsonImpl::getIntValue() const { return 0; }
float JsonImpl::getFloatValue() const { return 0.0f; }
bool JsonImpl::getBoolValue() const { return false; }
bool JsonImpl::parseWithRootDetection(const char* jsonStr, fl::string* error) { return false; }
fl::string JsonImpl::serialize() const { return "{}"; }
JsonImpl JsonImpl::createArray() { JsonImpl impl; impl.mIsRootArray = true; return impl; }
JsonImpl JsonImpl::createObject() { return JsonImpl(); }

// Json class implementation 
Json::Json() : mImpl(fl::make_shared<JsonImpl>()) {}

Json Json::parse(const char* jsonStr) {
    Json result;
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

} // namespace fl 

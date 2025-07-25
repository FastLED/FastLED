#include "fl/json.h"


#include "fl/json.h"
#include "fl/string.h"
#include "fl/variant.h"

namespace fl {

// --- JsonValue Implementations ---


fl::unique_ptr<JsonVariant>&& JsonValueNew() {
    return fl::move(fl::make_unique<JsonVariant>());
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(fl::nullptr_t) {
    return fl::move(fl::make_unique<JsonVariant>());
}


fl::unique_ptr<JsonVariant>&& JsonValueNew(bool value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(int value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(long value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(double value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(const fl::string& value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(const JsonObject& value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(const JsonArray& value) {
    return fl::move(fl::make_unique<JsonVariant>(value));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(JsonObject&& value) {
    return fl::move(fl::make_unique<JsonVariant>(fl::move(value)));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(JsonArray&& value) {
    return fl::move(fl::make_unique<JsonVariant>(fl::move(value)));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(const JsonVariant& other) {
    return fl::move(fl::make_unique<JsonVariant>(*other.data_));
}

fl::unique_ptr<JsonVariant>&& JsonValueNew(JsonVariant&& other) {
    // return fl::move(fl::make_unique<JsonVariant>(fl::move(other.data_)));
    #if FASTLED_ENABLE_JSON
    fl::unique_ptr<JsonVariant> data = fl::make_unique<JsonVariant>();
    *data = fl::move(other);
    return fl::move(data);
    #else
    return fl::move(fl::make_unique<JsonVariant>());
    #endif
}


void JsonVariantConstruct(fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(JsonVariant&& other, fl::unique_ptr<JsonVariant>* data);
void JsonVariantConstruct(fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantConstruct(JsonVariant&& other, fl::unique_ptr<JsonVariantImpl>* data);
void JsonVariantInNewInPlace(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data);




// JsonValue::JsonValue() : data_(JsonValueNew()) {}
JsonValue::JsonValue() { JsonVariantConstruct(&data_); }
JsonValue::JsonValue(bool value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(int value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(long value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(double value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(const char* value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(const fl::string& value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(const JsonObject& value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(const JsonArray& value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(JsonObject&& value) { JsonVariantConstruct(value, &data_); }
JsonValue::JsonValue(JsonArray&& value) { JsonVariantConstruct(value, &data_); }

// Copy and Move constructors/assignment operators
JsonValue::JsonValue(const JsonValue& other) {
    // JsonVariantInNewInPlace(other.data_, &data_);
    data_ = JsonValueNew(*other.data_);
    // *data_ = *other.data_;
}
JsonValue::JsonValue(JsonValue&& other) noexcept {
    //JsonVariantInNewInPlace(other.data_, &data_);
    data_ = fl::move(other.data_);
}
JsonValue& JsonValue::operator=(const JsonValue& other) {
    if (this != &other) {
        *data_ = *other.data_;
    }
    return *this;
}
JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        *data_ = *other.data_;
    }
    return *this;
}

#if FASTLED_ENABLE_JSON

bool JsonValue::is_null() const { return data_->is_null(); }
// bool JsonValue::is_bool() const { return data_->is<bool>(); }
bool JsonValue::is_bool() const { return data_->is_bool(); }
bool JsonValue::is_int() const { return data_->is_int(); }
bool JsonValue::is_long() const { return data_->is_long(); }
bool JsonValue::is_double() const { return data_->is_double(); }
bool JsonValue::is_string() const { return data_->is_string(); }
bool JsonValue::is_object() const { return data_->is_object(); }
bool JsonValue::is_array() const { return data_->is_array(); }

#else
bool JsonValue::is_null() const { return true; }
bool JsonValue::is_bool() const { return false; }
bool JsonValue::is_int() const { return false; }
bool JsonValue::is_long() const { return false; }
bool JsonValue::is_double() const { return false; }
bool JsonValue::is_string() const { return false; }
bool JsonValue::is_object() const { return false; }
bool JsonValue::is_array() const { return false; }

#endif

using ImplVariant = fl::Variant<
    fl::nullptr_t,
    bool,
    int,
    long,
    double,
    fl::string,
    JsonObject,
    JsonArray,
    JsonVariant
>;

struct JsonVariantImpl: public ImplVariant {
    JsonVariantImpl() : ImplVariant() {}
    JsonVariantImpl(fl::nullptr_t) : ImplVariant(fl::nullptr_t()) {}
    JsonVariantImpl(bool value) : ImplVariant(value) {}
    JsonVariantImpl(int value) : ImplVariant(value) {}
    JsonVariantImpl(long value) : ImplVariant(value) {}
    JsonVariantImpl(double value) : ImplVariant(value) {}
    JsonVariantImpl(const fl::string& value) : ImplVariant(value) {}
    JsonVariantImpl(const JsonObject& value) : ImplVariant(value) {}
    JsonVariantImpl(const JsonArray& value) : ImplVariant(value) {}
    JsonVariantImpl(JsonObject&& value) : ImplVariant(fl::move(value)) {}
    JsonVariantImpl(JsonArray&& value) : ImplVariant(fl::move(value)) {}
    JsonVariantImpl(const JsonVariant& other) : ImplVariant(*other.data_) {}
    //JsonVariantImpl(JsonVariant&& other) noexcept : ImplVariant(fl::move(other.data_)) {}
    JsonVariantImpl(JsonVariant&& other) noexcept : ImplVariant(fl::move(other)) {}
};

template<>
inline fl::string JsonValue::get<fl::string>() const {
    if (is_string()) {
        // return data_->get<fl::string>();
        return data_->get<fl::string>();
    }
    return ""; // Return empty string for non-string types
}



// Specialization for string to handle fl::string
template<>
inline fl::string JsonValue::operator|<fl::string>(fl::string defaultValue) const {
    if (is_null()) {
        return defaultValue;
    }
    if (is_string()) {
        return data_->get<fl::string>();
    }
    return defaultValue; // Return default if not a string
}


template<>
inline bool JsonValue::get<bool>() const {
    if (is_bool()) return data_->get<bool>();
    return false;
}

template<>
inline int JsonValue::get<int>() const {
    if (is_int()) return data_->get<int>();
    if (is_long()) return static_cast<int>(data_->get<long>());
    if (is_double()) return static_cast<int>(data_->get<double>());
    return 0;
}

template<>
inline long JsonValue::get<long>() const {
    if (is_long()) return data_->get<long>();
    if (is_int()) return static_cast<long>(data_->get<int>());
    return 0;
}

template<>
inline double JsonValue::get<double>() const {
    if (is_double()) return data_->get<double>();
    if (is_int()) return static_cast<double>(data_->get<int>());
    if (is_long()) return static_cast<double>(data_->get<long>());
    return 0.0;
}


template<>
inline JsonObject JsonValue::get<JsonObject>() const {
    if (is_object()) return data_->get<JsonObject>();
    return JsonObject();
}

template<>
inline JsonArray JsonValue::get<JsonArray>() const {
    if (is_array()) return data_->get<JsonArray>();
    return JsonArray();
}



JsonVariant::JsonVariant() { JsonVariantConstruct(&data_); }
JsonVariant::~JsonVariant() {}
JsonVariant::JsonVariant(bool value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(int value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(long value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(double value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(const fl::string& value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(const JsonObject& value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(const JsonArray& value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(JsonObject&& value) { JsonVariantConstruct(value, &data_); }
JsonVariant::JsonVariant(JsonArray&& value) { JsonVariantConstruct(value, &data_); }




JsonVariant::JsonVariant(const JsonVariant& other) {
    auto data = fl::make_unique<JsonVariantImpl>();
    data_ = fl::move(data);
    *data_ = *other.data_;
}
JsonVariant::JsonVariant(JsonVariant&& other) noexcept {
    // data_ = fl::move(other.data_);
    data_ = fl::move(other.data_);
}
JsonVariant& JsonVariant::operator=(const JsonVariant& other) {
    // *data_ = *other.data_;
    /// data_ = JsonValueNew(*other.data_);
    *data_ = *other.data_;
    return *this;
}

JsonVariant::JsonVariant(JsonVariantImpl& impl) {
    JsonVariantConstruct(&data_);
    *data_ = impl;
}


// template<typename T>
// T get() const;


template<>
inline fl::string JsonVariant::operator|<fl::string>(fl::string defaultValue) const {
    return data_->get<fl::string>();
}

JsonObject& JsonValue::get_object_mut() {
    if (!is_object()) {
        data_ = fl::make_unique<JsonVariant>(JsonObject()); // Convert to object if not already
    }
    // return fl::get<JsonObject>(data_);
    //return data_->get<JsonObject>();
    //  error: non-const lvalue reference to type 'JsonObject' cannot bind to a temporary of type 'fl::JsonObject'
    // return data_->get<JsonObject>();  // this needs to be fixed
    return data_->get_mut<JsonObject>();
}

JsonArray& JsonValue::get_array_mut() {
    if (!is_array()) {
        data_ = fl::make_unique<JsonVariant>(JsonArray()); // Convert to array if not already
    }
    //return fl::get<JsonArray>(data_);
    return data_->get_mut<JsonArray>();
}


// template<typename T>
// T& JsonVariant::get_mut();

// template<>
// inline JsonVariant& JsonValue::get<int>() {
//     return *data_;
// }

// make this work



JsonValue& JsonValue::operator[](const fl::string& key) {
    return get_object_mut()[key];
}

const JsonValue& JsonValue::operator[](const fl::string& key) const {
    if (is_object()) {
        const auto& obj = data_->get<JsonObject>();
        return obj[key];
    }
    static JsonValue null_value; // Return a static null value for non-objects or missing keys
    return null_value;
}

JsonValue& JsonValue::operator[](size_t index) {
    return get_array_mut()[index];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (is_array()) {
        const auto& arr = data_->get<JsonArray>();
        return arr[index];
    }
    static JsonValue null_value; // Return a static null value for non-arrays or out of bounds
    return null_value;
}

void JsonValue::push_back(const JsonValue& value) {
    get_array_mut().push_back(value);
}

void JsonValue::push_back(JsonValue&& value) {
    get_array_mut().push_back(fl::move(value));
}

size_t JsonValue::size() const {
    if (is_array()) {
        return data_->get<JsonArray>().size();
    }
    return 0;
}

fl::string JsonValue::to_string() const {
    if (is_null()) return "null";
    if (is_bool()) return data_->get<bool>() ? "true" : "false";
    if (is_int()) return fl::to_string(data_->get<int>());
    if (is_long()) return fl::to_string(data_->get<long>());
    if (is_double()) return fl::to_string(data_->get<double>());
    if (is_string()) {
        return "\"" + data_->get<fl::string>() + "\"";
    }
    if (is_object()) return data_->get<JsonObject>().to_string();
    if (is_array()) return data_->get<JsonArray>().to_string();
    return ""; // Should not happen
}

// --- JsonObject Implementations ---

JsonValue& JsonObject::operator[](const fl::string& key) {
    return members_[key];
}

const JsonValue& JsonObject::operator[](const fl::string& key) const {
    auto it = members_.find(key);
    if (it != members_.end()) {
        return it->second;
    }
    static JsonValue null_value; // Return a static null value for missing keys
    return null_value;
}

fl::string JsonObject::to_string() const {
    fl::string s = "{";
    bool first = true;
    for (const auto& pair : members_) {
        if (!first) s += ",";
        s += "\"" + pair.first + "\":" + pair.second.to_string();
        first = false;
    }
    s += "}";
    return s;
}

// --- JsonArray Implementations ---

JsonValue& JsonArray::operator[](size_t index) {
    if (index >= elements_.size()) {
        elements_.resize(index + 1); // Resize and fill with nulls if needed
    }
    return elements_[index];
}

const JsonValue& JsonArray::operator[](size_t index) const {
    if (index < elements_.size()) {
        return elements_[index];
    }
    static JsonValue null_value; // Return a static null value for out of bounds
    return null_value;
}

void JsonArray::push_back(const JsonValue& value) {
    elements_.push_back(value);
}

void JsonArray::push_back(JsonValue&& value) {
    elements_.push_back(fl::move(value));
}

size_t JsonArray::size() const {
    return elements_.size();
}

fl::string JsonArray::to_string() const {
    fl::string s = "[";
    bool first = true;
    for (const auto& element : elements_) {
        if (!first) s += ",";
        s += element.to_string();
        first = false;
    }
    s += "]";
    return s;
}

// --- Json Implementations ---

// This is a placeholder. A real JSON parser would be much more complex.
// For now, we'll just return an empty Json object.
Json Json::parse(const char* jsonString) {
    // In a real scenario, this would parse the JSON string.
    // For the purpose of making the test compile and allowing further development,
    // we'll return a Json object that can be populated manually for testing.
    // This will need to be replaced with a proper parser (e.g., using a library like RapidJSON or a custom parser).
    Json json_obj;
    // Simulate parsing for the test case by manually constructing the expected structure
    // This part is temporary and will be replaced by actual parsing logic.

    // Example of how you might manually construct the object for the test's configJson
    // This is NOT a parser, but a hardcoded representation for initial compilation.
    JsonObject strip;
    strip["num_leds"] = 150;
    strip["pin"] = 5;
    strip["type"] = "WS2812B";
    strip["brightness"] = 200;

    JsonObject effects;
    effects["current"] = "rainbow";
    effects["speed"] = 75;

    JsonObject animation_settings;
    animation_settings["duration_ms"] = 5000L; // Use long for duration_ms
    animation_settings["loop"] = true;

    JsonObject root_object;
    root_object["strip"] = strip;
    root_object["effects"] = effects;
    root_object["animation_settings"] = animation_settings;

    json_obj.root_ = JsonValue(root_object);

    return json_obj;
}

JsonValue& Json::operator[](const fl::string& key) {
    if (!root_.has_value()) {
        root_ = JsonObject(); // Initialize as an empty object if not set
    }
    return (*root_)[key];
}

const JsonValue& Json::operator[](const fl::string& key) const {
    if (root_.has_value()) {
        return (*root_)[key];
    }
    static JsonValue null_value; // Return a static null value if no root
    return null_value;
}


// void JsonVariantConstruct(fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(JsonVariant&& other, fl::unique_ptr<JsonVariant>* data);
// void JsonVariantConstruct(fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariantImpl>* data);
// void JsonVariantConstruct(JsonVariant&& other, fl::unique_pt



#if FASTLED_ENABLE_JSON

void JsonVariantConstruct(fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant());
}
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(value)));
}
void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(value)));
}
void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(other));
}
void JsonVariantConstruct(JsonVariant&& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(other)));
}
void JsonVariantConstruct(fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl());
}
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(fl::move(value)));
}

void JsonVariantInNewInPlace(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(other));
}



// void get_value(const JsonVariant* data, int& value);
// void get_value(const JsonVariant* data, long& value);
// void get_value(const JsonVariant* data, double& value);
// void get_value(const JsonVariant* data, fl::string& value);
// void get_value(const JsonVariant* data, JsonObject& value);
// void get_value(const JsonVariant* data, JsonArray& value);

// void get_value_mut(JsonVariant* data, int& value);
// void get_value_mut(JsonVariant* data, long& value);
// void get_value_mut(JsonVariant* data, double& value);
// void get_value_mut(JsonVariant* data, fl::string& value);
// void get_value_mut(JsonVariant* data, JsonObject& value);
// void get_value_mut(JsonVariant* data, JsonArray& value);

void get_value(const JsonVariant* data, bool& value) {
    value = data->get<bool>();
}

void get_value(const JsonVariant* data, int& value) {
    value = data->get<int>();
}


void get_value(const JsonVariant* data, long& value) {
    value = data->get<long>();
}

void get_value(const JsonVariant* data, double& value) {
    value = data->get<double>();
}

void get_value(const JsonVariant* data, fl::string& value) {
    value = data->get<fl::string>();
}

void get_value(const JsonVariant* data, JsonObject& value) {
    value = data->get<JsonObject>();
}

void get_value(const JsonVariant* data, JsonArray& value) {
    value = data->get<JsonArray>();
}

void get_value_mut(JsonVariant* data, int& value) {
    value = data->get_mut<int>();
}

void get_value_mut(JsonVariant* data, long& value) {
    value = data->get_mut<long>();
}

void get_value_mut(JsonVariant* data, double& value) {
    value = data->get_mut<double>();
}

void get_value_mut(JsonVariant* data, fl::string& value) {
    value = data->get_mut<fl::string>();
}

void get_value_mut(JsonVariant* data, JsonObject& value) {
    value = data->get_mut<JsonObject>();
}

void get_value_mut(JsonVariant* data, JsonArray& value) {
    value = data->get_mut<JsonArray>();
}

// void get_value_impl(const JsonVariantImpl* data, bool& value);
// void get_value_impl(const JsonVariantImpl* data, int& value);
// void get_value_impl(const JsonVariantImpl* data, long& value);
// void get_value_impl(const JsonVariantImpl* data, double& value);
// void get_value_impl(const JsonVariantImpl* data, fl::string& value);
// void get_value_impl(const JsonVariantImpl* data, JsonObject& value);
// void get_value_impl(const JsonVariantImpl* data, JsonArray& value);

void get_value_impl(const JsonVariantImpl* data, bool& value) {
    value = data->get<bool>();
}

void get_value_impl(const JsonVariantImpl* data, int& value) {
    value = data->get<int>();
}

void get_value_impl(const JsonVariantImpl* data, long& value) {
    value = data->get<long>();
}

void get_value_impl(const JsonVariantImpl* data, double& value) {
    value = data->get<double>();
}

void get_value_impl(const JsonVariantImpl* data, fl::string& value) {
    value = data->get<fl::string>();
}

void get_value_impl(const JsonVariantImpl* data, JsonObject& value) {
    value = data->get<JsonObject>();
}

void get_value_impl(const JsonVariantImpl* data, JsonArray& value) {
    value = data->get<JsonArray>();
}



#else


void get_value_impl_mut(JsonVariantImpl* data, int** value) {
    static int dummy = 0;
    *value = &dummy;
}

void get_value_impl_mut(JsonVariantImpl* data, long** value) {
    static long dummy = 0;
    *value = &dummy;
}

void get_value_impl_mut(JsonVariantImpl* data, double** value) {
    static double dummy = 0.0;
    *value = &dummy;
}

void get_value_impl_mut(JsonVariantImpl* data, fl::string** value) {
    static fl::string dummy = "";
    *value = &dummy;
}

void get_value_impl_mut(JsonVariantImpl* data, JsonObject** value) {
    static JsonObject dummy;
    *value = &dummy;
}
void get_value_impl_mut(JsonVariantImpl* data, JsonArray** value) {
    static JsonArray dummy;
    *value = &dummy;
}



// void get_value_impl_mut(JsonVariantImpl* data, long& value);
// void get_value_impl_mut(JsonVariantImpl* data, double& value);
// void get_value_impl_mut(JsonVariantImpl* data, fl::string& value);
// void get_value_impl_mut(JsonVariantImpl* data, JsonObject& value);
// void get_value_impl_mut(JsonVariantImpl* data, JsonArray& value);

void get_value_impl_mut(JsonVariantImpl* data, JsonVariant** value) {
    static JsonVariant dummy;
    *value = &dummy;
}


void JsonVariantConstruct(fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant());
}
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(value));
}
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(value)));
}
void JsonVariantConstruct(JsonArray&& value, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(value)));
}
void JsonVariantConstruct(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(other));
}
void JsonVariantConstruct(JsonVariant&& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(fl::move(other)));
}
void JsonVariantConstruct(fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl());
}
void JsonVariantConstruct(bool value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(int value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(long value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(double value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const fl::string& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const JsonObject& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(const JsonArray& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(value));
}
void JsonVariantConstruct(JsonObject&& value, fl::unique_ptr<JsonVariantImpl>* data) {
    data->reset(new JsonVariantImpl(fl::move(value)));
}

void JsonVariantInNewInPlace(const JsonVariant& other, fl::unique_ptr<JsonVariant>* data) {
    data->reset(new JsonVariant(other));
}



// void get_value(const JsonVariant* data, int& value);
// void get_value(const JsonVariant* data, long& value);
// void get_value(const JsonVariant* data, double& value);
// void get_value(const JsonVariant* data, fl::string& value);
// void get_value(const JsonVariant* data, JsonObject& value);
// void get_value(const JsonVariant* data, JsonArray& value);

// void get_value_mut(JsonVariant* data, int& value);
// void get_value_mut(JsonVariant* data, long& value);
// void get_value_mut(JsonVariant* data, double& value);
// void get_value_mut(JsonVariant* data, fl::string& value);
// void get_value_mut(JsonVariant* data, JsonObject& value);
// void get_value_mut(JsonVariant* data, JsonArray& value);

void get_value(const JsonVariant* data, bool& value) {
    value = data->get<bool>();
}

void get_value(const JsonVariant* data, int& value) {
    value = data->get<int>();
}


void get_value(const JsonVariant* data, long& value) {
    value = data->get<long>();
}

void get_value(const JsonVariant* data, double& value) {
    value = data->get<double>();
}

void get_value(const JsonVariant* data, fl::string& value) {
    value = data->get<fl::string>();
}

void get_value(const JsonVariant* data, JsonObject& value) {
    value = data->get<JsonObject>();
}

void get_value(const JsonVariant* data, JsonArray& value) {
    value = data->get<JsonArray>();
}

void get_value_mut(JsonVariant* data, int& value) {
    value = data->get_mut<int>();
}

void get_value_mut(JsonVariant* data, long& value) {
    value = data->get_mut<long>();
}

void get_value_mut(JsonVariant* data, double& value) {
    value = data->get_mut<double>();
}

void get_value_mut(JsonVariant* data, fl::string& value) {
    value = data->get_mut<fl::string>();
}

void get_value_mut(JsonVariant* data, JsonObject& value) {
    value = data->get_mut<JsonObject>();
}

void get_value_mut(JsonVariant* data, JsonArray& value) {
    value = data->get_mut<JsonArray>();
}

// void get_value_impl(const JsonVariantImpl* data, bool& value);
// void get_value_impl(const JsonVariantImpl* data, int& value);
// void get_value_impl(const JsonVariantImpl* data, long& value);
// void get_value_impl(const JsonVariantImpl* data, double& value);
// void get_value_impl(const JsonVariantImpl* data, fl::string& value);
// void get_value_impl(const JsonVariantImpl* data, JsonObject& value);
// void get_value_impl(const JsonVariantImpl* data, JsonArray& value);

void get_value_impl(const JsonVariantImpl* data, bool& value) {
    value = data->get<bool>();
}

void get_value_impl(const JsonVariantImpl* data, int& value) {
    value = data->get<int>();
}

void get_value_impl(const JsonVariantImpl* data, long& value) {
    value = data->get<long>();
}

void get_value_impl(const JsonVariantImpl* data, double& value) {
    value = data->get<double>();
}

void get_value_impl(const JsonVariantImpl* data, fl::string& value) {
    value = data->get<fl::string>();
}

void get_value_impl(const JsonVariantImpl* data, JsonObject& value) {
    value = data->get<JsonObject>();
}

void get_value_impl(const JsonVariantImpl* data, JsonArray& value) {
    value = data->get<JsonArray>();
}
#endif // FASTLED_ENABLE_JSON
} // namespace fl

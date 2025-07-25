#pragma once


#include "fl/string.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/variant.h"
#include "fl/optional.h"

#include "fl/type_traits.h"


#if FASTLED_ENABLE_JSON
#include "third_party/arduinojson/json.h"
#endif

namespace fl {

class JsonObject;
class JsonArray;
struct JsonVariantImpl;



void get_value_impl(const JsonVariantImpl* data, bool& value);
void get_value_impl(const JsonVariantImpl* data, int& value);
void get_value_impl(const JsonVariantImpl* data, long& value);
void get_value_impl(const JsonVariantImpl* data, double& value);
void get_value_impl(const JsonVariantImpl* data, fl::string& value);
void get_value_impl(const JsonVariantImpl* data, JsonObject& value);
void get_value_impl(const JsonVariantImpl* data, JsonArray& value);

void get_value_impl_mut(JsonVariantImpl* data, int** value);
void get_value_impl_mut(JsonVariantImpl* data, long** value);
void get_value_impl_mut(JsonVariantImpl* data, double** value);
void get_value_impl_mut(JsonVariantImpl* data, fl::string** value);
void get_value_impl_mut(JsonVariantImpl* data, JsonObject** value);
void get_value_impl_mut(JsonVariantImpl* data, JsonArray** value);

// Define the variant type for JSON values
struct JsonVariant {
    public:
        JsonVariant();
        ~JsonVariant();
        JsonVariant(bool value);
        JsonVariant(int value);
        JsonVariant(long value);
        JsonVariant(double value);
        JsonVariant(const fl::string& value);
        JsonVariant(const JsonObject& value);
        JsonVariant(const JsonArray& value);
        JsonVariant(JsonObject&& value);
        JsonVariant(JsonArray&& value);
    
        JsonVariant(const JsonVariant& other);
        JsonVariant(JsonVariant&& other) noexcept;
        JsonVariant& operator=(const JsonVariant& other);
        JsonVariant& operator=(JsonVariant&& other) noexcept;

        JsonVariant(JsonVariantImpl& impl);

        template<typename T>
        T get() const {
            T value;
            get_value_impl(data_.get(), value);
            return value;
        }

        template<typename T>
        T value_or(T defaultValue) const;

        template<typename T>
        T& get_mut() {
            T* value = nullptr;
            get_value_impl_mut(data_.get(), &value);
            return *value;
        }

        bool is_null() const;
        bool is_bool() const;
        bool is_int() const;
        bool is_long() const;
        bool is_double() const;
        bool is_string() const;
        bool is_object() const;
        bool is_array() const;
    
        fl::unique_ptr<JsonVariantImpl> data_;
}; 

void get_value(const JsonVariant* data, bool& value);
void get_value(const JsonVariant* data, int& value);
void get_value(const JsonVariant* data, long& value);
void get_value(const JsonVariant* data, double& value);
void get_value(const JsonVariant* data, fl::string& value);
void get_value(const JsonVariant* data, JsonObject& value);
void get_value(const JsonVariant* data, JsonArray& value);

void get_value_mut(JsonVariant* data, int& value);
void get_value_mut(JsonVariant* data, long& value);
void get_value_mut(JsonVariant* data, double& value);
void get_value_mut(JsonVariant* data, fl::string& value);
void get_value_mut(JsonVariant* data, JsonObject& value);
void get_value_mut(JsonVariant* data, JsonArray& value);

class JsonValue {
    public:
        JsonValue(); // Null constructor
        JsonValue(bool value);
        JsonValue(int value);
        JsonValue(long value);
        JsonValue(double value);
        JsonValue(const char* value);
        JsonValue(const fl::string& value);
        JsonValue(const JsonObject& value);
        JsonValue(const JsonArray& value);
        JsonValue(JsonObject&& value);
        JsonValue(JsonArray&& value);
    
        // Copy and Move constructors/assignment operators
        JsonValue(const JsonValue& other);
        JsonValue(JsonValue&& other) noexcept;
        JsonValue& operator=(const JsonValue& other);
        JsonValue& operator=(JsonValue&& other) noexcept;
    
        // Type checking
        bool is_null() const;
        bool is_bool() const;
        bool is_int() const;
        bool is_long() const;
        bool is_double() const;
        bool is_string() const;
        bool is_object() const;
        bool is_array() const;
    
        // Getters with type conversion
        template<typename T>
        T get() const {
            T value;
            get_value(data_.get(), value);
            return value;
        }

        fl::vector<fl::string> getObjectKeys() const;

        int getSize() const {
            return 0;
        }


    
        // Operator for default values (e.g., json["key"] | defaultValue)
        template<typename T>
        T value_or(T defaultValue) const;
    
        // Accessors for object and array types
        JsonValue& operator[](const fl::string& key);
        const JsonValue& operator[](const fl::string& key) const;
        JsonValue& operator[](size_t index);
        const JsonValue& operator[](size_t index) const;
    
        // Array specific methods
        void push_back(const JsonValue& value);
        void push_back(JsonValue&& value);
        size_t size() const;
    
        // String conversion (for debugging/serialization)
        fl::string to_string() const;
    
    private:
        fl::unique_ptr<JsonVariant> data_;
    
        // Helper to get a mutable reference to the underlying object/array
        JsonObject& get_object_mut();
        JsonArray& get_array_mut();
    };
    
class JsonObject {
public:
    JsonValue& operator[](const fl::string& key);
    const JsonValue& operator[](const fl::string& key) const;
    fl::string to_string() const; // For serialization

private:
    fl::hash_map<fl::string, JsonValue> members_;
};



class JsonArray {
public:
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;
    void push_back(const JsonValue& value);
    void push_back(JsonValue&& value);
    size_t size() const;
    fl::string to_string() const; // For serialization

private:
    fl::vector<JsonValue> elements_;
};





class Json {
public:
    static Json parse(const char* jsonString);
    static fl::string parseJson(const char* jsonString, Json* doc);

    Json() = default;
    Json(const Json& other) = default;
    Json(Json&& other) noexcept = default;
    Json& operator=(const Json& other) = default;
    Json& operator=(Json&& other) noexcept = default;


    bool has_value() const;

    // Forwarding methods from JsonValue for convenience when Json represents the root
    bool is_null() const { return root_.has_value() && (*root_).is_null(); }
    bool is_bool() const { return root_.has_value() && (*root_).is_bool(); }
    bool is_int() const { return root_.has_value() && (*root_).is_int(); }
    bool is_long() const { return root_.has_value() && (*root_).is_long(); }
    bool is_double() const { return root_.has_value() && (*root_).is_double(); }
    bool is_string() const { return root_.has_value() && (*root_).is_string(); }
    bool is_object() const { return root_.has_value() && (*root_).is_object(); }
    bool is_array() const { return root_.has_value() && (*root_).is_array(); }

    size_t size() const { return root_.has_value() ? (*root_).size() : 0; }

    template<typename T>
    T as() const {
        if (root_.has_value()) {
            return (*root_).get<T>();
        }
        return T(); // Return default-constructed T if no value
    }
    
    template<typename T>
    T value_or(T defaultValue) const {
        if (root_.has_value()) {
            return (*root_).value_or<T>(defaultValue);
        }
        return defaultValue;
    }
        
    Json& operator[](const fl::string& key);
    const Json& operator[](const fl::string& key) const;

    // Operator for array access at the root level
    Json operator[](size_t index);
    const Json operator[](size_t index) const;
    fl::size getSize() const { return root_.has_value() ? (*root_).size() : 0; }

    template<typename T>
    T operator|(T defaultValue) const {
        return value_or(defaultValue);
    }

    Json createNestedObject();
    Json createNestedArray();

private:
    fl::optional<JsonValue> root_;
};

fl::vector<fl::string> JsonValue::getObjectKeys() const {
    return fl::vector<fl::string>();
}


} // namespace fl

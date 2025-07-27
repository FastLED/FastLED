#pragma once

#include "fl/string.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/variant.h"
#include "fl/optional.h"
#include "fl/unique_ptr.h"
#include "fl/shared_ptr.h"
#include "fl/functional.h"

#include "fl/sketch_macros.h"

#ifndef FASTLED_ENABLE_JSON
#ifdef SKETCH_HAS_LOTS_OF_MEMORY
#define FASTLED_ENABLE_JSON 1
#else
#define FASTLED_ENABLE_JSON 0
#endif
#endif


namespace fl {


// Forward declarations
struct JsonValue;

// Define Array and Object as pointers to avoid incomplete type issues
// We'll use heap-allocated containers for these to avoid alignment issues
using JsonArray = fl::vector<fl::shared_ptr<JsonValue>>;
using JsonObject = fl::HashMap<fl::string, fl::shared_ptr<JsonValue>>;

// Function to get a reference to a static null JsonValue
JsonValue& get_null_value();

// AI - pay attention to this - implementing visitor pattern
template<typename T>
struct DefaultValueVisitor {
    const T& fallback;
    const T* result = nullptr;

    DefaultValueVisitor(const T& fb) : fallback(fb) {}

    // This is the method that fl::Variant expects
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }

    // Specific overload for the type T
    void operator()(const T& value) {
        result = &value;
    }

    // Generic overload for all other types
    template<typename U>
    void operator()(const U&) {
        // Do nothing for other types
    }
    
    // Special handling for nullptr_t
    void operator()(const fl::nullptr_t&) {
        // Do nothing - will return fallback
    }
};

// The JSON node
struct JsonValue {
    // Forward declarations for nested iterator classes
    class iterator;
    class const_iterator;
    
    // The variant holds exactly one of these alternatives
    using variant_t = fl::Variant<
        fl::nullptr_t,   // null
        bool,            // true/false
        int64_t,         // integer
        double,          // floating-point
        fl::string,      // string
        JsonArray,           // array
        JsonObject           // object
    >;

    typedef JsonValue::iterator iterator;
    typedef JsonValue::const_iterator const_iterator;

    variant_t data;

    // Constructors
    JsonValue() noexcept : data(nullptr) {}
    JsonValue(fl::nullptr_t) noexcept : data(nullptr) {}
    JsonValue(bool b) noexcept : data(b) {}
    JsonValue(int64_t i) noexcept : data(i) {}
    JsonValue(double d) noexcept : data(d) {}
    JsonValue(const fl::string& s) : data(s) {
        FASTLED_WARN("Created JsonValue with string: '" << s << "'");
        FASTLED_WARN("Tag value: " << data.tag());
        FASTLED_WARN("Is string: " << is_string());
        FASTLED_WARN("Is int: " << is_int());
    }
    JsonValue(const JsonArray& a) : data(a) {
        FASTLED_WARN("Created JsonValue with array");
    }
    JsonValue(const JsonObject& o) : data(o) {
        FASTLED_WARN("Created JsonValue with object");
    }

    JsonValue& operator=(const JsonValue& other) {
        data = other.data;
        return *this;
    }

    JsonValue& operator=(JsonValue&& other) {
        data = fl::move(other.data);
        return *this;
    }

    template<typename T>
    JsonValue& operator=(T&& value) {
        data = fl::forward<T>(value);
        return *this;
    }

    JsonValue& operator=(fl::nullptr_t) {
        data = nullptr;
        return *this;
    }

    JsonValue& operator=(bool b) {
        data = b;
        return *this;
    }

    JsonValue& operator=(int64_t i) {
        data = i;
        return *this;
    }

    JsonValue& operator=(double d) {
        data = d;
        return *this;
    }

    JsonValue& operator=(fl::string s) {
        data = fl::move(s);
        return *this;
    }

    JsonValue& operator=(JsonArray a) {
        data = fl::move(a);
        return *this;
    }


    
    // Special constructor for char values
    static fl::shared_ptr<JsonValue> from_char(char c) {
        return fl::make_shared<JsonValue>(fl::string(1, c));
    }
    
    // Visitor pattern implementation
    template<typename Visitor>
    auto visit(Visitor&& visitor) -> decltype(visitor(fl::nullptr_t{})) {
        return data.visit(fl::forward<Visitor>(visitor));
    }
    
    template<typename Visitor>
    auto visit(Visitor&& visitor) const -> decltype(visitor(fl::nullptr_t{})) {
        return data.visit(fl::forward<Visitor>(visitor));
    }

    // Type queries - using is<T>() instead of index() for fl::Variant
    bool is_null() const noexcept { 
        FASTLED_WARN("is_null called, tag=" << data.tag());
        return data.is<fl::nullptr_t>(); 
    }
    bool is_bool() const noexcept { 
        FASTLED_WARN("is_bool called, tag=" << data.tag());
        return data.is<bool>(); 
    }
    bool is_int() const noexcept { 
        FASTLED_WARN("is_int called, tag=" << data.tag());
        return data.is<int64_t>(); 
    }
    bool is_double() const noexcept { 
        FASTLED_WARN("is_double called, tag=" << data.tag());
        return data.is<double>(); 
    }
    bool is_string() const noexcept { 
        FASTLED_WARN("is_string called, tag=" << data.tag());
        return data.is<fl::string>(); 
    }
    bool is_array() const noexcept { 
        FASTLED_WARN("is_array called, tag=" << data.tag());
        return data.is<JsonArray>(); 
    }
    bool is_object() const noexcept { 
        FASTLED_WARN("is_object called, tag=" << data.tag());
        return data.is<JsonObject>(); 
    }

    // Safe extractors (return optional values, not references)
    fl::optional<bool> as_bool() {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() {
        auto ptr = data.ptr<int64_t>();
        return ptr ? fl::optional<int64_t>(*ptr) : fl::nullopt;
    }
    
    fl::optional<double> as_double() {
        auto ptr = data.ptr<double>();
        return ptr ? fl::optional<double>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::string> as_string() {
        auto ptr = data.ptr<fl::string>();
        return ptr ? fl::optional<fl::string>(*ptr) : fl::nullopt;
    }
    
    fl::optional<JsonArray> as_array() {
        auto ptr = data.ptr<JsonArray>();
        return ptr ? fl::optional<JsonArray>(*ptr) : fl::nullopt;
    }
    
    fl::optional<JsonObject> as_object() {
        auto ptr = data.ptr<JsonObject>();
        return ptr ? fl::optional<JsonObject>(*ptr) : fl::nullopt;
    }

    // Const overloads
    fl::optional<bool> as_bool() const {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() const {
        auto ptr = data.ptr<int64_t>();
        return ptr ? fl::optional<int64_t>(*ptr) : fl::nullopt;
    }
    
    fl::optional<double> as_double() const {
        auto ptr = data.ptr<double>();
        return ptr ? fl::optional<double>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::string> as_string() const {
        auto ptr = data.ptr<fl::string>();
        return ptr ? fl::optional<fl::string>(*ptr) : fl::nullopt;
    }
    
    fl::optional<JsonArray> as_array() const {
        auto ptr = data.ptr<JsonArray>();
        return ptr ? fl::optional<JsonArray>(*ptr) : fl::nullopt;
    }
    
    fl::optional<JsonObject> as_object() const {
        auto ptr = data.ptr<JsonObject>();
        return ptr ? fl::optional<JsonObject>(*ptr) : fl::nullopt;
    }
    
    // Generic getter template method
    template<typename T>
    fl::optional<T> get() const {
        auto ptr = data.ptr<T>();
        return ptr ? fl::optional<T>(*ptr) : fl::nullopt;
    }
    
    template<typename T>
    fl::optional<T> get() {
        auto ptr = data.ptr<T>();
        return ptr ? fl::optional<T>(*ptr) : fl::nullopt;
    }

    // Iterator support for objects
    iterator begin() {
        if (!is_object()) {
                static JsonObject empty_obj;
            return iterator(empty_obj.begin());
        }
        auto ptr = data.ptr<JsonObject>();
        return iterator(ptr->begin());
    }
    
    iterator end() {
        if (!is_object()) {
            static JsonObject empty_obj;
            return iterator(empty_obj.end());
        }
        auto ptr = data.ptr<JsonObject>();
        return iterator(ptr->end());
    }
    
    const_iterator begin() const {
        if (!is_object()) {
            static JsonObject empty_obj;
            return const_iterator::from_iterator(empty_obj.begin());
        }
        auto ptr = data.ptr<const JsonObject>();
        if (!ptr) return const_iterator::from_iterator(JsonObject().begin());
        return const_iterator::from_iterator(ptr->begin());
    }
    
    const_iterator end() const {
        if (!is_object()) {
            static JsonObject empty_obj;
            return const_iterator::from_iterator(empty_obj.end());
        }
        auto ptr = data.ptr<const JsonObject>();
        if (!ptr) return const_iterator::from_iterator(JsonObject().end());
        return const_iterator::from_iterator(ptr->end());
    }
    
    // Free functions for range-based for loops
    friend iterator begin(JsonValue& v) { return v.begin(); }
    friend iterator end(JsonValue& v) { return v.end(); }
    friend const_iterator begin(const JsonValue& v) { return v.begin(); }
    friend const_iterator end(const JsonValue& v) { return v.end(); }

    // Indexing for fluid chaining
    JsonValue& operator[](size_t idx) {
        if (!is_array()) data = JsonArray{};
        auto ptr = data.ptr<JsonArray>();
        if (!ptr) return get_null_value(); // Handle error case
        auto &arr = *ptr;
        if (idx >= arr.size()) {
            // Resize array and fill with null values
            for (size_t i = arr.size(); i <= idx; i++) {
                arr.push_back(fl::make_shared<JsonValue>());
            }
        }
        if (idx >= arr.size()) return get_null_value(); // Handle error case
        return *arr[idx];
    }
    
    JsonValue& operator[](const fl::string &key) {
        if (!is_object()) data = JsonObject{};
        auto ptr = data.ptr<JsonObject>();
        if (!ptr) return get_null_value(); // Handle error case
        auto &obj = *ptr;
        if (obj.find(key) == obj.end()) {
            // Create a new entry if key doesn't exist
            obj[key] = fl::make_shared<JsonValue>();
        }
        return *obj[key];
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(const T& fallback) const {
        DefaultValueVisitor<T> visitor(fallback);
        data.visit(visitor);
        return visitor.result ? *visitor.result : fallback;
    }
    
    // Explicit method for default values (alternative to operator|)
    template<typename T>
    T as_or(const T& fallback) const {
        DefaultValueVisitor<T> visitor(fallback);
        data.visit(visitor);
        return visitor.result ? *visitor.result : fallback;
    }

    // Contains methods for checking existence
    bool contains(size_t idx) const {
        if (!is_array()) return false;
        auto ptr = data.ptr<JsonArray>();
        return ptr && idx < ptr->size();
    }
    
    bool contains(const fl::string &key) const {
        if (!is_object()) return false;
        auto ptr = data.ptr<JsonObject>();
        return ptr && ptr->find(key) != ptr->end();
    }

    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const {
        fl::vector<fl::string> result;
        if (is_object()) {
            for (auto it = begin(); it != end(); ++it) {
                auto keyValue = *it;
                result.push_back(keyValue.first);
            }
        }
        return result;
    }
    
    // Backward compatibility method
    fl::vector<fl::string> getObjectKeys() const { return keys(); }

    // Size methods
    size_t size() const {
        if (is_array()) {
            auto ptr = data.ptr<JsonArray>();
            return ptr ? ptr->size() : 0;
        } else if (is_object()) {
            auto ptr = data.ptr<JsonObject>();
            return ptr ? ptr->size() : 0;
        }
        return 0;
    }

    // Serialization
    fl::string to_string() const;
    
    // Visitor-based serialization helper
    friend class SerializerVisitor;

    // Parsing factory (FLArduinoJson implementation)
    static fl::shared_ptr<JsonValue> parse(const fl::string &txt);
    
    // Iterator support for objects
    class iterator {
    private:
        JsonObject::iterator m_iter;
        
    public:
        iterator() = default;
        iterator(JsonObject::iterator iter) : m_iter(iter) {}
        
        // Getter for const iterator conversion
        JsonObject::iterator get_iter() const { return m_iter; }
        
        iterator& operator++() {
            ++m_iter;
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        
        bool operator!=(const iterator& other) const {
            return m_iter != other.m_iter;
        }
        
        bool operator==(const iterator& other) const {
            return m_iter == other.m_iter;
        }
        
        struct KeyValue {
            fl::string first;
            JsonValue& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<JsonValue>& value_ptr) 
                : first(key), second(value_ptr ? *value_ptr : get_null_value()) {}
        };
        
        KeyValue operator*() const {
            return KeyValue(m_iter->first, m_iter->second);
        }
        
        // Remove operator-> to avoid static variable issues
    };
    
    // Iterator for JSON objects (const version)
    class const_iterator {
    private:
        JsonObject::const_iterator m_iter;
        
    public:
        const_iterator() = default;
        const_iterator(JsonObject::const_iterator iter) : m_iter(iter) {}
        
        // Factory method for conversion from iterator
        static const_iterator from_object_iterator(const iterator& other) {
            JsonObject::const_iterator const_iter(other.get_iter());
            return const_iterator(const_iter);
        }
        
        // Factory method for conversion from Object::iterator
        static const_iterator from_iterator(JsonObject::const_iterator iter) {
            return const_iterator(iter);
        }
        
        const_iterator& operator++() {
            ++m_iter;
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        
        bool operator!=(const const_iterator& other) const {
            return m_iter != other.m_iter;
        }
        
        bool operator==(const const_iterator& other) const {
            return m_iter == other.m_iter;
        }
        
        struct KeyValue {
            fl::string first;
            const JsonValue& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<JsonValue>& value_ptr) 
                : first(key), second(value_ptr ? *value_ptr : get_null_value()) {}
        };
        
        KeyValue operator*() const {
            return KeyValue(m_iter->first, m_iter->second);
        }
        
        // Remove operator-> to avoid static variable issues
    };
};

// Function to get a reference to a static null JsonValue
JsonValue& get_null_value();

// Main Json class that provides a more fluid and user-friendly interface
class Json {
private:
    fl::shared_ptr<JsonValue> m_value;

public:
    // Constructors
    Json() : m_value() {}  // Default initialize to nullptr
    Json(fl::nullptr_t) : m_value(fl::make_shared<JsonValue>(nullptr)) {}
    Json(bool b) : m_value(fl::make_shared<JsonValue>(b)) {}
    Json(int i) : m_value(fl::make_shared<JsonValue>(static_cast<int64_t>(i))) {}
    Json(int64_t i) : m_value(fl::make_shared<JsonValue>(i)) {}
    Json(float f) : m_value(fl::make_shared<JsonValue>(static_cast<double>(f))) {}
    Json(double d) : m_value(fl::make_shared<JsonValue>(d)) {}
    Json(const fl::string& s) : m_value(fl::make_shared<JsonValue>(s)) {}
    Json(const char* s): Json(fl::string(s)) {}
    Json(JsonArray a) : m_value(fl::make_shared<JsonValue>(fl::move(a))) {}
    Json(JsonObject o) : m_value(fl::make_shared<JsonValue>(fl::move(o))) {}
    // Constructor from shared_ptr<JsonValue>
    Json(const fl::shared_ptr<JsonValue>& value) : m_value(value) {}
    
    // Constructor for fl::vector<float> - converts to JSON array
    Json(const fl::vector<float>& vec) : m_value(fl::make_shared<JsonValue>(JsonArray{})) {
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<JsonValue>(static_cast<double>(item)));
            }
        }
    }
    
    // Special constructor for char values
    static Json from_char(char c) {
        Json result;
        auto value = fl::make_shared<JsonValue>(fl::string(1, c));
        FASTLED_WARN("Created JsonValue with string: " << value->is_string() << ", int: " << value->is_int());
        result.m_value = value;
        FASTLED_WARN("Json has string: " << result.is_string() << ", int: " << result.is_int());
        return result;
    }

    // Copy constructor
    Json(const Json& other) : m_value(other.m_value) {}

    // Assignment operator
    Json& operator=(const Json& other) {
        FL_WARN("Json& operator=(const Json& other): " << (other.m_value ? other.m_value.get() : 0));
        if (this != &other) {
            m_value = other.m_value;
        }
        return *this;
    }

    Json& operator=(Json&& other) {
        if (this != &other) {
            m_value = fl::move(other.m_value);
        }
        return *this;
    }
    
    // Assignment operators for primitive types to avoid ambiguity
    Json& operator=(bool value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(int value) {
        m_value = fl::make_shared<JsonValue>(static_cast<int64_t>(value));
        return *this;
    }
    
    Json& operator=(float value) {
        m_value = fl::make_shared<JsonValue>(static_cast<double>(value));
        return *this;
    }
    
    Json& operator=(double value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(const fl::string& value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(const char* value) {
        m_value = fl::make_shared<JsonValue>(fl::string(value));
        return *this;
    }
    
    // Assignment operator for fl::vector<float>
    Json& operator=(fl::vector<float> vec) {
        m_value = fl::make_shared<JsonValue>(JsonArray{});
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<JsonValue>(static_cast<double>(item)));
            }
        }
        return *this;
    }

    // Type queries
    bool is_null() const { return m_value ? m_value->is_null() : true; }
    bool is_bool() const { return m_value && m_value->is_bool(); }
    bool is_int() const { return m_value && m_value->is_int(); }
    bool is_double() const { return m_value && m_value->is_double(); }
    bool is_string() const { return m_value && m_value->is_string(); }
    bool is_array() const { return m_value && m_value->is_array(); }
    bool is_object() const { return m_value && m_value->is_object(); }

    // Safe extractors
    fl::optional<bool> as_bool() const { return m_value ? m_value->as_bool() : fl::nullopt; }
    fl::optional<int64_t> as_int() const { return m_value ? m_value->as_int() : fl::nullopt; }
    fl::optional<double> as_double() const { return m_value ? m_value->as_double() : fl::nullopt; }
    fl::optional<fl::string> as_string() const { return m_value ? m_value->as_string() : fl::nullopt; }
    fl::optional<JsonArray> as_array() const { return m_value ? m_value->as_array() : fl::nullopt; }
    fl::optional<JsonObject> as_object() const { return m_value ? m_value->as_object() : fl::nullopt; }

    template<typename T>
    fl::optional<T> as() {
        return m_value ? m_value->get<T>() : fl::nullopt;
    }

    // Iterator support for objects
    JsonValue::iterator begin() { 
        if (!m_value) return JsonValue::iterator(JsonObject().begin());
        return m_value->begin(); 
    }
    JsonValue::iterator end() { 
        if (!m_value) return JsonValue::iterator(JsonObject().end());
        return m_value->end(); 
    }
    JsonValue::const_iterator begin() const { 
        if (!m_value) return JsonValue::const_iterator::from_iterator(JsonObject().begin());
        return JsonValue::const_iterator::from_object_iterator(m_value->begin()); 
    }
    JsonValue::const_iterator end() const { 
        if (!m_value) return JsonValue::const_iterator::from_iterator(JsonObject().end());
        return JsonValue::const_iterator::from_object_iterator(m_value->end()); 
    }
    
    // Free functions for range-based for loops
    friend JsonValue::iterator begin(Json& j) { return j.begin(); }
    friend JsonValue::iterator end(Json& j) { return j.end(); }
    friend JsonValue::const_iterator begin(const Json& j) { return j.begin(); }
    friend JsonValue::const_iterator end(const Json& j) { return j.end(); }

    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const {
        fl::vector<fl::string> result;
        if (m_value && m_value->is_object()) {
            for (auto it = begin(); it != end(); ++it) {
                auto keyValue = *it;
                result.push_back(keyValue.first);
            }
        }
        return result;
    }
    
    // Backward compatibility method
    fl::vector<fl::string> getObjectKeys() const { return keys(); }

    // Indexing for fluid chaining
    Json& operator[](size_t idx) {
        if (!m_value) {
            m_value = fl::make_shared<JsonValue>(JsonArray{});
        }
        return *reinterpret_cast<Json*>(&(*m_value)[idx]);
    }
    
    const Json operator[](size_t idx) const {
        if (!m_value || !m_value->is_array()) {
            return Json(nullptr);
        }
        auto arr = m_value->as_array();
        if (arr && idx < arr->size()) {
            return Json((*arr)[idx]);
        }
        return Json(nullptr);
    }
    
    Json& operator[](const fl::string &key) {
        if (!m_value) {
            m_value = fl::make_shared<JsonValue>(JsonObject{});
        }
        FASTLED_WARN("Accessing key '" << key << "' in object");
        return *reinterpret_cast<Json*>(&(*m_value)[key]);
    }
    
    const Json operator[](const fl::string &key) const {
        if (!m_value || !m_value->is_object()) {
            return Json(nullptr);
        }
        auto obj = m_value->as_object();
        if (obj && obj->find(key) != obj->end()) {
            return Json((*obj)[key]);
        }
        return Json(nullptr);
    }

    // Contains methods for checking existence
    bool contains(size_t idx) const { 
        return m_value && m_value->contains(idx); 
    }
    bool contains(const fl::string &key) const { 
        return m_value && m_value->contains(key); 
    }

    // Size method
    size_t size() const { 
        return m_value ? m_value->size() : 0; 
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(const T& fallback) const {
        if (!m_value) return fallback;
        return (*m_value) | fallback;
    }
    
    // Explicit method for default values (alternative to operator|)
    template<typename T>
    T as_or(const T& fallback) const {
        if (!m_value) return fallback;
        return m_value->as_or(fallback);
    }

    // has_value method for compatibility
    bool has_value() const { 
        return m_value && !m_value->is_null(); 
    }

    // Serialization - now delegates to native implementation
    fl::string to_string() const { return to_string_native(); }
    
    // Native serialization (without external libraries)
    fl::string to_string_native() const;

    // Parsing factory method
    static Json parse(const fl::string &txt) {
        auto parsed = JsonValue::parse(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
    }

    // Convenience methods for creating arrays and objects
    static Json array() {
        return Json(JsonArray{});
    }

    static Json object() {
        return Json(JsonObject{});
    }
    
    // Compatibility with existing API for array/object access
    size_t getSize() const { return size(); }
    
    // Set methods for building objects
    void set(const fl::string& key, const Json& value) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<JsonValue>(JsonObject{});
        }
        // Directly assign the value to the object without going through Json::operator[]
        auto objPtr = m_value->data.ptr<JsonObject>();
        if (objPtr) {
            FASTLED_WARN("Setting key '" << key << "' in object");
            FASTLED_WARN("Value is_string: " << value.is_string());
            FASTLED_WARN("Value is_int: " << value.is_int());
            // Create or update the entry directly
            (*objPtr)[key] = value.m_value;
        }
    }
    
    void set(const fl::string& key, bool value) { set(key, Json(value)); }
    void set(const fl::string& key, int value) { set(key, Json(value)); }
    void set(const fl::string& key, int64_t value) { set(key, Json(value)); }
    void set(const fl::string& key, float value) { set(key, Json(value)); }
    void set(const fl::string& key, double value) { set(key, Json(value)); }
    void set(const fl::string& key, const fl::string& value) { set(key, Json(value)); }
    void set(const fl::string& key, const char* value) { set(key, Json(value)); }
    template<typename T, typename = fl::enable_if_t<fl::is_same<T, char>::value>>
    void set(const fl::string& key, T value) { set(key, Json(value)); }
    
    // Array push_back methods
    void push_back(const Json& value) {
        if (!m_value || !m_value->is_array()) {
            m_value = fl::make_shared<JsonValue>(JsonArray{});
        }
        // For arrays, we need to manually handle the insertion since our indexing
        // mechanism auto-creates elements
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            ptr->push_back(value.m_value);
        }
    }
    
    // Create methods for compatibility
    static Json createArray() { return Json::array(); }
    static Json createObject() { return Json::object(); }
    
    // Serialize method for compatibility
    fl::string serialize() const { return to_string(); }

    // Helper function to normalize JSON string (remove whitespace)
    static fl::string normalizeJsonString(const char* jsonStr);
};

} // namespace fl

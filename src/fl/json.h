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
struct Value;

// Define Array and Object as pointers to avoid incomplete type issues
// We'll use heap-allocated containers for these to avoid alignment issues
using Array = fl::vector<fl::shared_ptr<Value>>;
using Object = fl::HashMap<fl::string, fl::shared_ptr<Value>>;

// Function to get a reference to a static null Value
Value& get_null_value();

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
struct Value {
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
        Array,           // array
        Object           // object
    >;

    typedef Value::iterator iterator;
    typedef Value::const_iterator const_iterator;

    variant_t data;

    // Constructors
    Value() noexcept : data(nullptr) {}
    Value(fl::nullptr_t) noexcept : data(nullptr) {}
    Value(bool b) noexcept : data(b) {}
    Value(int64_t i) noexcept : data(i) {}
    Value(double d) noexcept : data(d) {}
    Value(const fl::string& s) : data(s) {
        FASTLED_WARN("Created Value with string, is_string: " << is_string() << ", is_int: " << is_int());
    }
    Value(const Array& a) : data(a) {

    }
    Value(const Object& o) : data(o) {

    }

    Value& operator=(const Value& other) {
        data = other.data;
        return *this;
    }

    Value& operator=(Value&& other) {
        data = fl::move(other.data);
        return *this;
    }

    template<typename T>
    Value& operator=(T&& value) {
        data = fl::forward<T>(value);
        return *this;
    }

    Value& operator=(fl::nullptr_t) {
        data = nullptr;
        return *this;
    }

    Value& operator=(bool b) {
        data = b;
        return *this;
    }

    Value& operator=(int64_t i) {
        data = i;
        return *this;
    }

    Value& operator=(double d) {
        data = d;
        return *this;
    }

    Value& operator=(fl::string s) {
        data = fl::move(s);
        return *this;
    }

    Value& operator=(Array a) {
        data = fl::move(a);
        return *this;
    }


    
    // Special constructor for char values
    static fl::shared_ptr<Value> from_char(char c) {
        return fl::make_shared<Value>(fl::string(1, c));
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
    bool is_null() const noexcept { return data.is<fl::nullptr_t>(); }
    bool is_bool() const noexcept { return data.is<bool>(); }
    bool is_int() const noexcept { return data.is<int64_t>(); }
    bool is_double() const noexcept { return data.is<double>(); }
    bool is_string() const noexcept { return data.is<fl::string>(); }
    bool is_array() const noexcept { return data.is<Array>(); }
    bool is_object() const noexcept { return data.is<Object>(); }

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
    
    fl::optional<Array> as_array() {
        auto ptr = data.ptr<Array>();
        return ptr ? fl::optional<Array>(*ptr) : fl::nullopt;
    }
    
    fl::optional<Object> as_object() {
        auto ptr = data.ptr<Object>();
        return ptr ? fl::optional<Object>(*ptr) : fl::nullopt;
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
    
    fl::optional<Array> as_array() const {
        auto ptr = data.ptr<Array>();
        return ptr ? fl::optional<Array>(*ptr) : fl::nullopt;
    }
    
    fl::optional<Object> as_object() const {
        auto ptr = data.ptr<Object>();
        return ptr ? fl::optional<Object>(*ptr) : fl::nullopt;
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
            static Object empty_obj;
            return iterator(empty_obj.begin());
        }
        auto ptr = data.ptr<Object>();
        return iterator(ptr->begin());
    }
    
    iterator end() {
        if (!is_object()) {
            static Object empty_obj;
            return iterator(empty_obj.end());
        }
        auto ptr = data.ptr<Object>();
        return iterator(ptr->end());
    }
    
    const_iterator begin() const {
        if (!is_object()) {
            static Object empty_obj;
            return const_iterator::from_iterator(empty_obj.begin());
        }
        auto ptr = data.ptr<const Object>();
        if (!ptr) return const_iterator::from_iterator(Object().begin());
        return const_iterator::from_iterator(ptr->begin());
    }
    
    const_iterator end() const {
        if (!is_object()) {
            static Object empty_obj;
            return const_iterator::from_iterator(empty_obj.end());
        }
        auto ptr = data.ptr<const Object>();
        if (!ptr) return const_iterator::from_iterator(Object().end());
        return const_iterator::from_iterator(ptr->end());
    }
    
    // Free functions for range-based for loops
    friend iterator begin(Value& v) { return v.begin(); }
    friend iterator end(Value& v) { return v.end(); }
    friend const_iterator begin(const Value& v) { return v.begin(); }
    friend const_iterator end(const Value& v) { return v.end(); }

    // Indexing for fluid chaining
    Value& operator[](size_t idx) {
        if (!is_array()) data = Array{};
        auto ptr = data.ptr<Array>();
        if (!ptr) return get_null_value(); // Handle error case
        auto &arr = *ptr;
        if (idx >= arr.size()) {
            // Resize array and fill with null values
            for (size_t i = arr.size(); i <= idx; i++) {
                arr.push_back(fl::make_shared<Value>());
            }
        }
        if (idx >= arr.size()) return get_null_value(); // Handle error case
        return *arr[idx];
    }
    
    Value& operator[](const fl::string &key) {
        if (!is_object()) data = Object{};
        auto ptr = data.ptr<Object>();
        if (!ptr) return get_null_value(); // Handle error case
        auto &obj = *ptr;
        if (obj.find(key) == obj.end()) {
            // Create a new entry if key doesn't exist
            obj[key] = fl::make_shared<Value>();
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
        auto ptr = data.ptr<Array>();
        return ptr && idx < ptr->size();
    }
    
    bool contains(const fl::string &key) const {
        if (!is_object()) return false;
        auto ptr = data.ptr<Object>();
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
            auto ptr = data.ptr<Array>();
            return ptr ? ptr->size() : 0;
        } else if (is_object()) {
            auto ptr = data.ptr<Object>();
            return ptr ? ptr->size() : 0;
        }
        return 0;
    }

    // Serialization
    fl::string to_string() const;
    
    // Visitor-based serialization helper
    friend class SerializerVisitor;

    // Parsing factory (FLArduinoJson implementation)
    static fl::shared_ptr<Value> parse(const fl::string &txt);
    
    // Iterator support for objects
    class iterator {
    private:
        Object::iterator m_iter;
        
    public:
        iterator() = default;
        iterator(Object::iterator iter) : m_iter(iter) {}
        
        // Getter for const iterator conversion
        Object::iterator get_iter() const { return m_iter; }
        
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
            Value& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<Value>& value_ptr) 
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
        Object::const_iterator m_iter;
        
    public:
        const_iterator() = default;
        const_iterator(Object::const_iterator iter) : m_iter(iter) {}
        
        // Factory method for conversion from iterator
        static const_iterator from_object_iterator(const iterator& other) {
            Object::const_iterator const_iter(other.get_iter());
            return const_iterator(const_iter);
        }
        
        // Factory method for conversion from Object::iterator
        static const_iterator from_iterator(Object::const_iterator iter) {
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
            const Value& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<Value>& value_ptr) 
                : first(key), second(value_ptr ? *value_ptr : get_null_value()) {}
        };
        
        KeyValue operator*() const {
            return KeyValue(m_iter->first, m_iter->second);
        }
        
        // Remove operator-> to avoid static variable issues
    };
};

// Function to get a reference to a static null Value
Value& get_null_value();

// Main Json class that provides a more fluid and user-friendly interface
class Json {
private:
    fl::shared_ptr<Value> m_value;

public:
    // Constructors
    Json() : m_value() {}  // Default initialize to nullptr
    Json(fl::nullptr_t) : m_value(fl::make_shared<Value>(nullptr)) {}
    Json(bool b) : m_value(fl::make_shared<Value>(b)) {}
    Json(int i) : m_value(fl::make_shared<Value>(static_cast<int64_t>(i))) {}
    Json(int64_t i) : m_value(fl::make_shared<Value>(i)) {}
    Json(float f) : m_value(fl::make_shared<Value>(static_cast<double>(f))) {}
    Json(double d) : m_value(fl::make_shared<Value>(d)) {}
    Json(const fl::string& s) : m_value(fl::make_shared<Value>(s)) {}
    Json(const char* s): Json(fl::string(s)) {}
    Json(Array a) : m_value(fl::make_shared<Value>(fl::move(a))) {}
    Json(Object o) : m_value(fl::make_shared<Value>(fl::move(o))) {}
    // Constructor from shared_ptr<Value>
    Json(const fl::shared_ptr<Value>& value) : m_value(value) {}
    
    // Constructor for fl::vector<float> - converts to JSON array
    Json(const fl::vector<float>& vec) : m_value(fl::make_shared<Value>(Array{})) {
        auto ptr = m_value->data.ptr<Array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<Value>(static_cast<double>(item)));
            }
        }
    }
    
    // Special constructor for char values
    static Json from_char(char c) {
        Json result;
        auto value = fl::make_shared<Value>(fl::string(1, c));
        FASTLED_WARN("Created Value with string: " << value->is_string() << ", int: " << value->is_int());
        result.m_value = value;
        FASTLED_WARN("Json has string: " << result.is_string() << ", int: " << result.is_int());
        return result;
    }

    // Copy constructor
    Json(const Json& other) : m_value(other.m_value) {}

    // Assignment operator
    Json& operator=(const Json& other) {
        FL_WARN("Json& operator=(const Json& other): " << other.m_value.get());
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
        m_value = fl::make_shared<Value>(value);
        return *this;
    }
    
    Json& operator=(int value) {
        m_value = fl::make_shared<Value>(static_cast<int64_t>(value));
        return *this;
    }
    
    Json& operator=(float value) {
        m_value = fl::make_shared<Value>(static_cast<double>(value));
        return *this;
    }
    
    Json& operator=(double value) {
        m_value = fl::make_shared<Value>(value);
        return *this;
    }
    
    Json& operator=(const fl::string& value) {
        m_value = fl::make_shared<Value>(value);
        return *this;
    }
    
    Json& operator=(const char* value) {
        m_value = fl::make_shared<Value>(fl::string(value));
        return *this;
    }
    
    // Assignment operator for fl::vector<float>
    Json& operator=(fl::vector<float> vec) {
        m_value = fl::make_shared<Value>(Array{});
        auto ptr = m_value->data.ptr<Array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<Value>(static_cast<double>(item)));
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
    fl::optional<Array> as_array() const { return m_value ? m_value->as_array() : fl::nullopt; }
    fl::optional<Object> as_object() const { return m_value ? m_value->as_object() : fl::nullopt; }

    template<typename T>
    fl::optional<T> as() {
        return m_value ? m_value->get<T>() : fl::nullopt;
    }

    // Iterator support for objects
    Value::iterator begin() { 
        if (!m_value) return Value::iterator(Object().begin());
        return m_value->begin(); 
    }
    Value::iterator end() { 
        if (!m_value) return Value::iterator(Object().end());
        return m_value->end(); 
    }
    Value::const_iterator begin() const { 
        if (!m_value) return Value::const_iterator::from_iterator(Object().begin());
        return Value::const_iterator::from_object_iterator(m_value->begin()); 
    }
    Value::const_iterator end() const { 
        if (!m_value) return Value::const_iterator::from_iterator(Object().end());
        return Value::const_iterator::from_object_iterator(m_value->end()); 
    }
    
    // Free functions for range-based for loops
    friend Value::iterator begin(Json& j) { return j.begin(); }
    friend Value::iterator end(Json& j) { return j.end(); }
    friend Value::const_iterator begin(const Json& j) { return j.begin(); }
    friend Value::const_iterator end(const Json& j) { return j.end(); }

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
            m_value = fl::make_shared<Value>(Array{});
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
            m_value = fl::make_shared<Value>(Object{});
        }
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
        auto parsed = Value::parse(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
    }

    // Convenience methods for creating arrays and objects
    static Json array() {
        return Json(Array{});
    }

    static Json object() {
        return Json(Object{});
    }
    
    // Compatibility with existing API for array/object access
    size_t getSize() const { return size(); }
    
    // Set methods for building objects
    void set(const fl::string& key, const Json& value) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<Value>(Object{});
        }
        (*this)[key] = value;
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
            m_value = fl::make_shared<Value>(Array{});
        }
        // For arrays, we need to manually handle the insertion since our indexing
        // mechanism auto-creates elements
        auto ptr = m_value->data.ptr<Array>();
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

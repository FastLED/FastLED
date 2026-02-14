
#include "fl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/math.h" // For floor function
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/detail/private.h"
#include "fl/log.h"
#include "fl/math_macros.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/limits.h"
#include "fl/stl/move.h"
#include "fl/stl/strstream.h"
#include "fl/stl/shared_ptr.h"

// fl::numeric_limits<i16>::min(), fl::numeric_limits<i16>::max(), and fl::numeric_limits<u8>::max() should come from the platform's
// <stdint.h> or <cstdint> headers (via fl/stl/stdint.h).
// FastLED no longer defines these macros to avoid conflicts with system headers.


#if FASTLED_ENABLE_JSON

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_NULL_DEREFERENCE
#include "third_party/arduinojson/json.h"  // IWYU pragma: keep
FL_DISABLE_WARNING_POP

#endif  // FASTLED_ENABLE_JSON

namespace fl {




// Helper function to check if a double can be reasonably represented as a float
// Used for debug logging - may appear unused in release builds
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(unused-function)
static bool canBeRepresentedAsFloat(double value) {

    
    auto isnan = [](double value) -> bool {
        return value != value;
    };
    
    // Check for special values
    if (isnan(value)) {
        return true; // These can be represented as float
    }
    
    // Check if the value is within reasonable float range
    // Reject values that are clearly beyond float precision (beyond 2^24 for integers)
    // or outside the float range
    if (fl::fl_abs(value) > 16777216.0) { // 2^24 - beyond which floats lose integer precision
        return false;
    }
    
    // For values within reasonable range, allow conversion even with minor precision loss
    // This handles cases like 300000.14159 which should be convertible to float
    // even though it loses some precision
    return true;
}
FL_DISABLE_WARNING_POP    


JsonValue& get_null_value() {
    static JsonValue null_value;
    return null_value;
}

JsonObject& get_empty_json_object() {
    static JsonObject empty_object;
    return empty_object;
}

fl::shared_ptr<JsonValue> JsonValue::parse(const fl::string& txt) {
    #if !FASTLED_ENABLE_JSON
    return fl::make_shared<JsonValue>(fl::string(txt));
    #else
    // Determine the size of the JsonDocument needed.
    FLArduinoJson::JsonDocument doc;

    FLArduinoJson::DeserializationError error = FLArduinoJson::deserializeJson(doc, txt.c_str());

    if (error) {
        const char* errorMsg = error.c_str();
        FL_WARN("JSON parsing failed: " << (errorMsg ? errorMsg : "<null error message>"));
        return fl::make_shared<JsonValue>(nullptr); // Return null on error
    }

    // Iterative converter using work stack + result stack.
    // Avoids recursion which costs ~1,200 bytes/level on the call stack.

    enum class ParseAction {
        CONVERT,         // Convert a JsonVariantConst, push result
        ASSEMBLE_ARRAY,  // Pop n results, build JsonArray, push result
        ASSEMBLE_OBJECT  // Pop n results, build JsonObject with keys, push result
    };

    struct ParseWork {
        ParseAction action;
        FLArduinoJson::JsonVariantConst src; // for CONVERT
        size_t count;                         // for ASSEMBLE
        fl::vector<fl::string> keys;          // for ASSEMBLE_OBJECT
    };

    // Array type classification (same logic as before, no recursion)
    enum ArrayType {
        ALL_UINT8,
        ALL_INT16,
        ALL_FLOATS,
        GENERIC_ARRAY
    };

    struct ArrayTypeInfo {
        bool isUint8 = true;
        bool isInt16 = true;
        bool isFloat = true;

        void disableAll() {
            isUint8 = false;
            isInt16 = false;
            isFloat = false;
        }

        void checkNumericValue(double val) {
            bool isInteger = val == floor(val);
            if (!isInteger || val < 0 || val > (fl::numeric_limits<u8>::max)()) {
                isUint8 = false;
            }
            if (!isInteger || val < (fl::numeric_limits<i16>::min)() || val > (fl::numeric_limits<i16>::max)()) {
                isInt16 = false;
            }
            if (!canBeRepresentedAsFloat(val)) {
                isFloat = false;
            }
        }

        void checkIntegerValue(i64 val) {
            if (val < 0 || val > (fl::numeric_limits<u8>::max)()) {
                isUint8 = false;
            }
            if (val < (fl::numeric_limits<i16>::min)() || val > (fl::numeric_limits<i16>::max)()) {
                isInt16 = false;
            }
            if (val < -16777216 || val > 16777216) {
                isFloat = false;
            }
        }

        ArrayType getBestType() const {
            if (isUint8) return ALL_UINT8;
            if (isInt16) return ALL_INT16;
            if (isFloat) return ALL_FLOATS;
            return GENERIC_ARRAY;
        }
    };

    fl::vector<ParseWork> work_stack;
    fl::vector<fl::shared_ptr<JsonValue>> result_stack;
    work_stack.reserve(16);
    result_stack.reserve(16);

    // Push root conversion
    ParseWork root_work;
    root_work.action = ParseAction::CONVERT;
    root_work.src = doc.as<FLArduinoJson::JsonVariantConst>();
    root_work.count = 0;
    work_stack.push_back(fl::move(root_work));

    while (!work_stack.empty()) {
        ParseWork item = fl::move(work_stack.back());
        work_stack.pop_back();

        switch (item.action) {
            case ParseAction::CONVERT: {
                const auto& src = item.src;

                if (src.isNull()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(nullptr));
                } else if (src.is<bool>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(src.as<bool>()));
                } else if (src.is<i64>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(src.as<i64>()));
                } else if (src.is<i32>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(static_cast<i64>(src.as<i32>())));
                } else if (src.is<u32>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(static_cast<i64>(src.as<u32>())));
                } else if (src.is<double>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(static_cast<float>(src.as<double>())));
                } else if (src.is<float>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(src.as<float>()));
                } else if (src.is<const char*>()) {
                    result_stack.push_back(fl::make_shared<JsonValue>(fl::string(src.as<const char*>())));
                } else if (src.is<FLArduinoJson::JsonArrayConst>()) {
                    FLArduinoJson::JsonArrayConst arr = src.as<FLArduinoJson::JsonArrayConst>();

                    if (arr.size() == 0) {
                        result_stack.push_back(fl::make_shared<JsonValue>(JsonArray{}));
                        break;
                    }

                    // Classify array type (no recursion needed)
                    ArrayTypeInfo typeInfo;
                    for (const auto& elem : arr) {
                        if (!elem.is<i32>() && !elem.is<i64>() && !elem.is<double>()) {
                            typeInfo.disableAll();
                            break;
                        }
                        if (elem.is<double>()) {
                            typeInfo.checkNumericValue(elem.as<double>());
                        } else {
                            i64 val = elem.is<i32>() ? elem.as<i32>() : elem.as<i64>();
                            typeInfo.checkIntegerValue(val);
                        }
                    }

                    ArrayType arrayType = typeInfo.getBestType();

                    if (arrayType == ALL_UINT8) {
                        fl::vector<u8> byteData;
                        for (const auto& elem : arr) {
                            if (elem.is<double>()) {
                                byteData.push_back(static_cast<u8>(elem.as<double>()));
                            } else {
                                i64 val = elem.is<i32>() ? elem.as<i32>() : elem.as<i64>();
                                byteData.push_back(static_cast<u8>(val));
                            }
                        }
                        result_stack.push_back(fl::make_shared<JsonValue>(fl::move(byteData)));
                    } else if (arrayType == ALL_INT16) {
                        fl::vector<i16> intData;
                        for (const auto& elem : arr) {
                            if (elem.is<double>()) {
                                intData.push_back(static_cast<i16>(elem.as<double>()));
                            } else {
                                i64 val = elem.is<i32>() ? elem.as<i32>() : elem.as<i64>();
                                intData.push_back(static_cast<i16>(val));
                            }
                        }
                        result_stack.push_back(fl::make_shared<JsonValue>(fl::move(intData)));
                    } else if (arrayType == ALL_FLOATS) {
                        fl::vector<float> floatData;
                        for (const auto& elem : arr) {
                            if (elem.is<double>()) {
                                floatData.push_back(static_cast<float>(elem.as<double>()));
                            } else {
                                i64 val = elem.is<i32>() ? elem.as<i32>() : elem.as<i64>();
                                floatData.push_back(static_cast<float>(val));
                            }
                        }
                        result_stack.push_back(fl::make_shared<JsonValue>(fl::move(floatData)));
                    } else {
                        // GENERIC_ARRAY - push assemble marker + child converts
                        size_t count = 0;
                        for (const auto& elem : arr) { (void)elem; count++; }

                        ParseWork assemble;
                        assemble.action = ParseAction::ASSEMBLE_ARRAY;
                        assemble.count = count;
                        work_stack.push_back(fl::move(assemble));

                        // Push children in FORWARD order; LIFO processing
                        // means they execute in reverse, producing results
                        // that pop in forward order during assembly.
                        for (const auto& elem : arr) {
                            ParseWork child;
                            child.action = ParseAction::CONVERT;
                            child.src = elem;
                            child.count = 0;
                            work_stack.push_back(fl::move(child));
                        }
                    }
                } else if (src.is<FLArduinoJson::JsonObjectConst>()) {
                    FLArduinoJson::JsonObjectConst obj = src.as<FLArduinoJson::JsonObjectConst>();

                    ParseWork assemble;
                    assemble.action = ParseAction::ASSEMBLE_OBJECT;
                    assemble.count = 0;

                    // Collect keys and push child conversions in forward order
                    for (const auto& kv : obj) {
                        assemble.keys.push_back(fl::string(kv.key().c_str()));
                        assemble.count++;
                    }
                    work_stack.push_back(fl::move(assemble));

                    for (const auto& kv : obj) {
                        ParseWork child;
                        child.action = ParseAction::CONVERT;
                        child.src = kv.value();
                        child.count = 0;
                        work_stack.push_back(fl::move(child));
                    }
                } else {
                    result_stack.push_back(fl::make_shared<JsonValue>(nullptr));
                }
                break;
            }

            case ParseAction::ASSEMBLE_ARRAY: {
                JsonArray arr;
                // Pop item.count results. Due to LIFO ordering, they
                // come off in forward order (first element popped first).
                for (size_t i = 0; i < item.count; i++) {
                    arr.push_back(fl::move(result_stack.back()));
                    result_stack.pop_back();
                }
                result_stack.push_back(fl::make_shared<JsonValue>(fl::move(arr)));
                break;
            }

            case ParseAction::ASSEMBLE_OBJECT: {
                JsonObject obj;
                // Pop item.count results in forward key order.
                for (size_t i = 0; i < item.count; i++) {
                    obj[item.keys[i]] = fl::move(result_stack.back());
                    result_stack.pop_back();
                }
                result_stack.push_back(fl::make_shared<JsonValue>(fl::move(obj)));
                break;
            }
        }
    }

    return result_stack.empty() ? fl::make_shared<JsonValue>(nullptr) : result_stack.back();
    #endif
}

fl::string JsonValue::to_string() const {
    // Parse the JSON value to a string, then parse it back to a Json object,
    // and use the working to_string_native method
    // This is a workaround to avoid reimplementing the serialization logic
    
    // First, create a temporary Json document with this value
    Json temp;
    // Use the public method to set the value
    temp.set_value(fl::make_shared<JsonValue>(*this));
    // Use the working implementation
    return temp.to_string_native();
}

// Recursive serializer visitor - declared and defined at file scope
// Receives direct references to variant data (no copies) and handles serialization recursively
struct SerializerVisitor {
    fl::deque<char>& out;

    void append(const char* str) {
        while (*str) { out.push_back(*str++); }
    }

    void append_str(const fl::string& str) {
        for (size_t i = 0; i < str.size(); ++i) {
            out.push_back(str[i]);
        }
    }

    void append_escaped(const fl::string& str) {
        out.push_back('"');
        for (size_t i = 0; i < str.size(); ++i) {
            char c = str[i];
            switch (c) {
                case '"':  append("\\\""); break;
                case '\\': append("\\\\"); break;
                case '\n': append("\\n"); break;
                case '\r': append("\\r"); break;
                case '\t': append("\\t"); break;
                case '\b': append("\\b"); break;
                case '\f': append("\\f"); break;
                default: out.push_back(c); break;
            }
        }
        out.push_back('"');
    }

    // Serialize a JsonValue recursively
    void serialize_value(const JsonValue* value) {
        if (!value) {
            append("null");
            return;
        }
        value->data.visit(*this);
    }

    // accept() overloads - called by variant::visit() with direct references
    void accept(const fl::nullptr_t&) { append("null"); }

    void accept(const bool& b) { append(b ? "true" : "false"); }

    void accept(const i64& i) {
        fl::string num_str;
        num_str.append(i);
        append_str(num_str);
    }

    void accept(const float& f) {
        fl::string num_str;
        num_str.append(f, 3);
        append_str(num_str);
    }

    void accept(const fl::string& s) { append_escaped(s); }

    void accept(const JsonArray& arr) {
        if (arr.empty()) {
            append("[]");
            return;
        }
        out.push_back('[');
        bool first = true;
        for (const auto& item : arr) {
            if (!first) out.push_back(',');
            first = false;
            serialize_value(item ? item.get() : &get_null_value());
        }
        out.push_back(']');
    }

    void accept(const JsonObject& obj) {
        if (obj.empty()) {
            append("{}");
            return;
        }
        out.push_back('{');
        bool first = true;
        for (const auto& kv : obj) {
            if (!first) out.push_back(',');
            first = false;
            append_escaped(kv.first);
            out.push_back(':');
            serialize_value(kv.second ? kv.second.get() : &get_null_value());
        }
        out.push_back('}');
    }

    void accept(const fl::vector<i16>& audio) {
        out.push_back('[');
        bool first = true;
        for (const auto& item : audio) {
            if (!first) out.push_back(',');
            first = false;
            fl::string num_str;
            num_str.append(static_cast<int>(item));
            append_str(num_str);
        }
        out.push_back(']');
    }

    void accept(const fl::vector<u8>& bytes) {
        out.push_back('[');
        bool first = true;
        for (const auto& item : bytes) {
            if (!first) out.push_back(',');
            first = false;
            fl::string num_str;
            num_str.append(static_cast<int>(item));
            append_str(num_str);
        }
        out.push_back(']');
    }

    void accept(const fl::vector<float>& floats) {
        out.push_back('[');
        bool first = true;
        for (const auto& item : floats) {
            if (!first) out.push_back(',');
            first = false;
            fl::string num_str;
            num_str.append(static_cast<float>(item), 6);
            append_str(num_str);
        }
        out.push_back(']');
    }
};

fl::string Json::to_string_native() const {
    if (!m_value) {
        return "null";
    }

    // Use fl::deque for memory-efficient JSON serialization
    fl::deque<char> json_chars;

    // Use the visitor to serialize the value recursively
    SerializerVisitor visitor{json_chars};
    visitor.serialize_value(m_value.get());

    // Convert deque to fl::string efficiently
    fl::string result;
    if (!json_chars.empty()) {
        result.assign(&json_chars[0], json_chars.size());
    }

    return result;
}

// Forward declaration for the serializeValue function
fl::string serializeValue(const JsonValue& value);


fl::string Json::normalizeJsonString(const char* jsonStr) {
    fl::string result;
    if (!jsonStr) {
        return result;
    }
    size_t len = strlen(jsonStr);
    for (size_t i = 0; i < len; ++i) {
        char c = jsonStr[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

} // namespace fl

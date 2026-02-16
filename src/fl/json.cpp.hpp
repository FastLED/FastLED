
#include "fl/json.h"
#include "fl/json/detail/types.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/span.h"
#include "fl/stl/charconv.h"
#include "fl/stl/math.h" // For floor function
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/stl/optional.h"
#include "fl/stl/unordered_map.h"
#include "fl/log.h"
#include "fl/math_macros.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/limits.h"
#include "fl/stl/move.h"
#include "fl/stl/strstream.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string_interner.h"
#include "fl/string_view.h"

// fl::numeric_limits<i16>::min(), fl::numeric_limits<i16>::max(), and fl::numeric_limits<u8>::max() should come from the platform's
// <stdint.h> or <cstdint> headers (via fl/stl/stdint.h).
// FastLED no longer defines these macros to avoid conflicts with system headers.


#if FASTLED_ARDUINO_JSON_PARSING_ENABLED

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_NULL_DEREFERENCE
#include "third_party/arduinojson/json.h"  // IWYU pragma: keep
FL_DISABLE_WARNING_POP

#endif  // FASTLED_ARDUINO_JSON_PARSING_ENABLED

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
    #if !FASTLED_ARDUINO_JSON_PARSING_ENABLED
    // ArduinoJson disabled - should not be called directly
    // This function is only for ArduinoJson parsing
    FL_ERROR("JsonValue::parse() called but FASTLED_ARDUINO_JSON_PARSING_ENABLED is disabled. "
             "Use Json::parse() for native parsing instead.");
    return fl::make_shared<JsonValue>(nullptr);
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

// ============================================================================
// CUSTOM JSON PARSER - VISITOR PATTERN (Milestones 2-4)
// ============================================================================

namespace {  // Anonymous namespace for internal implementation

// Recursion limit protection
constexpr int MAX_JSON_DEPTH = 32;

// Token types
enum class JsonToken : u8 {
    LBRACE, RBRACE, LBRACKET, RBRACKET, COLON, COMMA,
    STRING, NUMBER, TRUE, FALSE, NULL_VALUE, ERROR, END_OF_INPUT,

    // Array lookahead optimization tokens
    ARRAY_UINT8,   // [0-255] → vector<u8>
    ARRAY_INT8,    // [-128 to 127] (unused, covered by INT16)
    ARRAY_INT16,   // [-32768 to 32767] → vector<i16>
    ARRAY_INT32,   // (unused, falls back to slow path)
    ARRAY_INT64,   // (unused, falls back to slow path)
    ARRAY_FLOAT,   // Floats or mixed int/float → vector<float>
    ARRAY_DOUBLE,  // (unused, uses ARRAY_FLOAT)
    ARRAY_STRING,  // (unused, falls back to slow path)
    ARRAY_BOOL,    // (unused, falls back to slow path)
    ARRAY_MIXED    // (unused, falls back to slow path)
};

// Visitor return value
enum class ParseState : u8 { KEEP_GOING = 0, ERROR = 1 };

// Base visitor interface
class JsonVisitor {
public:
    virtual ParseState on_token(JsonToken token, const fl::span<const char>& value) = 0;
    virtual ~JsonVisitor() = default;
};

// Character-by-character tokenizer
class JsonTokenizer {
private:
    const char* mInput;
    size_t mLen;
    size_t mPos;
    bool mEnableLookahead;

    void skip_whitespace() {
        while (mPos < mLen && (mInput[mPos] == ' ' || mInput[mPos] == '\t' ||
                               mInput[mPos] == '\n' || mInput[mPos] == '\r')) {
            mPos++;
        }
    }

    // Array lookahead scanner - scans array to determine if it can be optimized
    // Returns specialized token (ARRAY_UINT8, etc.) or LBRACKET for slow path
    // Assumes mPos is positioned after '[' character
    JsonToken scan_array_lookahead(fl::span<const char>& out_span) {
        if (!mEnableLookahead) {
            return JsonToken::LBRACKET;  // Lookahead disabled
        }

        size_t start_pos = mPos;  // Position after '['

        // Track element types and ranges
        bool has_int = false;
        bool has_float = false;
        bool has_float_beyond_precision = false;  // Track floats > 2^24
        bool has_string = false;
        bool has_bool = false;
        bool has_null = false;
        i64 int_min = fl::numeric_limits<i64>::max();
        i64 int_max = fl::numeric_limits<i64>::min();

        skip_whitespace();

        // Empty array - use slow path
        if (mPos < mLen && mInput[mPos] == ']') {
            out_span = fl::span<const char>();
            return JsonToken::LBRACKET;
        }

        // Scan elements
        while (mPos < mLen) {
            skip_whitespace();

            // Check for array end
            if (mInput[mPos] == ']') {
                break;
            }

            char c = mInput[mPos];

            // Abort on nested structures
            if (c == '[' || c == '{') {
                return JsonToken::LBRACKET;
            }

            // STRING
            if (c == '"') {
                has_string = true;
                mPos++;
                while (mPos < mLen) {
                    if (mInput[mPos] == '\\') {
                        mPos += 2;  // Skip escape sequence
                    } else if (mInput[mPos] == '"') {
                        mPos++;
                        break;
                    } else {
                        mPos++;
                    }
                }
            }
            // NUMBER
            else if (c == '-' || (c >= '0' && c <= '9')) {
                size_t num_start = mPos;
                bool is_float = false;

                if (c == '-') mPos++;
                while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') mPos++;

                if (mPos < mLen && mInput[mPos] == '.') {
                    is_float = true;
                    mPos++;
                    while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') mPos++;
                }

                if (mPos < mLen && (mInput[mPos] == 'e' || mInput[mPos] == 'E')) {
                    is_float = true;
                    mPos++;
                    if (mPos < mLen && (mInput[mPos] == '+' || mInput[mPos] == '-')) mPos++;
                    while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') mPos++;
                }

                if (is_float) {
                    has_float = true;
                    // Parse float value to check if it's beyond integer precision
                    // Note: fl::parseFloat may have precision loss for very large numbers
                    float f_val = fl::parseFloat(&mInput[num_start], mPos - num_start);
                    if (fl::fl_abs(f_val) > 16777216.0f) {
                        has_float_beyond_precision = true;
                    }
                } else {
                    has_int = true;
                    // Parse actual value to get accurate range
                    // fl::parseInt doesn't allocate (Phase 1 test confirms zero allocations)
                    i64 val = fl::parseInt(&mInput[num_start], mPos - num_start);
                    if (val < int_min) int_min = val;
                    if (val > int_max) int_max = val;
                }
            }
            // TRUE
            else if (c == 't' && mPos + 3 < mLen &&
                     mInput[mPos+1] == 'r' && mInput[mPos+2] == 'u' && mInput[mPos+3] == 'e') {
                has_bool = true;
                mPos += 4;
            }
            // FALSE
            else if (c == 'f' && mPos + 4 < mLen &&
                     mInput[mPos+1] == 'a' && mInput[mPos+2] == 'l' &&
                     mInput[mPos+3] == 's' && mInput[mPos+4] == 'e') {
                has_bool = true;
                mPos += 5;
            }
            // NULL
            else if (c == 'n' && mPos + 3 < mLen &&
                     mInput[mPos+1] == 'u' && mInput[mPos+2] == 'l' && mInput[mPos+3] == 'l') {
                has_null = true;
                mPos += 4;
            }
            else {
                return JsonToken::LBRACKET;  // Unknown token
            }

            skip_whitespace();

            // Check for comma or end
            if (mPos < mLen) {
                if (mInput[mPos] == ',') {
                    mPos++;
                } else if (mInput[mPos] == ']') {
                    break;
                } else {
                    return JsonToken::LBRACKET;  // Invalid syntax
                }
            }
        }

        // Determine array type
        int type_count = (has_int ? 1 : 0) + (has_float ? 1 : 0) +
                         (has_string ? 1 : 0) + (has_bool ? 1 : 0) + (has_null ? 1 : 0);

        out_span = fl::span<const char>(&mInput[start_pos], mPos - start_pos);

        // Only emit specialized tokens for types JsonValue supports
        if (type_count == 1 || (type_count == 2 && has_int && has_float)) {
            if (has_float || (has_int && has_float)) {
                // Don't optimize floats beyond integer precision
                if (has_float_beyond_precision) {
                    return JsonToken::LBRACKET;  // Use slow path
                }
                return JsonToken::ARRAY_FLOAT;  // vector<float>
            }
            if (has_int) {
                // Determine optimal integer type
                if (int_min >= 0 && int_max <= 255) {
                    return JsonToken::ARRAY_UINT8;  // vector<u8>
                }
                if (int_min >= -32768 && int_max <= 32767) {
                    return JsonToken::ARRAY_INT16;  // vector<i16>
                }
                // Larger ints fall back to slow path
                return JsonToken::LBRACKET;
            }
            // Strings, bools - use slow path
            return JsonToken::LBRACKET;
        }

        // Mixed types - use slow path
        return JsonToken::LBRACKET;
    }

    JsonToken next_token(fl::span<const char>& out_value) {
        skip_whitespace();
        if (mPos >= mLen) {
            out_value = fl::span<const char>();
            return JsonToken::END_OF_INPUT;
        }

        char c = mInput[mPos];

        // Structural tokens
        switch (c) {
            case '{':
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::LBRACE;
            case '}':
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::RBRACE;
            case '[': {
                size_t saved_pos = mPos;
                mPos++;  // Skip '['
                JsonToken array_token = scan_array_lookahead(out_value);
                if (array_token != JsonToken::LBRACKET) {
                    // Specialized array token - mPos now points at ']'
                    mPos++;  // Skip ']'
                    return array_token;
                }
                // Slow path - restore position
                mPos = saved_pos;
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::LBRACKET;
            }
            case ']':
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::RBRACKET;
            case ':':
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::COLON;
            case ',':
                out_value = fl::span<const char>(&mInput[mPos], 1);
                mPos++;
                return JsonToken::COMMA;
            default:
                break;  // Fall through to other token types
        }

        // STRING tokenization
        if (c == '"') {
            size_t start = mPos + 1;  // After opening quote
            mPos++;

            while (mPos < mLen) {
                if (mInput[mPos] == '\\') {
                    // Skip escape sequence
                    mPos++;
                    if (mPos >= mLen) {
                        out_value = fl::span<const char>();
                        return JsonToken::ERROR;  // Unclosed string
                    }
                    mPos++;  // Skip escaped character
                } else if (mInput[mPos] == '"') {
                    // Found closing quote
                    out_value = fl::span<const char>(&mInput[start], mPos - start);
                    mPos++;  // Move past closing quote
                    return JsonToken::STRING;
                } else {
                    mPos++;
                }
            }

            // Unclosed string
            out_value = fl::span<const char>();
            return JsonToken::ERROR;
        }

        // NUMBER tokenization
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = mPos;

            // Optional minus
            if (c == '-') mPos++;

            // Require at least one digit
            if (mPos >= mLen || !(mInput[mPos] >= '0' && mInput[mPos] <= '9')) {
                out_value = fl::span<const char>();
                return JsonToken::ERROR;
            }

            // Digits before decimal
            while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') {
                mPos++;
            }

            // Optional decimal point
            if (mPos < mLen && mInput[mPos] == '.') {
                mPos++;
                // Require digits after decimal
                if (mPos >= mLen || !(mInput[mPos] >= '0' && mInput[mPos] <= '9')) {
                    out_value = fl::span<const char>();
                    return JsonToken::ERROR;
                }
                while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') {
                    mPos++;
                }
            }

            // Optional exponent
            if (mPos < mLen && (mInput[mPos] == 'e' || mInput[mPos] == 'E')) {
                mPos++;
                // Optional sign
                if (mPos < mLen && (mInput[mPos] == '+' || mInput[mPos] == '-')) {
                    mPos++;
                }
                // Require exponent digits
                if (mPos >= mLen || !(mInput[mPos] >= '0' && mInput[mPos] <= '9')) {
                    out_value = fl::span<const char>();
                    return JsonToken::ERROR;
                }
                while (mPos < mLen && mInput[mPos] >= '0' && mInput[mPos] <= '9') {
                    mPos++;
                }
            }

            out_value = fl::span<const char>(&mInput[start], mPos - start);
            return JsonToken::NUMBER;
        }

        // Keyword: true
        if (c == 't' && mPos + 3 < mLen &&
            mInput[mPos+1] == 'r' && mInput[mPos+2] == 'u' && mInput[mPos+3] == 'e') {
            out_value = fl::span<const char>(&mInput[mPos], 4);
            mPos += 4;
            return JsonToken::TRUE;
        }

        // Keyword: false
        if (c == 'f' && mPos + 4 < mLen &&
            mInput[mPos+1] == 'a' && mInput[mPos+2] == 'l' &&
            mInput[mPos+3] == 's' && mInput[mPos+4] == 'e') {
            out_value = fl::span<const char>(&mInput[mPos], 5);
            mPos += 5;
            return JsonToken::FALSE;
        }

        // Keyword: null
        if (c == 'n' && mPos + 3 < mLen &&
            mInput[mPos+1] == 'u' && mInput[mPos+2] == 'l' && mInput[mPos+3] == 'l') {
            out_value = fl::span<const char>(&mInput[mPos], 4);
            mPos += 4;
            return JsonToken::NULL_VALUE;
        }

        // Unknown token
        out_value = fl::span<const char>();
        return JsonToken::ERROR;
    }

public:
    JsonTokenizer(bool enable_lookahead = true) : mInput(nullptr), mLen(0), mPos(0), mEnableLookahead(enable_lookahead) {}

    bool parse(const fl::string& input, JsonVisitor& visitor) {
        return parse(fl::string_view(input.c_str(), input.length()), visitor);
    }

    bool parse(fl::string_view input, JsonVisitor& visitor) {
        mInput = input.data();
        mLen = input.size();
        mPos = 0;

        while (true) {
            fl::span<const char> token_value;
            JsonToken token = next_token(token_value);

            if (token == JsonToken::ERROR) {
                return false;
            }

            ParseState result = visitor.on_token(token, token_value);
            if (result == ParseState::ERROR) {
                return false;
            }

            if (token == JsonToken::END_OF_INPUT) {
                break;
            }
        }

        return true;
    }
};

// Validator - validates JSON structure with bracket matching (Milestone 8)
class JsonValidator : public JsonVisitor {
private:
    fl::vector_inlined<char, 32> mBracketStack;  // Tracks '[' or '{'
    bool mExpectKey;       // In object, expecting key next
    bool mExpectValue;     // Expecting value next
    bool mExpectColon;     // Expecting colon after key
    int mDepth;

public:
    JsonValidator() : mExpectKey(false), mExpectValue(false), mExpectColon(false), mDepth(0) {}

    ParseState on_token(JsonToken token, const fl::span<const char>& value) override {
        (void)value;  // Suppress unused parameter warning

        // Recursion depth check
        if (mDepth > MAX_JSON_DEPTH) {
            FL_ERROR("JSON parser: FATAL - recursion depth exceeded " << MAX_JSON_DEPTH);
            return ParseState::ERROR;
        }

        switch (token) {
            case JsonToken::LBRACE:
                mBracketStack.push_back('{');
                mDepth++;
                mExpectKey = true;
                mExpectValue = false;
                return ParseState::KEEP_GOING;

            case JsonToken::RBRACE:
                if (mBracketStack.empty() || mBracketStack.back() != '{') {
                    return ParseState::ERROR;  // Unmatched closing brace
                }
                mBracketStack.pop_back();
                mDepth--;
                mExpectKey = false;
                return ParseState::KEEP_GOING;

            case JsonToken::LBRACKET:
                mBracketStack.push_back('[');
                mDepth++;
                mExpectValue = true;
                return ParseState::KEEP_GOING;

            case JsonToken::RBRACKET:
                if (mBracketStack.empty() || mBracketStack.back() != '[') {
                    return ParseState::ERROR;  // Unmatched closing bracket
                }
                mBracketStack.pop_back();
                mDepth--;
                mExpectValue = false;
                return ParseState::KEEP_GOING;

            case JsonToken::COLON:
                if (!mExpectColon) {
                    return ParseState::ERROR;  // Unexpected colon
                }
                mExpectColon = false;
                mExpectValue = true;
                return ParseState::KEEP_GOING;

            case JsonToken::STRING:
                if (!mBracketStack.empty() && mBracketStack.back() == '{' && mExpectKey) {
                    // This is an object key
                    mExpectKey = false;
                    mExpectColon = true;
                } else if (mExpectValue || mBracketStack.empty()) {
                    // This is a value
                    mExpectValue = false;
                } else {
                    return ParseState::ERROR;
                }
                return ParseState::KEEP_GOING;

            case JsonToken::NUMBER:
            case JsonToken::TRUE:
            case JsonToken::FALSE:
            case JsonToken::NULL_VALUE:
            // Specialized array tokens (complete arrays, treat as values)
            case JsonToken::ARRAY_UINT8:
            case JsonToken::ARRAY_INT16:
            case JsonToken::ARRAY_FLOAT:
                if (mExpectValue || mBracketStack.empty()) {
                    mExpectValue = false;
                    return ParseState::KEEP_GOING;
                }
                return ParseState::ERROR;

            case JsonToken::COMMA:
                // Set expectations for next element
                if (!mBracketStack.empty()) {
                    if (mBracketStack.back() == '{') {
                        mExpectKey = true;
                    } else {
                        mExpectValue = true;
                    }
                }
                return ParseState::KEEP_GOING;

            case JsonToken::END_OF_INPUT:
                return ParseState::KEEP_GOING;

            default:
                return ParseState::ERROR;
        }
    }

    bool is_valid() const {
        return mBracketStack.empty() && !mExpectColon && !mExpectKey;
    }
};

// Helper: Check if string contains escape sequences
bool has_escape_sequences(const fl::span<const char>& span) {
    for (size_t i = 0; i < span.size(); i++) {
        if (span[i] == '\\') {
            return true;
        }
    }
    return false;
}

// Helper: Unescape JSON string (only call if has_escape_sequences() returns true)
fl::string unescape_string(const fl::span<const char>& span) {
    fl::string result;
    result.reserve(span.size());

    for (size_t i = 0; i < span.size(); i++) {
        if (span[i] == '\\' && i + 1 < span.size()) {
            char next = span[i + 1];
            switch (next) {
                case '"':  result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '/':  result += '/'; i++; break;
                case 'b':  result += '\b'; i++; break;
                case 'f':  result += '\f'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                default:   result += span[i]; break;  // Pass through unknown escapes
            }
        } else {
            result += span[i];
        }
    }

    return result;
}

// Array optimization helpers (Milestone 9)
enum ArrayType { ALL_UINT8, ALL_INT16, ALL_FLOATS, GENERIC_ARRAY };

ArrayType classify_array(const JsonArray& arr) {
    if (arr.empty()) return GENERIC_ARRAY;

    bool all_numeric = true;
    i64 min_val = fl::numeric_limits<i64>::max();
    i64 max_val = fl::numeric_limits<i64>::min();
    bool has_float = false;
    bool has_float_beyond_precision = false;

    for (const auto& elem : arr) {
        if (!elem) {
            all_numeric = false;
            break;
        }

        if (elem->is_int()) {
            auto val = elem->as_int();
            if (val) {
                i64 v = *val;
                min_val = (v < min_val) ? v : min_val;
                max_val = (v > max_val) ? v : max_val;
            } else {
                all_numeric = false;
                break;
            }
        } else if (elem->is_float()) {
            has_float = true;
            auto val = elem->as_float();
            if (!val) {
                all_numeric = false;
                break;
            }
            // Check if float is beyond integer precision (>2^24 or <-2^24)
            float f = *val;
            if (fl::fl_abs(f) > 16777216.0f) {
                has_float_beyond_precision = true;
            }
        } else {
            all_numeric = false;
            break;
        }
    }

    if (!all_numeric) return GENERIC_ARRAY;

    // Classification
    if (has_float) {
        // If floats are beyond integer precision, don't optimize
        if (has_float_beyond_precision) {
            return GENERIC_ARRAY;
        }
        return ALL_FLOATS;
    }

    // Integer arrays
    if (min_val >= 0 && max_val <= 255) {
        return ALL_UINT8;
    }

    if (min_val >= -32768 && max_val <= 32767) {
        return ALL_INT16;
    }

    // Large integers (>32767 or <-32768) that don't fit in int16 but can be represented as float
    // Convert to float if within float's integer precision range (±2^24)
    if (min_val >= -16777216 && max_val <= 16777216) {
        return ALL_FLOATS;
    }

    return GENERIC_ARRAY;
}

fl::shared_ptr<JsonValue> optimize_array(fl::shared_ptr<JsonValue> array_val) {
    auto arr = array_val->data.ptr<JsonArray>();
    if (!arr) return array_val;

    ArrayType type = classify_array(*arr);

    switch (type) {
        case ALL_UINT8: {
            fl::vector<u8> vec;
            vec.reserve(arr->size());
            for (const auto& elem : *arr) {
                auto val = elem->as_int();
                if (val) vec.push_back(static_cast<u8>(*val));
            }
            return fl::make_shared<JsonValue>(fl::move(vec));
        }

        case ALL_INT16: {
            fl::vector<i16> vec;
            vec.reserve(arr->size());
            for (const auto& elem : *arr) {
                auto val = elem->as_int();
                if (val) vec.push_back(static_cast<i16>(*val));
            }
            return fl::make_shared<JsonValue>(fl::move(vec));
        }

        case ALL_FLOATS: {
            fl::vector<float> vec;
            vec.reserve(arr->size());
            for (const auto& elem : *arr) {
                if (elem->is_float()) {
                    auto val = elem->as_float();
                    if (val) vec.push_back(*val);
                } else if (elem->is_int()) {
                    auto val = elem->as_int();
                    if (val) vec.push_back(static_cast<float>(*val));
                }
            }
            return fl::make_shared<JsonValue>(fl::move(vec));
        }

        default:
            return array_val;  // Keep as generic array
    }
}

// Builder - handles nested objects/arrays with stack management (Milestone 6)
class JsonBuilder : public JsonVisitor {
private:
    struct StackFrame {
        enum Type { OBJECT, ARRAY } type;
        fl::shared_ptr<JsonValue> value;
        fl::string pending_key;  // For objects: key waiting for value
    };

    fl::vector_inlined<StackFrame, 8> mStack;  // Inline first 8 frames (most JSON is shallow)
    fl::shared_ptr<JsonValue> mRoot;
    int mDepth;
    // String interning enabled for large strings (> 64 bytes) that overflow SSO
    fl::StringInterner mInterner;

    // Parse integer array directly from span into vector (zero allocations)
    template<typename T>
    bool parse_int_array(const fl::span<const char>& span, fl::vector<T>& out_vec) {
        const char* p = span.data();
        const char* end = p + span.size();

        while (p < end) {
            // Skip whitespace
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
            if (p >= end) break;

            // Parse number
            const char* num_start = p;
            if (*p == '-') p++;
            while (p < end && *p >= '0' && *p <= '9') p++;

            i64 val = fl::parseInt(num_start, p - num_start);
            out_vec.push_back(static_cast<T>(val));

            // Skip whitespace
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

            // Skip comma
            if (p < end && *p == ',') p++;
        }

        return true;
    }

    // Parse float array directly from span into vector (zero allocations)
    template<typename T>
    bool parse_float_array(const fl::span<const char>& span, fl::vector<T>& out_vec) {
        const char* p = span.data();
        const char* end = p + span.size();

        while (p < end) {
            // Skip whitespace
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
            if (p >= end) break;

            // Parse number
            const char* num_start = p;
            bool is_float = false;

            if (*p == '-') p++;
            while (p < end && *p >= '0' && *p <= '9') p++;

            if (p < end && *p == '.') {
                is_float = true;
                p++;
                while (p < end && *p >= '0' && *p <= '9') p++;
            }

            if (p < end && (*p == 'e' || *p == 'E')) {
                is_float = true;
                p++;
                if (p < end && (*p == '+' || *p == '-')) p++;
                while (p < end && *p >= '0' && *p <= '9') p++;
            }

            T val;
            if (is_float) {
                val = static_cast<T>(fl::parseFloat(num_start, p - num_start));
            } else {
                val = static_cast<T>(fl::parseInt(num_start, p - num_start));
            }
            out_vec.push_back(val);

            // Skip whitespace
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

            // Skip comma
            if (p < end && *p == ',') p++;
        }

        return true;
    }

    void push_value(const fl::shared_ptr<JsonValue>& val) {
        if (mStack.empty()) {
            mRoot = val;
        } else {
            StackFrame& top = mStack.back();

            if (top.type == StackFrame::OBJECT) {
                // Attach to object with pending key
                auto obj = top.value->data.ptr<JsonObject>();
                if (obj && !top.pending_key.empty()) {
                    (*obj)[top.pending_key] = val;
                    top.pending_key.clear();
                }
            } else {
                // Attach to array
                auto arr = top.value->data.ptr<JsonArray>();
                if (arr) {
                    arr->push_back(val);
                }
            }
        }
    }

public:
    JsonBuilder() : mRoot(), mDepth(0) {}

    ParseState on_token(JsonToken token, const fl::span<const char>& value) override {
        // Recursion depth check
        if (mDepth > MAX_JSON_DEPTH) {
            FL_ERROR("JSON parser: FATAL - recursion depth exceeded " << MAX_JSON_DEPTH);
            return ParseState::ERROR;
        }

        switch (token) {
            // Specialized array tokens - parse directly into typed vectors
            case JsonToken::ARRAY_UINT8: {
                fl::vector<u8> vec;
                if (!parse_int_array(value, vec)) return ParseState::ERROR;
                auto arr_val = fl::make_shared<JsonValue>(fl::move(vec));
                push_value(arr_val);
                return ParseState::KEEP_GOING;
            }

            case JsonToken::ARRAY_INT16: {
                fl::vector<i16> vec;
                if (!parse_int_array(value, vec)) return ParseState::ERROR;
                auto arr_val = fl::make_shared<JsonValue>(fl::move(vec));
                push_value(arr_val);
                return ParseState::KEEP_GOING;
            }

            case JsonToken::ARRAY_FLOAT: {
                fl::vector<float> vec;
                if (!parse_float_array(value, vec)) return ParseState::ERROR;
                auto arr_val = fl::make_shared<JsonValue>(fl::move(vec));
                push_value(arr_val);
                return ParseState::KEEP_GOING;
            }

            case JsonToken::LBRACE: {
                auto obj_val = fl::make_shared<JsonValue>(JsonObject{});
                // Don't push to parent yet - will push in RBRACE
                mStack.push_back({StackFrame::OBJECT, obj_val, ""});
                mDepth++;
                return ParseState::KEEP_GOING;
            }

            case JsonToken::RBRACE: {
                if (!mStack.empty() && mStack.back().type == StackFrame::OBJECT) {
                    auto obj_val = mStack.back().value;
                    mStack.pop_back();
                    mDepth--;
                    // Push to parent after object is complete
                    push_value(obj_val);
                }
                return ParseState::KEEP_GOING;
            }

            case JsonToken::LBRACKET: {
                auto arr_val = fl::make_shared<JsonValue>(JsonArray{});
                // Don't push to parent yet - will push in RBRACKET after optimization
                mStack.push_back({StackFrame::ARRAY, arr_val, ""});
                mDepth++;
                return ParseState::KEEP_GOING;
            }

            case JsonToken::RBRACKET: {
                if (!mStack.empty() && mStack.back().type == StackFrame::ARRAY) {
                    // Optimize array before pushing to parent (Milestone 9)
                    auto arr_val = optimize_array(mStack.back().value);
                    mStack.pop_back();
                    mDepth--;
                    // Push optimized array to parent
                    push_value(arr_val);
                }
                return ParseState::KEEP_GOING;
            }

            case JsonToken::STRING: {
                // Check if this is an object key (next token should be COLON)
                if (!mStack.empty() && mStack.back().type == StackFrame::OBJECT &&
                    mStack.back().pending_key.empty()) {
                    // Key: handle escape sequences, then intern (StringInterner handles SSO internally)
                    if (has_escape_sequences(value)) {
                        mStack.back().pending_key = mInterner.intern(unescape_string(value));
                    } else {
                        mStack.back().pending_key = mInterner.intern(value);
                    }
                } else {
                    // Value: handle escape sequences, then intern (StringInterner handles SSO internally)
                    fl::string str;
                    if (has_escape_sequences(value)) {
                        str = mInterner.intern(unescape_string(value));
                    } else {
                        str = mInterner.intern(value);
                    }
                    auto str_val = fl::make_shared<JsonValue>(str);
                    push_value(str_val);
                }

                return ParseState::KEEP_GOING;
            }

            case JsonToken::NUMBER: {
                // Determine if int or float by checking for decimal/exponent
                bool is_float = false;
                for (size_t i = 0; i < value.size(); i++) {
                    if (value[i] == '.' || value[i] == 'e' || value[i] == 'E') {
                        is_float = true;
                        break;
                    }
                }

                fl::shared_ptr<JsonValue> num_val;
                if (is_float) {
                    float f = fl::parseFloat(value.data(), value.size());
                    num_val = fl::make_shared<JsonValue>(f);
                } else {
                    int i = fl::parseInt(value.data(), value.size());
                    i64 i64_val = static_cast<i64>(i);
                    num_val = fl::make_shared<JsonValue>(i64_val);
                }

                push_value(num_val);
                return ParseState::KEEP_GOING;
            }

            case JsonToken::TRUE:
                push_value(fl::make_shared<JsonValue>(true));
                return ParseState::KEEP_GOING;

            case JsonToken::FALSE:
                push_value(fl::make_shared<JsonValue>(false));
                return ParseState::KEEP_GOING;

            case JsonToken::NULL_VALUE:
                push_value(fl::make_shared<JsonValue>(nullptr));
                return ParseState::KEEP_GOING;

            case JsonToken::COLON:
            case JsonToken::COMMA:
                // Structural tokens - no action needed
                return ParseState::KEEP_GOING;

            case JsonToken::END_OF_INPUT:
                return ParseState::KEEP_GOING;

            default:
                return ParseState::ERROR;
        }
    }

    fl::shared_ptr<JsonValue> get_result() {
        return mRoot ? mRoot : fl::make_shared<JsonValue>(nullptr);
    }
};

}  // namespace

// PARSE2 IMPLEMENTATION - Milestone 8: Two-phase parser with validation
fl::shared_ptr<JsonValue> JsonValue::parse2(const fl::string& txt) {
    JsonTokenizer tokenizer;

    // Phase 1: Validate
    JsonValidator validator;
    if (!tokenizer.parse(txt, validator) || !validator.is_valid()) {
        return fl::make_shared<JsonValue>(nullptr);
    }

    // Phase 2: Build
    JsonBuilder builder;
    if (!tokenizer.parse(txt, builder)) {
        return fl::make_shared<JsonValue>(nullptr);
    }

    return builder.get_result();
}

// Phase 1 validation only (for testing - MUST allocate zero heap memory)
bool JsonValue::parse2_validate_only(const fl::string& txt) {
    return parse2_validate_only(fl::string_view(txt.c_str(), txt.length()));
}

bool JsonValue::parse2_validate_only(fl::string_view txt) {
    JsonTokenizer tokenizer;
    JsonValidator validator;
    return tokenizer.parse(txt, validator) && validator.is_valid();
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

#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/dbg.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JSON type detection - basic types") {
    fl::JsonDocument doc;
    
    // Test object type - create a proper object
    doc["object"]["nested"] = "value";
    auto objectVar = doc["object"];
    CHECK(fl::getJsonType(objectVar) == fl::JSON_OBJECT);
    
    // Test array type - create a proper array
    doc["array"].add(1);
    doc["array"].add(2);
    doc["array"].add(3);
    auto arrayVar = doc["array"];
    CHECK(fl::getJsonType(arrayVar) == fl::JSON_ARRAY);
    
    // Test string type
    doc["string"] = "hello";
    auto stringVar = doc["string"];
    CHECK(fl::getJsonType(stringVar) == fl::JSON_STRING);
    
    // Test integer type
    doc["integer"] = 42;
    auto intVar = doc["integer"];
    CHECK(fl::getJsonType(intVar) == fl::JSON_INTEGER);
    
    // Test float type
    doc["float"] = 3.14f;
    auto floatVar = doc["float"];
    CHECK(fl::getJsonType(floatVar) == fl::JSON_FLOAT);
    
    // Test boolean type
    doc["boolean"] = true;
    auto boolVar = doc["boolean"];
    CHECK(fl::getJsonType(boolVar) == fl::JSON_BOOLEAN);
    
    // Test null type
    doc["null"] = nullptr;
    auto nullVar = doc["null"];
    CHECK(fl::getJsonType(nullVar) == fl::JSON_NULL);
}

TEST_CASE("JSON type detection - user example") {
    // Test the exact usage pattern from the user's request
    fl::JsonDocument doc;
    doc["foo"] = "string_value";
    doc["bar"] = 123;
    doc["baz"] = true;
    doc["obj"]["key"] = "value";  // Create proper object
    doc["arr"].add("item1");     // Create proper array
    doc["null_val"] = nullptr;
    doc["float_val"] = 2.718;
    
    // Test the user's example pattern
    auto v = doc["foo"];
    fl::JsonType t = fl::getJsonType(v);
    switch(t) {
        case fl::JSON_OBJECT:  
            CHECK(false); // Should not be object
            break;
        case fl::JSON_ARRAY:   
            CHECK(false); // Should not be array
            break;
        case fl::JSON_INTEGER: 
            CHECK(false); // Should not be integer
            break;
        case fl::JSON_FLOAT:   
            CHECK(false); // Should not be float
            break;
        case fl::JSON_BOOLEAN: 
            CHECK(false); // Should not be boolean
            break;
        case fl::JSON_STRING:  
            CHECK(true); // Should be string
            FL_WARN("foo is string - correct!");
            break;
        case fl::JSON_NULL:               
            CHECK(false); // Should not be null
            break;
    }
    
    // Test all types
    CHECK(fl::getJsonType(doc["foo"]) == fl::JSON_STRING);
    CHECK(fl::getJsonType(doc["bar"]) == fl::JSON_INTEGER);
    CHECK(fl::getJsonType(doc["baz"]) == fl::JSON_BOOLEAN);
    CHECK(fl::getJsonType(doc["obj"]) == fl::JSON_OBJECT);
    CHECK(fl::getJsonType(doc["arr"]) == fl::JSON_ARRAY);
    CHECK(fl::getJsonType(doc["null_val"]) == fl::JSON_NULL);
    CHECK(fl::getJsonType(doc["float_val"]) == fl::JSON_FLOAT);
}

TEST_CASE("JSON type detection - parsed from string") {
    // Test type detection on JSON parsed from string
    const char* jsonStr = R"({
        "name": "test",
        "count": 100,
        "pi": 3.14159,
        "active": true,
        "data": null,
        "items": [1, 2, 3],
        "config": {
            "debug": false
        }
    })";
    
    fl::JsonDocument doc;
    fl::string error;
    bool success = fl::parseJson(jsonStr, &doc, &error);
    
    CHECK(success);
    CHECK(error.empty());
    
    auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
    
    // Test all types from parsed JSON
    CHECK(fl::getJsonType(obj["name"]) == fl::JSON_STRING);
    CHECK(fl::getJsonType(obj["count"]) == fl::JSON_INTEGER);
    CHECK(fl::getJsonType(obj["pi"]) == fl::JSON_FLOAT);
    CHECK(fl::getJsonType(obj["active"]) == fl::JSON_BOOLEAN);
    CHECK(fl::getJsonType(obj["data"]) == fl::JSON_NULL);
    CHECK(fl::getJsonType(obj["items"]) == fl::JSON_ARRAY);
    CHECK(fl::getJsonType(obj["config"]) == fl::JSON_OBJECT);
    
    FL_WARN("Parsed JSON type detection tests passed");
}

TEST_CASE("JSON type detection - comprehensive switch example") {
    // Test the full example as requested by the user
    fl::JsonDocument doc;
    doc["object"]["key"] = "value";  // Create proper object
    doc["array"].add(1);
    doc["array"].add(2);             // Create proper array
    doc["integer"] = 42;
    doc["float"] = 3.14;
    doc["boolean"] = true;
    doc["string"] = "hello";
    doc["null"] = nullptr;
    
    const char* keys[] = {"object", "array", "integer", "float", "boolean", "string", "null"};
    fl::JsonType expectedTypes[] = {
        fl::JSON_OBJECT, fl::JSON_ARRAY, fl::JSON_INTEGER, 
        fl::JSON_FLOAT, fl::JSON_BOOLEAN, fl::JSON_STRING, fl::JSON_NULL
    };
    
    for (int i = 0; i < 7; i++) {
        auto v = doc[keys[i]];
        fl::JsonType t = fl::getJsonType(v);
        CHECK(t == expectedTypes[i]);
        
        // Print results like in the user's example
        switch(t) {
            case fl::JSON_OBJECT:  
                FL_WARN(keys[i] << ": object");
                break;
            case fl::JSON_ARRAY:   
                FL_WARN(keys[i] << ": array");
                break;
            case fl::JSON_INTEGER: 
                FL_WARN(keys[i] << ": integer");
                break;
            case fl::JSON_FLOAT:   
                FL_WARN(keys[i] << ": float");
                break;
            case fl::JSON_BOOLEAN: 
                FL_WARN(keys[i] << ": boolean");
                break;
            case fl::JSON_STRING:  
                FL_WARN(keys[i] << ": string");
                break;
            case fl::JSON_NULL:               
                FL_WARN(keys[i] << ": null");
                break;
        }
    }
}

TEST_CASE("JSON type detection - example from user request") {
    // Exactly reproduce the user's example
    fl::JsonDocument doc;
    doc["foo"] = "test_string";
    
    // Method 1: using getJsonType function
    auto variant = doc["foo"];
    fl::JsonType t = fl::getJsonType(variant);
    switch(t) {
        case fl::JSON_OBJECT:  
            FL_WARN("object"); 
            CHECK(false);
            break;
        case fl::JSON_ARRAY:   
            FL_WARN("array");   
            CHECK(false);
            break;
        case fl::JSON_INTEGER: 
            FL_WARN("integer"); 
            CHECK(false);
            break;
        case fl::JSON_FLOAT:   
            FL_WARN("float");   
            CHECK(false);
            break;
        case fl::JSON_BOOLEAN: 
            FL_WARN("boolean"); 
            CHECK(false);
            break;
        case fl::JSON_STRING:  
            FL_WARN("string");  
            CHECK(true); // Should be string
            break;
        case fl::JSON_NULL:               
            FL_WARN("null");    
            CHECK(false);
            break;
    }
}

#else

TEST_CASE("JSON type detection - disabled") {
    // When JSON is disabled, the function should still compile but return JSON_NULL
    int dummy = 0;
    fl::JsonType result = fl::getJsonType(dummy);
    CHECK(result == fl::JSON_NULL);
    
    FL_WARN("JSON disabled - type detection returns JSON_NULL");
}

#endif // FASTLED_ENABLE_JSON

The problem is that fl::string wants to convert uint8 to a numerical representation. This is a decision we made to make it easy to view uint8 types as their int value. You are hitting an issue where pushing back on a string produces it's numerical representation. As an alternative you can do fl::string.push_ascii(...)


run `bash test json_roundtrip` to confirm implimentation.


# fl::string JsonValue::to_string()

DO ALL THE SERIALIZATION BY POPULATING A FLArduinoJson JsonDocument, then convert to std::string buffer, then convert to fl::string. You will have to include <string> in the json.cpp file and add // ok include

 read BUG.md, then describe what it says tthen run `bash test json_roundtrip` Implement the missing functionilty by building the FLArduinoJson JsonDocument then dumping that to a std::string buffer. The Json::toJson already does this, but JsonValue::toJson does not. JsonValue::toJson needs to build up a FLArduinoJson JsonDocument from it's values then call serialize to an std::string then convert to a fl::string


Failed tests summary:
Test test_json_roundtrip.exe failed with return code 1
Output:
  json.cpp(305): *** Json::to_string_native: Converting JsonValue to FLArduinoJson::JsonVariant
  json.cpp(305): *** Json::to_string_native: Converting JsonValue to FLArduinoJson::JsonVariant
  json.cpp(305): *** Json::to_string_native: Converting JsonValue to FLArduinoJson::JsonVariant
  Setting up Windows crash handler...
  Windows crash handler setup complete.
  [doctest] doctest version is "2.4.11"
  [doctest] run with "--help" for options
  ===============================================================================       
  C:/Users/niteris/dev/fastled/tests/test_json_roundtrip.cpp(6):
  TEST CASE:  JSON Round Trip Serialization Test

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(21): ERROR: CHECK_EQ( parsedJson["value"].as_or(0), 21 ) is NOT correct!
    values: CHECK_EQ( 0, 21 )

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(31): ERROR: CHECK_EQ( normalizedInitial, normalizedSerialized ) is NOT correct!
    values: CHECK_EQ( 123341109710910134583498111983444341189710811710134585049125, 123125 )

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(37): ERROR: CHECK( reparsedJson.contains("name") ) is NOT correct!
    values: CHECK( false )

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(38): ERROR: CHECK( reparsedJson.contains("value") ) is NOT correct!
    values: CHECK( false )

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(39): ERROR: CHECK_EQ( reparsedJson["name"].as_or(string("")), "bob" ) is NOT correct!
    values: CHECK_EQ( , bob )

  C:\Users\niteris\dev\fastled\tests\test_json_roundtrip.cpp(40): ERROR: CHECK_EQ( reparsedJson["value"].as_or(0), 21 ) is NOT correct!
    values: CHECK_EQ( 0, 21 )

  ===============================================================================       
  [doctest] test cases:  1 | 0 passed | 1 failed | 0 skipped
  [doctest] assertions: 13 | 7 passed | 6 failed |
  [doctest] Status: FAILURE!

1 test failed: test_json_roundtrip.exe
Command failed: uv run ci/cpp_test_run.py --clang --test json_roundtrip

2.81   /build_tools/ccache-emcxx.sh -c -x c++ -o /build/quick/Json.ino.o -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_USE_PROGMEM=0 -DUSE_OFFSET_CONVERTER=0 -DGL_ENABLE_GET_PROC_ADDRESS=0 -D_REENTRANT=1 -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -DFASTLED_HAS_NETWORKING=1 -std=gnu++17 -fpermissive -Wno-constant-logical-operand -Wnon-c-typedef-for-linkage -Werror=bad-function-cast -Werror=cast-function-type -fno-threadsafe-statics -fno-exceptions -fno-rtti -pthread -I. -Isrc -Isrc/platforms/wasm/compiler -DSKETCH_COMPILE=1 -DFASTLED_WASM_USE_CCALL -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -fno-merge-constants -fno-merge-all-constants -fno-delayed-template-parsing -fmax-type-align=4 -ffast-math -fno-math-errno -fno-exceptions 
-fno-rtti /js/src/Json.ino.cpp
2.81 🔧 Mode-specific flags: -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -fno-merge-constants -fno-merge-all-constants -fno-delayed-template-parsing -fmax-type-align=4 -ffast-math -fno-math-errno -fno-exceptions -fno-rtti
2.81 📤 Compiler output:
2.81 [emcc] clang++: warning: optimization flag '-fno-merge-constants' is not supported [-Wignored-optimization-argument]
2.81 [emcc] /js/src/Json.ino.cpp:34:5: error: use of undeclared identifier 'basicJsonParsingExample'
2.81 [emcc]    34 |     basicJsonParsingExample();
2.81 [emcc]       |     ^
2.81 [emcc] /js/src/Json.ino.cpp:37:5: error: use of undeclared identifier 'jsonTypeInspectionExample'
2.81 [emcc]    37 |     jsonTypeInspectionExample();
2.81 [emcc]       |     ^
2.81 [emcc] /js/src/Json.ino.cpp:40:5: error: use of undeclared identifier 'jsonCreationExample'
2.81 [emcc]    40 |     jsonCreationExample();
2.81 [emcc]       |     ^
2.81 [emcc] /js/src/Json.ino.cpp:43:5: error: use of undeclared identifier 'idealJsonApiDemo'
2.81 [emcc]    43 |     idealJsonApiDemo();
2.81 [emcc]       |     ^
2.81 [emcc] /js/src/Json.ino.cpp:109:28: error: no member named 'is_array' in 'FLArduinoJson::detail::MemberProxy<FLArduinoJson::JsonDocument &, const char *>'  
2.81 [emcc]   109 |         if (doc["effects"].is_array()) {
2.81 [emcc]       |             ~~~~~~~~~~~~~~ ^
2.81 [emcc] /js/src/Json.ino.cpp:112:44: error: no member named 'size' in 'fl::Json'
2.81 [emcc]   112 |             for (size_t i = 0; i < effects.size(); i++) {
2.81 [emcc]       |                                    ~~~~~~~ ^
2.81 [emcc] /js/src/Json.ino.cpp:115:33: error: no member named 'size' in 'fl::Json'
2.81 [emcc]   115 |                 if (i < effects.size() - 1) Serial.print(", ");
2.81 [emcc]       |                         ~~~~~~~ ^
2.81 [emcc] /js/src/Json.ino.cpp:148:28: error: no matching conversion for functional-style cast from 'fl::JsonDocument' to 'fl::Json'
2.81 [emcc]   148 |         auto jsonWrapper = fl::Json(doc);
2.81 [emcc]       |                            ^~~~~~~~~~~~~
2.81 [emcc] src/fl/json.h:62:5: note: candidate constructor not viable: no known conversion from 'fl::JsonDocument' to 'const Json' for 1st argument
2.81 [emcc]    62 |     Json(const Json& other) = default;
2.81 [emcc]       |     ^    ~~~~~~~~~~~~~~~~~
2.81 [emcc] src/fl/json.h:61:5: note: candidate constructor not viable: requires 0 arguments, but 1 was provided
2.81 [emcc]    61 |     Json();
2.81 [emcc]       |     ^
2.81 [emcc] /js/src/Json.ino.cpp:163:31: error: no member named 'type' in 'fl::Json'
2.81 [emcc]   163 |     fl::JsonType type = value.type();
2.81 [emcc]       |                         ~~~~~ ^
2.81 [emcc] /js/src/Json.ino.cpp:164:33: error: no member named 'type_str' in 'fl::Json'
2.81 [emcc]   164 |     const char* typeStr = value.type_str();
2.81 [emcc]       |                           ~~~~~ ^
2.81 [emcc] /js/src/Json.ino.cpp:175:47: error: expected '(' for function-style cast or type construction
2.81 [emcc]   175 |             auto strOpt = value.get<fl::string>();
2.81 [emcc]       |                                     ~~~~~~~~~~^
2.81 [emcc] /js/src/Json.ino.cpp:175:33: error: no member named 'get' in 'fl::Json'; did you mean 'set'?
2.81 [emcc]   175 |             auto strOpt = value.get<fl::string>();
2.81 [emcc]       |                                 ^~~
2.81 [emcc]       |                                 set
2.81 [emcc] src/fl/json.h:199:10: note: 'set' declared here
2.81 [emcc]   199 |     void set(const char* key, const Json& value); // For objects
2.81 [emcc]       |          ^
2.81 [emcc] /js/src/Json.ino.cpp:175:49: error: expected expression
2.81 [emcc]   175 |             auto strOpt = value.get<fl::string>();
2.81 [emcc]       |                                                 ^
2.81 [emcc] /js/src/Json.ino.cpp:183:40: error: expected '(' for function-style cast or type construction
2.81 [emcc]   183 |             auto intOpt = value.get<int>();
2.81 [emcc]       |                                     ~~~^
2.81 [emcc] /js/src/Json.ino.cpp:183:33: error: no member named 'get' in 'fl::Json'; did you mean 'set'?
2.81 [emcc]   183 |             auto intOpt = value.get<int>();
2.81 [emcc]       |                                 ^~~
2.81 [emcc]       |                                 set
2.81 [emcc] src/fl/json.h:199:10: note: 'set' declared here
2.81 [emcc]   199 |     void set(const char* key, const Json& value); // For objects
2.81 [emcc]       |          ^
2.81 [emcc] /js/src/Json.ino.cpp:183:42: error: expected expression
2.81 [emcc]   183 |             auto intOpt = value.get<int>();
2.81 [emcc]       |                                          ^
2.81 [emcc] /js/src/Json.ino.cpp:191:44: error: expected '(' for function-style cast or type construction
2.81 [emcc]   191 |             auto floatOpt = value.get<float>();
2.81 [emcc]       |                                       ~~~~~^
2.81 [emcc] /js/src/Json.ino.cpp:191:35: error: no member named 'get' in 'fl::Json'; did you mean 'set'?
2.81 [emcc]   191 |             auto floatOpt = value.get<float>();
2.81 [emcc]       |                                   ^~~
2.81 [emcc]       |                                   set
2.81 [emcc] src/fl/json.h:199:10: note: 'set' declared here
2.81 [emcc]   199 |     void set(const char* key, const Json& value); // For objects
2.81 [emcc]       |          ^
2.81 [emcc] /js/src/Json.ino.cpp:191:46: error: expected expression
2.81 [emcc]   191 |             auto floatOpt = value.get<float>();
2.81 [emcc]       |                                              ^
2.81 [emcc] fatal error: too many errors emitted, stopping now [-ferror-limit=]
2.81 [emcc] 20 errors generated.
2.81 ❌ Error compiling /js/src/Json.ino.cpp:
2.81 🛑 Cancelling remaining compilation tasks...
Compilation error: Error compiling /js/src/Json.ino.cpp: Compilation failed with exit code 1

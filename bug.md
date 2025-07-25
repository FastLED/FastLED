[1/2] /usr/bin/ccache em++ -DFASTLED_ALL_SRC=1 -DFASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_NO_AUTO_NAMESPACE -DFASTLED_NO_PINMAP -DFASTLED_STUB_IMPL -DHAS_HARDWARE_PIN_SUPPORT -DPROGMEM="" -Isrc -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_USE_PROGMEM=0 -DUSE_OFFSET_CONVERTER=0 -DGL_ENABLE_GET_PROC_ADDRESS=0 -D_REENTRANT=1 -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -DFASTLED_HAS_NETWORKING=1 -std=gnu++17 -fpermissive -Wno-constant-logical-operand -Wnon-c-typedef-for-linkage -Werror=bad-function-cast -Werror=cast-function-type -fno-threadsafe-statics -fno-exceptions -fno-rtti -pthread -emit-llvm -Wall -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -fno-merge-constants -fno-merge-all-constants -fno-delayed-template-parsing -fmax-type-align=4 -ffast-math -fno-math-errno -funwind-tables -ffunction-sections -fdata-sections -Werror=return-type -Werror=missing-declarations -Werror=uninitialized -Werror=array-bounds -Werror=null-dereference -Werror=deprecated-declarations -Wno-comment -Werror=suggest-override -Werror=non-virtual-dtor -Werror=switch-enum -Werror=delete-non-virtual-dtor -std=gnu++17 -MD -MT fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o -MF fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o.d -o fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o -c /build/quick/fastled/fastled_unified.cpp
FAILED: fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o 
/usr/bin/ccache em++ -DFASTLED_ALL_SRC=1 -DFASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_NO_AUTO_NAMESPACE -DFASTLED_NO_PINMAP -DFASTLED_STUB_IMPL -DHAS_HARDWARE_PIN_SUPPORT -DPROGMEM="" -Isrc -DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50 -DFASTLED_FORCE_NAMESPACE=1 -DFASTLED_USE_PROGMEM=0 -DUSE_OFFSET_CONVERTER=0 -DGL_ENABLE_GET_PROC_ADDRESS=0 -D_REENTRANT=1 -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -DFASTLED_HAS_NETWORKING=1 -std=gnu++17 -fpermissive -Wno-constant-logical-operand -Wnon-c-typedef-for-linkage -Werror=bad-function-cast -Werror=cast-function-type -fno-threadsafe-statics -fno-exceptions -fno-rtti -pthread -emit-llvm -Wall -O1 -g0 -fno-inline-functions -fno-vectorize -fno-unroll-loops -fno-strict-aliasing -fno-merge-constants -fno-merge-all-constants -fno-delayed-template-parsing -fmax-type-align=4 -ffast-math -fno-math-errno -funwind-tables -ffunction-sections -fdata-sections -Werror=return-type -Werror=missing-declarations -Werror=uninitialized -Werror=array-bounds -Werror=null-dereference -Werror=deprecated-declarations -Wno-comment -Werror=suggest-override -Werror=non-virtual-dtor -Werror=switch-enum -Werror=delete-non-virtual-dtor -std=gnu++17 -MD -MT fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o -MF fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o.d -o fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o -c /build/quick/fastled/fastled_unified.cpp
clang++: warning: optimization flag '-fno-merge-constants' is not supported [-Wignored-optimization-argument]
In file included from /build/quick/fastled/fastled_unified.cpp:107:
src/platforms/wasm/fs_wasm.cpp:318:23: error: no member named 'is_number' in 'fl::Json'
  318 |         if (!size_obj.is_number()) {
      |              ~~~~~~~~ ^
In file included from /build/quick/fastled/fastled_unified.cpp:109:
src/platforms/wasm/js_bindings.cpp:102:19: error: no matching member function for call to 'set'
  102 |         stripsObj.set(std::to_string(stripIndex), stripMapObj);
      |         ~~~~~~~~~~^~~
src/fl/detail/json_impl.cpp:629:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  629 | void Json::set(const char* key, const Json& value) {
      |            ^   ~~~~~~~~~~~~~~~
src/fl/detail/json_impl.cpp:635:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  635 | void Json::set(const char* key, int value) {
      |            ^   ~~~~~~~~~~~~~~~
src/fl/detail/json_impl.cpp:641:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  641 | void Json::set(const char* key, const char* value) {
      |            ^   ~~~~~~~~~~~~~~~
src/fl/detail/json_impl.cpp:647:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  647 | void Json::set(const char* key, const fl::string& value) {
      |            ^   ~~~~~~~~~~~~~~~
src/fl/detail/json_impl.cpp:653:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  653 | void Json::set(const char* key, float value) {
      |            ^   ~~~~~~~~~~~~~~~
src/fl/detail/json_impl.cpp:659:12: note: candidate function not viable: no known conversion from 'string' (aka 'basic_string') to 'const char *' for 1st argument
  659 | void Json::set(const char* key, bool value) {
      |            ^   ~~~~~~~~~~~~~~~
In file included from /build/quick/fastled/fastled_unified.cpp:109:
src/platforms/wasm/js_bindings.cpp:248:9: error: call to member function 'set' is ambiguous
  248 |     doc.set("timestamp", millis());
      |     ~~~~^~~
src/fl/detail/json_impl.cpp:635:12: note: candidate function
  635 | void Json::set(const char* key, int value) {
      |            ^
src/fl/detail/json_impl.cpp:653:12: note: candidate function
  653 | void Json::set(const char* key, float value) {
      |            ^
src/fl/detail/json_impl.cpp:659:12: note: candidate function
  659 | void Json::set(const char* key, bool value) {
      |            ^
In file included from /build/quick/fastled/fastled_unified.cpp:109:
src/platforms/wasm/js_bindings.cpp:277:9: error: call to member function 'set' is ambiguous
  277 |     doc.set("timestamp", millis());
      |     ~~~~^~~
src/fl/detail/json_impl.cpp:635:12: note: candidate function
  635 | void Json::set(const char* key, int value) {
      |            ^
src/fl/detail/json_impl.cpp:653:12: note: candidate function
  653 | void Json::set(const char* key, float value) {
      |            ^
src/fl/detail/json_impl.cpp:659:12: note: candidate function
  659 | void Json::set(const char* key, bool value) {
      |            ^
In file included from /build/quick/fastled/fastled_unified.cpp:109:
src/platforms/wasm/js_bindings.cpp:299:9: error: call to member function 'set' is ambiguous
  299 |     doc.set("length", screenmap.getLength());
      |     ~~~~^~~
src/fl/detail/json_impl.cpp:635:12: note: candidate function
  635 | void Json::set(const char* key, int value) {
      |            ^
src/fl/detail/json_impl.cpp:653:12: note: candidate function
  653 | void Json::set(const char* key, float value) {
      |            ^
src/fl/detail/json_impl.cpp:659:12: note: candidate function
  659 | void Json::set(const char* key, bool value) {
      |            ^
5 errors generated.
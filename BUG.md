BUG ISSUE (RESOLVED):

We were in the middle of a migration to fl::json2::JSon. These were compiler errors indicating incomplete transition:

fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o.d -o fastled/CMakeFiles/fastled.dir/fastled_unified.cpp.o -c /build/quick/fastled/fastled_unified.cpp
clang++: warning: optimization flag '-fno-merge-constants' is not supported [-Wignored-optimization-argument]
In file included from /build/quick/fastled/fastled_unified.cpp:105:
src/platforms/wasm/fs_wasm.cpp:323:40: error: invalid operands to binary expression ('Json' and 'const char[1]')
  323 |         fl::string path = file["path"] | "";
      |                           ~~~~~~~~~~~~ ^ ~~
src/crgb.h:799:27: note: candidate function not viable: no known conversion from 'Json' to 'const CRGB' for 1st argument
  799 | FASTLED_FORCE_INLINE CRGB operator|( const CRGB& p1, const CRGB& p2)
      |                           ^          ~~~~~~~~~~~~~~
src/fl/json.h:585:7: note: candidate template ignored: substitution failure [with T = char[1]]: function cannot return array type 'char[1]'
  585 |     T operator|(const T& fallback) const {
      |     ~ ^
1 error generated.
ninja: build stopped: subcommand failed.
  Processing 444 files with line ending conversion...
  Summary: 444 files processed, 0 updated, 444 unchanged
  No files were updated - libfastled recompilation will be suppressed if libraries exist
Fast sync from /host/fastled/src to src complete in 0.95 seconds
No source files changed, but recompiling due to missing libraries
src/platforms/wasm/fs_wasm.cpp:302:9: error: no member named 'parseJson' in namespace 'fl'
  302 |     fl::parseJson(jsonStr, &doc);
      |     ~~~~^
2 errors generated.
ninja: build stopped: subcommand failed.
  Processing 444 files with line ending conversion...
  Summary: 444 files processed, 0 updated, 444 unchanged
  No files were updated - libfastled recompilation will be suppressed if libraries exist
Fast sync from /host/fastled/src to src complete in 1.00 seconds
No source files changed, but recompiling due to missing libraries

ISSUE RESOLUTION:
1. Fixed the call to `fl::parseJson` by using `fl::json2::Json::parse(fl::string(jsonStr))` instead.
2. Fixed the operator| issue with string literals by using `fl::string("")` instead of `""`.

Both issues were related to the migration from the old JSON API to the new fl::json2::Json API.
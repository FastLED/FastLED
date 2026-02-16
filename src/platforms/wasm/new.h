// ok no namespace fl
#pragma once

// IWYU pragma: private

// WebAssembly placement new operator - in global namespace
// WASM/Emscripten has full C++ standard library support

// IWYU pragma: begin_keep
#include <new>  // ok include
// IWYU pragma: end_keep

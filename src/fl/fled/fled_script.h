#pragma once

// fl::FledScript - passive carrier for an embedded script section of a
// .fled v2 bundle. PR2 introduces the type; v1 bundles never carry a
// script section so fl::Fled::script() currently always returns nullptr.

#include "fl/stl/int.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

enum class ScriptKind : fl::u8 { Wasm, MicroPy };

struct FledScript {
    ScriptKind                   kind;
    fl::shared_ptr<const fl::u8> bytes;
    fl::size                     length;
};

} // namespace fl

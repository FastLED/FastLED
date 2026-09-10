#pragma once

#include "fl/stl/json.h"

namespace autoresearch {

/// Prove PIO0 + PIO1 simultaneous operation at the resource level (#3899).
///
/// runParallelTest already drives both blocks at once, but for this pair it
/// skips RX validation and asserts nothing about resources -- so a PASS there
/// means only that show() did not hang. This checks what the acceptance
/// criterion actually asks: no resource collision, no stale state machine,
/// no DMA ownership leak.
fl::json runRpPioParallelResourceTest(const fl::json& args);

} // namespace autoresearch

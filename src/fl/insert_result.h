#pragma once

namespace fl {

// Because of the fixed size nature of a lot of FastLED's containers we
// need to provide additional feedback to the caller about the nature of
// why an insert did or did not happen. Specifically, we want to differentiate
// between failing to insert because the item already existed and when the container
// was full.
enum InsertResult {
    kInserted = 0,
    kExists = 1,
    kMaxSize = 2,
};

}  // namespace fl

#include "fl/stl/ios.h"

namespace fl {

// Manipulator instances — empty-struct markers used as `operator<<` tag
// dispatch (`cout << hex`). Each is `sizeof == 1` in practice. Public
// API surface with the same source-compat constraint as `fl::cout` /
// `fl::cin`: renaming to `hex()` etc. would break every existing user
// stream expression. Migration deferred; annotated for the linter.
// FL_LINT_ALLOW_GLOBAL(stream manipulator tag `hex` — public API surface, empty struct, ~1 byte per manipulator)
const hex_t hex;
// FL_LINT_ALLOW_GLOBAL(stream manipulator tag `dec` — see `hex` above)
const dec_t dec;
// FL_LINT_ALLOW_GLOBAL(stream manipulator tag `oct` — see `hex` above)
const oct_t oct;

} // namespace fl

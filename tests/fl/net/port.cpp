// Host-side tests for range-checked port parsing (FastLED#3956).
//
// The peer-OTA RPC narrowed an untrusted `port` with a bare
// `static_cast<uint16_t>`, guarded only on the low side. That silently
// retargeted the update: 70000 wrapped to 4464, 65537 to 1. These pin the
// boundaries so a bad port is rejected instead of redirected.

#include "test.h"
#include "fl/net/port.h"

FL_TEST_FILE(FL_FILEPATH) {

using fl::net::tryParsePort;

FL_TEST_CASE("tryParsePort: accepts the full uint16 endpoint range") {
    FL_CHECK(tryParsePort(1).has_value());
    FL_CHECK_EQ(tryParsePort(1).value(), fl::u16(1));

    FL_CHECK(tryParsePort(8081).has_value());
    FL_CHECK_EQ(tryParsePort(8081).value(), fl::u16(8081));

    // The top of the range must be usable, not off-by-one rejected.
    FL_CHECK(tryParsePort(65535).has_value());
    FL_CHECK_EQ(tryParsePort(65535).value(), fl::u16(65535));
}

FL_TEST_CASE("tryParsePort: rejects zero and negative ports") {
    // Port 0 means "any port" to the socket layer, never an explicit endpoint.
    FL_CHECK_FALSE(tryParsePort(0).has_value());
    FL_CHECK_FALSE(tryParsePort(-1).has_value());
    FL_CHECK_FALSE(tryParsePort(-8081).has_value());
}

FL_TEST_CASE("tryParsePort: rejects values that would truncate") {
    // The regression: each of these used to narrow to a valid-looking but
    // completely different port.
    FL_CHECK_FALSE(tryParsePort(65536).has_value()); // would become 0
    FL_CHECK_FALSE(tryParsePort(65537).has_value()); // would become 1
    FL_CHECK_FALSE(tryParsePort(70000).has_value()); // would become 4464
    FL_CHECK_FALSE(tryParsePort(74001).has_value()); // would become 8465
}

FL_TEST_CASE("tryParsePort: rejects far-out-of-range magnitudes") {
    FL_CHECK_FALSE(tryParsePort(fl::i64(1) << 32).has_value());
    FL_CHECK_FALSE(tryParsePort(-(fl::i64(1) << 32)).has_value());
}

}

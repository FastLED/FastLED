// Host-side tests for the fl::net::wifi connection API (FastLED#3576
// Phase 6). The host build has no WiFi silicon, so this exercises the
// stub contract: every call must be a safe no-op reporting UNSUPPORTED
// — sketches using the API must run unmodified on WiFi-less targets.

#include "test.h"
#include "fl/net/wifi.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("wifi stub: status is UNSUPPORTED and calls are safe no-ops") {
    using namespace fl::net::wifi;

    FL_CHECK(status() == Status::UNSUPPORTED);
    FL_CHECK_FALSE(isConnected());

    FL_CHECK_FALSE(connectSta("ssid", "password"));
    FL_CHECK_FALSE(startAp("ap", "password", 1));
    stop(); // must not crash

    FL_CHECK(ipAddress().empty());
    FL_CHECK(apIpAddress().empty());
    FL_CHECK(status() == Status::UNSUPPORTED);
}

FL_TEST_CASE("wifi: toString covers every status") {
    using namespace fl::net::wifi;
    FL_CHECK_EQ(fl::string(toString(Status::UNSUPPORTED)), fl::string("UNSUPPORTED"));
    FL_CHECK_EQ(fl::string(toString(Status::IDLE)), fl::string("IDLE"));
    FL_CHECK_EQ(fl::string(toString(Status::CONNECTING)), fl::string("CONNECTING"));
    FL_CHECK_EQ(fl::string(toString(Status::CONNECTED)), fl::string("CONNECTED"));
    FL_CHECK_EQ(fl::string(toString(Status::FAILED)), fl::string("FAILED"));
    FL_CHECK_EQ(fl::string(toString(Status::AP_ACTIVE)), fl::string("AP_ACTIVE"));
}

}

// Host ASAN replica of runSingleTestImpl's response assembly (#3588).
// The device crash fires inside this exact json choreography (garbage
// shared_ptr control block released during set()). Replicated verbatim
// with fragmentation churn between iterations so ASAN can catch any
// address-level defect in the json/flat_map/vector machinery.

#include "test.h"

#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

fl::json buildPattern(int run, bool with_errors) {
    fl::json pat = fl::json::object();
    pat.set("runNumber", static_cast<int64_t>(run));
    pat.set("totalLeds", static_cast<int64_t>(10));
    pat.set("mismatchedLeds", static_cast<int64_t>(10));
    pat.set("capturedBytes", static_cast<int64_t>(0));
    pat.set("captureWaitResult", static_cast<int64_t>(0));
    pat.set("rawEdgesAfterWait", static_cast<int64_t>(0));
    pat.set("decodeOk", static_cast<int64_t>(-1));
    pat.set("decodeError", static_cast<int64_t>(-1));
    pat.set("decodeBytes", static_cast<int64_t>(0));
    pat.set("decodeOutputCapacity", static_cast<int64_t>(3300));
    fl::string sample;
    for (int i = 0; i < 24; ++i) {
        sample += (i & 1) ? "H893 " : "L356 ";
    }
    pat.set("rawEdgeSample", sample.c_str());
    pat.set("captureFailed", true);
    pat.set("mismatchedBytes", static_cast<int64_t>(30));
    pat.set("lsbOnlyErrors", static_cast<int64_t>(0));
    if (with_errors) {
        fl::json errs = fl::json::array();
        for (int ei = 0; ei < 5; ++ei) {
            fl::json err = fl::json::object();
            err.set("led", static_cast<int64_t>(ei));
            fl::json expected = fl::json::array();
            expected.push_back(static_cast<int64_t>(255));
            expected.push_back(static_cast<int64_t>(255));
            expected.push_back(static_cast<int64_t>(255));
            err.set("expected", expected);
            fl::json actual = fl::json::array();
            actual.push_back(static_cast<int64_t>(0));
            actual.push_back(static_cast<int64_t>(0));
            actual.push_back(static_cast<int64_t>(0));
            err.set("actual", actual);
            errs.push_back(err);
        }
        pat.set("errors", errs);
        pat.set("totalErrors", static_cast<int64_t>(10));
    }
    return pat;
}

} // namespace

FL_TEST_CASE("runSingleTest response assembly is address-clean") {
    // Fragmentation churn buffers to vary allocator reuse per iteration.
    fl::vector<fl::vector<char>> churn;
    for (int iter = 0; iter < 200; ++iter) {
        churn.push_back(fl::vector<char>());
        churn.back().resize((iter * 37) % 512 + 16);
        if (iter % 3 == 0 && churn.size() > 2) {
            churn.erase(churn.begin());
        }

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("passed", false);
        response.set("totalTests", static_cast<int64_t>(4));
        response.set("passedTests", static_cast<int64_t>(0));
        response.set("captureEvidenceExpected", true);
        response.set("captureEvidenceBytes", static_cast<int64_t>(0));
        response.set("captureEvidenceRawEdges", static_cast<int64_t>(0));
        response.set("duration_ms", static_cast<int64_t>(699));
        response.set("show_duration_ms", static_cast<int64_t>(684));
        response.set("driver", "I2S");
        response.set("requestedTxPin", static_cast<int64_t>(33));
        response.set("requestedRxPin", static_cast<int64_t>(34));
        response.set("actualTxPin", static_cast<int64_t>(33));
        response.set("actualRxPin", static_cast<int64_t>(34));
        response.set("captureBackend", "RMT");
        response.set("laneCount", static_cast<int64_t>(1));
        fl::json sizes_response = fl::json::array();
        sizes_response.push_back(static_cast<int64_t>(10));
        response.set("laneSizes", sizes_response);
        response.set("pattern", "ALL_ONES");
        response.set("useLegacyApi", false);
        response.set("frameCount", static_cast<int64_t>(1));
        response.set("contaminateTxMux", false);

        fl::json patterns = fl::json::array();
        for (int p = 0; p < 4; ++p) {
            patterns.push_back(buildPattern(p, p % 2 == 0));
        }
        response.set("patterns", patterns);

        fl::string serialized = response.to_string();
        FL_CHECK(serialized.size() > 400);
    }
}

}

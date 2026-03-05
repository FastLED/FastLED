// tests/fl/channels/detail/validation/result_formatter.cpp
//
// Unit tests for result formatter

#include "test.h"
#include "fl/channels/validation.h"
#include "fl/channels/detail/validation/result_formatter.h"
#include "fl/channels/detail/validation/result_formatter.cpp.hpp"

using namespace fl;

FL_TEST_CASE("formatSummaryTable - empty results") {
    vector<DriverTestResult> results;
    string table = fl::validation::formatSummaryTable(results);

    // Should still have header and footer
    FL_CHECK(table.find("DRIVER VALIDATION SUMMARY") != string::npos);
    FL_CHECK(table.find("╔") != string::npos);
    FL_CHECK(table.find("╚") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - single passed driver") {
    vector<DriverTestResult> results;
    DriverTestResult r("RMT");
    r.total_tests = 10;
    r.passed_tests = 10;
    r.skipped = false;
    results.push_back(r);

    string table = fl::validation::formatSummaryTable(results);

    FL_CHECK(table.find("RMT") != string::npos);
    FL_CHECK(table.find("PASS ✓") != string::npos);
    FL_CHECK(table.find("10") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - single failed driver") {
    vector<DriverTestResult> results;
    DriverTestResult r("SPI");
    r.total_tests = 10;
    r.passed_tests = 7;
    r.skipped = false;
    results.push_back(r);

    string table = fl::validation::formatSummaryTable(results);

    FL_CHECK(table.find("SPI") != string::npos);
    FL_CHECK(table.find("FAIL ✗") != string::npos);
    FL_CHECK(table.find("7") != string::npos);
    FL_CHECK(table.find("10") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - skipped driver") {
    vector<DriverTestResult> results;
    DriverTestResult r("PARLIO");
    r.skipped = true;
    results.push_back(r);

    string table = fl::validation::formatSummaryTable(results);

    FL_CHECK(table.find("PARLIO") != string::npos);
    FL_CHECK(table.find("SKIPPED") != string::npos);
    FL_CHECK(table.find("-") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - multiple drivers mixed results") {
    vector<DriverTestResult> results;

    DriverTestResult r1("RMT");
    r1.total_tests = 20;
    r1.passed_tests = 20;
    results.push_back(r1);

    DriverTestResult r2("SPI");
    r2.total_tests = 15;
    r2.passed_tests = 12;
    results.push_back(r2);

    DriverTestResult r3("PARLIO");
    r3.skipped = true;
    results.push_back(r3);

    string table = fl::validation::formatSummaryTable(results);

    FL_CHECK(table.find("RMT") != string::npos);
    FL_CHECK(table.find("SPI") != string::npos);
    FL_CHECK(table.find("PARLIO") != string::npos);
    FL_CHECK(table.find("PASS ✓") != string::npos);
    FL_CHECK(table.find("FAIL ✗") != string::npos);
    FL_CHECK(table.find("SKIPPED") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - driver with no tests") {
    vector<DriverTestResult> results;
    DriverTestResult r("I2S");
    r.total_tests = 0;
    r.passed_tests = 0;
    r.skipped = false;
    results.push_back(r);

    string table = fl::validation::formatSummaryTable(results);

    FL_CHECK(table.find("I2S") != string::npos);
    FL_CHECK(table.find("NO TESTS") != string::npos);
}

FL_TEST_CASE("formatSummaryTable - long driver name truncation") {
    vector<DriverTestResult> results;
    DriverTestResult r("VERYLONGDRIVERNAME");
    r.total_tests = 5;
    r.passed_tests = 5;
    results.push_back(r);

    string table = fl::validation::formatSummaryTable(results);

    // Should be truncated to 12 chars
    FL_CHECK(table.find("VERYLONGDRIV") != string::npos);
}

FL_TEST_CASE("DriverTestResult - allPassed") {
    DriverTestResult r("RMT");
    r.total_tests = 10;
    r.passed_tests = 10;
    r.skipped = false;

    FL_CHECK(r.allPassed());
    FL_CHECK_FALSE(r.anyFailed());
}

FL_TEST_CASE("DriverTestResult - anyFailed") {
    DriverTestResult r("SPI");
    r.total_tests = 10;
    r.passed_tests = 7;
    r.skipped = false;

    FL_CHECK_FALSE(r.allPassed());
    FL_CHECK(r.anyFailed());
}

FL_TEST_CASE("DriverTestResult - skipped") {
    DriverTestResult r("PARLIO");
    r.skipped = true;

    FL_CHECK_FALSE(r.allPassed());
    FL_CHECK_FALSE(r.anyFailed());
}

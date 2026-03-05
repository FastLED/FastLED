#include "fl/stl/asio/error_code.h"
#include "fl/stl/asio/error_code.cpp.hpp"

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::asio;

FL_TEST_CASE("error_code - Default is success") {
    error_code ec;
    FL_CHECK(ec.ok());
    FL_CHECK_FALSE(static_cast<bool>(ec));
    FL_CHECK_EQ(ec.code, errc::success);
}

FL_TEST_CASE("error_code - Construction from errc") {
    error_code ec(errc::connection_refused);
    FL_CHECK_FALSE(ec.ok());
    FL_CHECK(static_cast<bool>(ec));
    FL_CHECK_EQ(ec.code, errc::connection_refused);
}

FL_TEST_CASE("error_code - Construction with message") {
    error_code ec(errc::timed_out, "request timed out");
    FL_CHECK_FALSE(ec.ok());
    FL_CHECK_EQ(ec.code, errc::timed_out);
    FL_CHECK_FALSE(ec.message.empty());
}

FL_TEST_CASE("error_code - Bool conversion for if-checks") {
    error_code ok_ec;
    error_code err_ec(errc::eof);

    // Asio-style: if (ec) means "if error"
    if (ok_ec) {
        FL_CHECK(false); // should not reach
    }
    if (err_ec) {
        FL_CHECK(true); // should reach
    }
}

FL_TEST_CASE("error_code - Equality comparison") {
    error_code a(errc::would_block);
    error_code b(errc::would_block);
    error_code c(errc::eof);

    FL_CHECK(a == b);
    FL_CHECK(a != c);
}

FL_TEST_CASE("error_code - Convert to/from fl::Error") {
    // Error -> error_code
    fl::Error err("something failed");
    error_code ec = error_code::from_error(err);
    FL_CHECK_FALSE(ec.ok());
    FL_CHECK_EQ(ec.code, errc::unknown);

    // error_code -> Error
    error_code ec2(errc::connection_reset, "reset by peer");
    fl::Error err2 = ec2.to_error();
    FL_CHECK_FALSE(err2.is_empty());

    // Success round-trip
    fl::Error empty_err;
    error_code ec3 = error_code::from_error(empty_err);
    FL_CHECK(ec3.ok());

    error_code success_ec;
    fl::Error err3 = success_ec.to_error();
    FL_CHECK(err3.is_empty());
}

FL_TEST_CASE("error_code - All errc values are distinct") {
    FL_CHECK(errc::success != errc::connection_refused);
    FL_CHECK(errc::connection_refused != errc::connection_reset);
    FL_CHECK(errc::connection_reset != errc::timed_out);
    FL_CHECK(errc::timed_out != errc::host_not_found);
    FL_CHECK(errc::host_not_found != errc::address_in_use);
    FL_CHECK(errc::address_in_use != errc::would_block);
    FL_CHECK(errc::would_block != errc::operation_aborted);
    FL_CHECK(errc::operation_aborted != errc::eof);
    FL_CHECK(errc::eof != errc::unknown);
}

} // FL_TEST_FILE

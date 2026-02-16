#include "fl/stl/map.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_CASE("fl::map basic usage") {
    fl::map<fl::string, fl::string> headers;

    fl::string name = "Content-Type";
    fl::string value = "application/json";

    headers[name] = value;

    FL_CHECK(headers.size() == 1);
    FL_CHECK(headers["Content-Type"] == "application/json");
}

#include "test.h"
#include "fl/json.h"

TEST_CASE("Test simple JSON parsing") {
    const char* jsonStr = "{\"map\":{\"strip1\":{\"x\":[0,1,2],\"y\":[0,0,0],\"diameter\":0.5}}}";
    
    fl::Json parsedJson = fl::Json::parse(jsonStr);
    CHECK(parsedJson.is_object());
    CHECK(parsedJson.contains("map"));
    
    if (parsedJson.contains("map")) {
        fl::Json mapObj = parsedJson["map"];
        CHECK(mapObj.is_object());
        CHECK(mapObj.contains("strip1"));
        
        if (mapObj.contains("strip1")) {
            fl::Json strip1Obj = mapObj["strip1"];
            CHECK(strip1Obj.is_object());
            CHECK(strip1Obj.contains("x"));
            CHECK(strip1Obj.contains("y"));
            CHECK(strip1Obj.contains("diameter"));
        }
    }
}
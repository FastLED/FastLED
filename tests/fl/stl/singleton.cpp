#include "test.h"
#include "fl/stl/singleton.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("singleton registry matches keys by content, not address") {
    // FL_PRETTY_FUNCTION keys can live at different addresses (per-DLL
    // copies, or flash-resident strings on ESP8266), so lookups must compare
    // the characters.
    // The registry keeps key pointers forever, so keys need static storage.
    static char keyA[] = "fl_test_registry_key";
    static char keyB[] = "fl_test_registry_key";
    FL_CHECK_NE(static_cast<const void*>(keyA), static_cast<const void*>(keyB));

    int value = 42;
    detail::singleton_registry_set(keyA, &value);
    FL_CHECK_EQ(detail::singleton_registry_get(keyB), &value);
    FL_CHECK_EQ(detail::singleton_registry_get("fl_test_registry_other"),
                static_cast<void*>(nullptr));
    FL_CHECK_EQ(detail::singleton_registry_get("fl_test_registry_ke"),
                static_cast<void*>(nullptr));
}

FL_TEST_CASE("SingletonShared returns one instance per type") {
    struct Counter {
        int hits = 0;
    };
    SingletonShared<Counter>::instance().hits++;
    SingletonShared<Counter>::instance().hits++;
    FL_CHECK_EQ(SingletonShared<Counter>::instance().hits, 2);
    FL_CHECK_EQ(SingletonShared<Counter>::instanceRef(),
                &SingletonShared<Counter>::instance());
}

}  // FL_TEST_FILE


// g++ --std=c++11 test.cpp

#include "test.h"


#include "fl/hashmap.h"


using namespace fl;


FASTLED_DEFINE_POD_HASH_FUNCTION(char);   // TODO: move this to fl/hash.h


TEST_CASE("Empty map") {
    HashMap<int,int> m;
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(!m.find(42));
}

TEST_CASE("Single insert & lookup") {
    HashMap<int,int> m;
    m.insert(10, 20);
    REQUIRE_EQ(m.size(), 1u);

    auto *v = m.find(10);
    REQUIRE(v);
    REQUIRE_EQ(*v, 20);
}

TEST_CASE("Insert duplicate key overwrites") {
    HashMap<int,std::string> m;
    m.insert(1, "foo");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE(*m.find(1) == "foo");

    // reinserting same key should update, not grow
    m.insert(1, "bar");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE(*m.find(1) == "bar");
}

TEST_CASE("Multiple distinct inserts & lookups") {
    HashMap<char,int> m;
    for(char c = 'a'; c < 'a'+10; ++c) {
        m.insert(c, c - 'a');
    }
    REQUIRE_EQ(m.size(), 10u);

    for(char c = 'a'; c < 'a'+10; ++c) {
        auto *v = m.find(c);
        REQUIRE(v);
        REQUIRE_EQ(*v, static_cast<int>(c - 'a'));
    }

    // missing key
    REQUIRE(!m.find('z'));
}

TEST_CASE("Erase existing keys") {
    HashMap<int,int> m;
    m.insert(5, 55);
    m.insert(6, 66);
    REQUIRE_EQ(m.size(), 2u);

    REQUIRE(m.erase(5));
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE(!m.find(5));

    REQUIRE(!m.erase(5));  // already gone
    REQUIRE_EQ(m.size(), 1u);

    REQUIRE(m.erase(6));
    REQUIRE_EQ(m.size(), 0u);
}

TEST_CASE("Clear map") {
    HashMap<int,int> m;
    for(int i = 0; i < 50; ++i) m.insert(i, i*2);
    REQUIRE_EQ(m.size(), 50u);

    m.clear();
    REQUIRE_EQ(m.size(), 0u);
    for(int i = 0; i < 50; ++i)
        REQUIRE(!m.find(i));
}

TEST_CASE("Stress insert & rehash behavior") {
    // Insert enough elements to force a resize/rehash
    HashMap<int,int> m(4 /* initial bucket count */);
    const int N = 500;
    for(int i = 0; i < N; ++i) {
        m.insert(i, i*i);
    }
    REQUIRE(m.size() == static_cast<std::size_t>(N));

    // All entries should be findable
    for(int i = 0; i < N; ++i){
        auto *v = m.find(i);
        REQUIRE(v);
        REQUIRE_EQ(*v, i*i);
    }
}

// TEST_CASE("Iterator round-trip (if supported)") {
//     HashMap<int,int> m;
//     for(int i = 0; i < 20; ++i) m.insert(i, i+100);

//     std::size_t count = 0;
//     for(auto & kv : m) {
//         // kv is a std::pair<const Key&, Value&>
//         REQUIRE(kv.second == kv.first + 100);
//         ++count;
//     }
//     REQUIRE_EQ(count, m.size());
// }
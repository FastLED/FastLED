/// @file string_view.cpp
/// @brief Test for fl::string_view - non-owning string view

#include "fl/string_view.h"
#include "doctest.h"
#include "fl/stl/string.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("string_view - construction") {
    // Default constructor
    string_view empty;
    CHECK(empty.empty());
    CHECK(empty.size() == 0);

    // From C string
    string_view from_cstr("hello");
    CHECK(from_cstr.size() == 5);
    CHECK_FALSE(from_cstr.empty());
    CHECK(from_cstr[0] == 'h');
    CHECK(from_cstr[4] == 'o');

    // From pointer and length
    const char* data = "world";
    string_view from_ptr(data, 3);
    CHECK(from_ptr.size() == 3);
    CHECK(from_ptr[0] == 'w');
    CHECK(from_ptr[2] == 'r');

    // From C array
    const char arr[] = "test";
    string_view from_arr(arr);
    CHECK(from_arr.size() == 4);

    // From fl::string
    fl::string str("fastled");
    string_view from_str(str);
    CHECK(from_str.size() == 7);
    CHECK(from_str[0] == 'f');
}

TEST_CASE("string_view - element access") {
    string_view sv("hello");

    // operator[]
    CHECK(sv[0] == 'h');
    CHECK(sv[4] == 'o');

    // at()
    CHECK(sv.at(0) == 'h');
    CHECK(sv.at(4) == 'o');

    // front() / back()
    CHECK(sv.front() == 'h');
    CHECK(sv.back() == 'o');

    // data()
    CHECK(sv.data() != nullptr);
    CHECK(sv.data()[0] == 'h');
}

TEST_CASE("string_view - iterators") {
    string_view sv("abc");

    // Forward iteration
    auto it = sv.begin();
    CHECK(*it == 'a');
    ++it;
    CHECK(*it == 'b');
    ++it;
    CHECK(*it == 'c');
    ++it;
    CHECK(it == sv.end());

    // Range-based for
    char expected = 'a';
    for (char c : sv) {
        CHECK(c == expected);
        ++expected;
    }
}

TEST_CASE("string_view - capacity") {
    string_view empty;
    CHECK(empty.size() == 0);
    CHECK(empty.length() == 0);
    CHECK(empty.empty());

    string_view non_empty("test");
    CHECK(non_empty.size() == 4);
    CHECK(non_empty.length() == 4);
    CHECK_FALSE(non_empty.empty());
}

TEST_CASE("string_view - modifiers") {
    // remove_prefix
    string_view sv1("hello");
    sv1.remove_prefix(2);
    CHECK(sv1.size() == 3);
    CHECK(sv1[0] == 'l');

    // remove_suffix
    string_view sv2("world");
    sv2.remove_suffix(2);
    CHECK(sv2.size() == 3);
    CHECK(sv2[2] == 'r');

    // swap
    string_view a("abc");
    string_view b("defgh");
    a.swap(b);
    CHECK(a.size() == 5);
    CHECK(b.size() == 3);
    CHECK(a[0] == 'd');
    CHECK(b[0] == 'a');
}

TEST_CASE("string_view - substr") {
    string_view sv("hello world");

    // Basic substr
    string_view sub1 = sv.substr(0, 5);
    CHECK(sub1.size() == 5);
    CHECK(sub1[0] == 'h');
    CHECK(sub1[4] == 'o');

    // Substr from middle
    string_view sub2 = sv.substr(6, 5);
    CHECK(sub2.size() == 5);
    CHECK(sub2[0] == 'w');

    // Substr to end
    string_view sub3 = sv.substr(6);
    CHECK(sub3.size() == 5);
    CHECK(sub3[0] == 'w');

    // Out of bounds returns empty
    string_view sub4 = sv.substr(100);
    CHECK(sub4.empty());
}

TEST_CASE("string_view - comparison") {
    string_view a("abc");
    string_view b("abc");
    string_view c("def");
    string_view d("ab");

    // Equality
    CHECK(a == b);
    CHECK_FALSE(a == c);
    CHECK(a != c);

    // Ordering
    CHECK(a < c);
    CHECK(c > a);
    CHECK(a <= b);
    CHECK(a >= b);

    // Different lengths
    CHECK(d < a);
    CHECK(a > d);
}

TEST_CASE("string_view - find") {
    string_view sv("hello world");

    // Find character
    CHECK(sv.find('h') == 0);
    CHECK(sv.find('w') == 6);
    CHECK(sv.find('d') == 10);
    CHECK(sv.find('x') == string_view::npos);

    // Find from position
    CHECK(sv.find('l', 3) == 3);
    CHECK(sv.find('l', 4) == 9);

    // Find substring
    CHECK(sv.find("hello") == 0);
    CHECK(sv.find("world") == 6);
    CHECK(sv.find("lo wo") == 3);
    CHECK(sv.find("xyz") == string_view::npos);

    // Empty substring
    CHECK(sv.find("") == 0);
}

TEST_CASE("string_view - rfind") {
    string_view sv("hello hello");

    // Reverse find character
    CHECK(sv.rfind('h') == 6);
    CHECK(sv.rfind('o') == 10);
    CHECK(sv.rfind('l') == 9);
    CHECK(sv.rfind('x') == string_view::npos);

    // Reverse find substring
    CHECK(sv.rfind("hello") == 6);
    CHECK(sv.rfind("ello") == 7);
    CHECK(sv.rfind("xyz") == string_view::npos);
}

TEST_CASE("string_view - find_first_of") {
    string_view sv("hello world");

    // Find first of character set
    CHECK(sv.find_first_of("aeiou") == 1);  // 'e'
    CHECK(sv.find_first_of("xyz") == string_view::npos);
    CHECK(sv.find_first_of("w") == 6);

    // From position
    CHECK(sv.find_first_of("aeiou", 2) == 4);  // 'o'
}

TEST_CASE("string_view - find_last_of") {
    string_view sv("hello world");

    // Find last of character set
    CHECK(sv.find_last_of("aeiou") == 7);  // 'o' in "world"
    CHECK(sv.find_last_of("xyz") == string_view::npos);
    CHECK(sv.find_last_of("h") == 0);
}

TEST_CASE("string_view - find_first_not_of") {
    string_view sv("aaabbbccc");

    // Find first not of character set
    CHECK(sv.find_first_not_of("a") == 3);  // First 'b'
    CHECK(sv.find_first_not_of("ab") == 6);  // First 'c'
    CHECK(sv.find_first_not_of("abc") == string_view::npos);
}

TEST_CASE("string_view - find_last_not_of") {
    string_view sv("aaabbbccc");

    // Find last not of character set
    CHECK(sv.find_last_not_of("c") == 5);  // Last 'b'
    CHECK(sv.find_last_not_of("bc") == 2);  // Last 'a'
    CHECK(sv.find_last_not_of("abc") == string_view::npos);
}

TEST_CASE("string_view - starts_with") {
    string_view sv("hello world");

    // starts_with substring
    CHECK(sv.starts_with("hello"));
    CHECK(sv.starts_with("h"));
    CHECK_FALSE(sv.starts_with("world"));
    CHECK_FALSE(sv.starts_with("hello world!"));

    // starts_with character
    CHECK(sv.starts_with('h'));
    CHECK_FALSE(sv.starts_with('w'));
}

TEST_CASE("string_view - ends_with") {
    string_view sv("hello world");

    // ends_with substring
    CHECK(sv.ends_with("world"));
    CHECK(sv.ends_with("d"));
    CHECK_FALSE(sv.ends_with("hello"));
    CHECK_FALSE(sv.ends_with("!world"));

    // ends_with character
    CHECK(sv.ends_with('d'));
    CHECK_FALSE(sv.ends_with('h'));
}

TEST_CASE("string_view - contains") {
    string_view sv("hello world");

    // contains substring
    CHECK(sv.contains("hello"));
    CHECK(sv.contains("world"));
    CHECK(sv.contains("o w"));
    CHECK_FALSE(sv.contains("xyz"));

    // contains character
    CHECK(sv.contains('h'));
    CHECK(sv.contains('w'));
    CHECK_FALSE(sv.contains('x'));
}

TEST_CASE("string_view - copy") {
    string_view sv("hello world");
    char buffer[20] = {0};

    // Copy substring
    fl::size copied = sv.copy(buffer, 5, 0);
    CHECK(copied == 5);
    CHECK(buffer[0] == 'h');
    CHECK(buffer[4] == 'o');

    // Copy from middle
    char buffer2[20] = {0};
    copied = sv.copy(buffer2, 5, 6);
    CHECK(copied == 5);
    CHECK(buffer2[0] == 'w');

    // Copy beyond end
    char buffer3[20] = {0};
    copied = sv.copy(buffer3, 100, 6);
    CHECK(copied == 5);  // Only 5 chars available
}

TEST_CASE("string_view - compare") {
    string_view a("abc");
    string_view b("abc");
    string_view c("abd");
    string_view d("ab");

    // Basic comparison
    CHECK(a.compare(b) == 0);
    CHECK(a.compare(c) < 0);
    CHECK(c.compare(a) > 0);
    CHECK(a.compare(d) > 0);
    CHECK(d.compare(a) < 0);

    // Compare with C string
    CHECK(a.compare("abc") == 0);
    CHECK(a.compare("abd") < 0);
}

TEST_CASE("string_view - hash") {
    string_view a("hello");
    string_view b("hello");
    string_view c("world");

    // Same strings should have same hash
    CHECK(hash_string_view(a) == hash_string_view(b));

    // Different strings should (usually) have different hashes
    CHECK(hash_string_view(a) != hash_string_view(c));
}

TEST_CASE("string_view - edge cases") {
    // Empty string
    string_view empty("");
    CHECK(empty.empty());
    CHECK(empty.size() == 0);
    CHECK(empty.find('x') == string_view::npos);
    CHECK(empty.starts_with(""));
    CHECK(empty.ends_with(""));

    // Single character
    string_view single("a");
    CHECK(single.size() == 1);
    CHECK(single[0] == 'a');
    CHECK(single.find('a') == 0);

    // Null data with zero length
    string_view null_view(nullptr, 0);
    CHECK(null_view.empty());
}

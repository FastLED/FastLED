/// @file string_view.cpp
/// @brief Test for fl::string_view - non-owning string view

#include "fl/string_view.h"
#include "test.h"
#include "fl/stl/string.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("string_view - construction") {
    // Default constructor
    string_view empty;
    FL_CHECK(empty.empty());
    FL_CHECK(empty.size() == 0);

    // From C string
    string_view from_cstr("hello");
    FL_CHECK(from_cstr.size() == 5);
    FL_CHECK_FALSE(from_cstr.empty());
    FL_CHECK(from_cstr[0] == 'h');
    FL_CHECK(from_cstr[4] == 'o');

    // From pointer and length
    const char* data = "world";
    string_view from_ptr(data, 3);
    FL_CHECK(from_ptr.size() == 3);
    FL_CHECK(from_ptr[0] == 'w');
    FL_CHECK(from_ptr[2] == 'r');

    // From C array
    const char arr[] = "test";
    string_view from_arr(arr);
    FL_CHECK(from_arr.size() == 4);

    // From fl::string
    fl::string str("fastled");
    string_view from_str(str);
    FL_CHECK(from_str.size() == 7);
    FL_CHECK(from_str[0] == 'f');
}

FL_TEST_CASE("string_view - element access") {
    string_view sv("hello");

    // operator[]
    FL_CHECK(sv[0] == 'h');
    FL_CHECK(sv[4] == 'o');

    // at()
    FL_CHECK(sv.at(0) == 'h');
    FL_CHECK(sv.at(4) == 'o');

    // front() / back()
    FL_CHECK(sv.front() == 'h');
    FL_CHECK(sv.back() == 'o');

    // data()
    FL_CHECK(sv.data() != nullptr);
    FL_CHECK(sv.data()[0] == 'h');
}

FL_TEST_CASE("string_view - iterators") {
    string_view sv("abc");

    // Forward iteration
    auto it = sv.begin();
    FL_CHECK(*it == 'a');
    ++it;
    FL_CHECK(*it == 'b');
    ++it;
    FL_CHECK(*it == 'c');
    ++it;
    FL_CHECK(it == sv.end());

    // Range-based for
    char expected = 'a';
    for (char c : sv) {
        FL_CHECK(c == expected);
        ++expected;
    }
}

FL_TEST_CASE("string_view - capacity") {
    string_view empty;
    FL_CHECK(empty.size() == 0);
    FL_CHECK(empty.length() == 0);
    FL_CHECK(empty.empty());

    string_view non_empty("test");
    FL_CHECK(non_empty.size() == 4);
    FL_CHECK(non_empty.length() == 4);
    FL_CHECK_FALSE(non_empty.empty());
}

FL_TEST_CASE("string_view - modifiers") {
    // remove_prefix
    string_view sv1("hello");
    sv1.remove_prefix(2);
    FL_CHECK(sv1.size() == 3);
    FL_CHECK(sv1[0] == 'l');

    // remove_suffix
    string_view sv2("world");
    sv2.remove_suffix(2);
    FL_CHECK(sv2.size() == 3);
    FL_CHECK(sv2[2] == 'r');

    // swap
    string_view a("abc");
    string_view b("defgh");
    a.swap(b);
    FL_CHECK(a.size() == 5);
    FL_CHECK(b.size() == 3);
    FL_CHECK(a[0] == 'd');
    FL_CHECK(b[0] == 'a');
}

FL_TEST_CASE("string_view - substr") {
    string_view sv("hello world");

    // Basic substr
    string_view sub1 = sv.substr(0, 5);
    FL_CHECK(sub1.size() == 5);
    FL_CHECK(sub1[0] == 'h');
    FL_CHECK(sub1[4] == 'o');

    // Substr from middle
    string_view sub2 = sv.substr(6, 5);
    FL_CHECK(sub2.size() == 5);
    FL_CHECK(sub2[0] == 'w');

    // Substr to end
    string_view sub3 = sv.substr(6);
    FL_CHECK(sub3.size() == 5);
    FL_CHECK(sub3[0] == 'w');

    // Out of bounds returns empty
    string_view sub4 = sv.substr(100);
    FL_CHECK(sub4.empty());
}

FL_TEST_CASE("string_view - comparison") {
    string_view a("abc");
    string_view b("abc");
    string_view c("def");
    string_view d("ab");

    // Equality
    FL_CHECK(a == b);
    FL_CHECK_FALSE(a == c);
    FL_CHECK(a != c);

    // Ordering
    FL_CHECK(a < c);
    FL_CHECK(c > a);
    FL_CHECK(a <= b);
    FL_CHECK(a >= b);

    // Different lengths
    FL_CHECK(d < a);
    FL_CHECK(a > d);
}

FL_TEST_CASE("string_view - find") {
    string_view sv("hello world");

    // Find character
    FL_CHECK(sv.find('h') == 0);
    FL_CHECK(sv.find('w') == 6);
    FL_CHECK(sv.find('d') == 10);
    FL_CHECK(sv.find('x') == string_view::npos);

    // Find from position
    FL_CHECK(sv.find('l', 3) == 3);
    FL_CHECK(sv.find('l', 4) == 9);

    // Find substring
    FL_CHECK(sv.find("hello") == 0);
    FL_CHECK(sv.find("world") == 6);
    FL_CHECK(sv.find("lo wo") == 3);
    FL_CHECK(sv.find("xyz") == string_view::npos);

    // Empty substring
    FL_CHECK(sv.find("") == 0);
}

FL_TEST_CASE("string_view - rfind") {
    string_view sv("hello hello");

    // Reverse find character
    FL_CHECK(sv.rfind('h') == 6);
    FL_CHECK(sv.rfind('o') == 10);
    FL_CHECK(sv.rfind('l') == 9);
    FL_CHECK(sv.rfind('x') == string_view::npos);

    // Reverse find substring
    FL_CHECK(sv.rfind("hello") == 6);
    FL_CHECK(sv.rfind("ello") == 7);
    FL_CHECK(sv.rfind("xyz") == string_view::npos);
}

FL_TEST_CASE("string_view - find_first_of") {
    string_view sv("hello world");

    // Find first of character set
    FL_CHECK(sv.find_first_of("aeiou") == 1);  // 'e'
    FL_CHECK(sv.find_first_of("xyz") == string_view::npos);
    FL_CHECK(sv.find_first_of("w") == 6);

    // From position
    FL_CHECK(sv.find_first_of("aeiou", 2) == 4);  // 'o'
}

FL_TEST_CASE("string_view - find_last_of") {
    string_view sv("hello world");

    // Find last of character set
    FL_CHECK(sv.find_last_of("aeiou") == 7);  // 'o' in "world"
    FL_CHECK(sv.find_last_of("xyz") == string_view::npos);
    FL_CHECK(sv.find_last_of("h") == 0);
}

FL_TEST_CASE("string_view - find_first_not_of") {
    string_view sv("aaabbbccc");

    // Find first not of character set
    FL_CHECK(sv.find_first_not_of("a") == 3);  // First 'b'
    FL_CHECK(sv.find_first_not_of("ab") == 6);  // First 'c'
    FL_CHECK(sv.find_first_not_of("abc") == string_view::npos);
}

FL_TEST_CASE("string_view - find_last_not_of") {
    string_view sv("aaabbbccc");

    // Find last not of character set
    FL_CHECK(sv.find_last_not_of("c") == 5);  // Last 'b'
    FL_CHECK(sv.find_last_not_of("bc") == 2);  // Last 'a'
    FL_CHECK(sv.find_last_not_of("abc") == string_view::npos);
}

FL_TEST_CASE("string_view - starts_with") {
    string_view sv("hello world");

    // starts_with substring
    FL_CHECK(sv.starts_with("hello"));
    FL_CHECK(sv.starts_with("h"));
    FL_CHECK_FALSE(sv.starts_with("world"));
    FL_CHECK_FALSE(sv.starts_with("hello world!"));

    // starts_with character
    FL_CHECK(sv.starts_with('h'));
    FL_CHECK_FALSE(sv.starts_with('w'));
}

FL_TEST_CASE("string_view - ends_with") {
    string_view sv("hello world");

    // ends_with substring
    FL_CHECK(sv.ends_with("world"));
    FL_CHECK(sv.ends_with("d"));
    FL_CHECK_FALSE(sv.ends_with("hello"));
    FL_CHECK_FALSE(sv.ends_with("!world"));

    // ends_with character
    FL_CHECK(sv.ends_with('d'));
    FL_CHECK_FALSE(sv.ends_with('h'));
}

FL_TEST_CASE("string_view - contains") {
    string_view sv("hello world");

    // contains substring
    FL_CHECK(sv.contains("hello"));
    FL_CHECK(sv.contains("world"));
    FL_CHECK(sv.contains("o w"));
    FL_CHECK_FALSE(sv.contains("xyz"));

    // contains character
    FL_CHECK(sv.contains('h'));
    FL_CHECK(sv.contains('w'));
    FL_CHECK_FALSE(sv.contains('x'));
}

FL_TEST_CASE("string_view - copy") {
    string_view sv("hello world");
    char buffer[20] = {0};

    // Copy substring
    fl::size copied = sv.copy(buffer, 5, 0);
    FL_CHECK(copied == 5);
    FL_CHECK(buffer[0] == 'h');
    FL_CHECK(buffer[4] == 'o');

    // Copy from middle
    char buffer2[20] = {0};
    copied = sv.copy(buffer2, 5, 6);
    FL_CHECK(copied == 5);
    FL_CHECK(buffer2[0] == 'w');

    // Copy beyond end
    char buffer3[20] = {0};
    copied = sv.copy(buffer3, 100, 6);
    FL_CHECK(copied == 5);  // Only 5 chars available
}

FL_TEST_CASE("string_view - compare") {
    string_view a("abc");
    string_view b("abc");
    string_view c("abd");
    string_view d("ab");

    // Basic comparison
    FL_CHECK(a.compare(b) == 0);
    FL_CHECK(a.compare(c) < 0);
    FL_CHECK(c.compare(a) > 0);
    FL_CHECK(a.compare(d) > 0);
    FL_CHECK(d.compare(a) < 0);

    // Compare with C string
    FL_CHECK(a.compare("abc") == 0);
    FL_CHECK(a.compare("abd") < 0);
}

FL_TEST_CASE("string_view - hash") {
    string_view a("hello");
    string_view b("hello");
    string_view c("world");

    // Same strings should have same hash
    FL_CHECK(hash_string_view(a) == hash_string_view(b));

    // Different strings should (usually) have different hashes
    FL_CHECK(hash_string_view(a) != hash_string_view(c));
}

FL_TEST_CASE("string_view - edge cases") {
    // Empty string
    string_view empty("");
    FL_CHECK(empty.empty());
    FL_CHECK(empty.size() == 0);
    FL_CHECK(empty.find('x') == string_view::npos);
    FL_CHECK(empty.starts_with(""));
    FL_CHECK(empty.ends_with(""));

    // Single character
    string_view single("a");
    FL_CHECK(single.size() == 1);
    FL_CHECK(single[0] == 'a');
    FL_CHECK(single.find('a') == 0);

    // Null data with zero length
    string_view null_view(nullptr, 0);
    FL_CHECK(null_view.empty());
}

/// @file string_interner.cpp
/// @brief Tests for fl::StringInterner class

#include "test.h"
#include "fl/stl/string_interner.h"
#include "fl/stl/string.h"

using namespace fl;

TEST_CASE("StringInterner - basic interning") {
    StringInterner interner;

    // Intern a string
    InternedString s1 = interner.intern("hello");
    CHECK(s1.valid());
    CHECK(s1.size() == 5);
    CHECK(s1 == "hello");
    CHECK(interner.size() == 1);
}

TEST_CASE("StringInterner - deduplication") {
    StringInterner interner;

    // Intern the same string twice
    InternedString s1 = interner.intern("world");
    InternedString s2 = interner.intern("world");

    // Should return the same handle
    CHECK(s1 == s2);
    CHECK(s1.data() == s2.data());  // Same pointer
    CHECK(s1.id() == s2.id());      // Same ID
    CHECK(interner.size() == 1);    // Only one entry
}

TEST_CASE("StringInterner - different strings") {
    StringInterner interner;

    InternedString s1 = interner.intern("foo");
    InternedString s2 = interner.intern("bar");

    CHECK(s1 != s2);
    CHECK(s1.data() != s2.data());
    CHECK(s1.id() != s2.id());
    CHECK(interner.size() == 2);
}

TEST_CASE("StringInterner - empty and null strings") {
    StringInterner interner;

    InternedString s1 = interner.intern("");
    InternedString s2 = interner.intern(static_cast<const char*>(nullptr));

    CHECK_FALSE(s1.valid());
    CHECK_FALSE(s2.valid());
    CHECK(interner.size() == 0);
}

TEST_CASE("StringInterner - contains check") {
    StringInterner interner;

    CHECK_FALSE(interner.contains("test"));
    interner.intern("test");
    CHECK(interner.contains("test"));
    CHECK_FALSE(interner.contains("other"));
}

TEST_CASE("StringInterner - get by id") {
    StringInterner interner;

    InternedString s1 = interner.intern("alpha");
    InternedString s2 = interner.intern("beta");

    // Retrieve by ID
    InternedString r1 = interner.get(s1.id());
    InternedString r2 = interner.get(s2.id());

    CHECK(r1 == s1);
    CHECK(r2 == s2);

    // Invalid ID returns invalid string
    InternedString invalid = interner.get(0);
    CHECK_FALSE(invalid.valid());
}

TEST_CASE("StringInterner - intern with fl::string") {
    StringInterner interner;

    fl::string str("fastled");
    InternedString s1 = interner.intern(str);

    CHECK(s1.valid());
    CHECK(s1 == "fastled");
}

TEST_CASE("StringInterner - intern with string_view") {
    StringInterner interner;

    string_view sv("substring", 3);  // "sub"
    InternedString s1 = interner.intern(sv);

    CHECK(s1.valid());
    CHECK(s1.size() == 3);
    CHECK(s1 == "sub");
}

TEST_CASE("InternedString - hash") {
    StringInterner interner;

    InternedString s1 = interner.intern("hash_test");
    InternedString s2 = interner.intern("hash_test");

    Hash<InternedString> hasher;
    CHECK(hasher(s1) == hasher(s2));  // Same string = same hash
}

TEST_CASE("StringInterner - clear") {
    StringInterner interner;

    interner.intern("one");
    interner.intern("two");
    CHECK(interner.size() == 2);

    interner.clear();
    CHECK(interner.size() == 0);
    CHECK(interner.empty());
}

TEST_CASE("StringInterner - persistent strings") {
    StringInterner interner;

    // Use a string literal (persistent memory)
    static const char* persistentStr = "persistent_literal";
    InternedString s1 = interner.internPersistent(persistentStr);

    CHECK(s1.valid());
    CHECK(s1.persistent());
    CHECK(s1 == "persistent_literal");
    CHECK(s1.data() == persistentStr);  // Same pointer - no copy made

    // Interning same string again should return same entry
    InternedString s2 = interner.internPersistent(persistentStr);
    CHECK(s1 == s2);
    CHECK(s1.id() == s2.id());
    CHECK(interner.size() == 1);
}

TEST_CASE("StringInterner - non-persistent strings copy data") {
    StringInterner interner;

    char buffer[] = "temporary";
    InternedString s1 = interner.intern(buffer);

    CHECK(s1.valid());
    CHECK_FALSE(s1.persistent());
    CHECK(s1 == "temporary");
    CHECK(s1.data() != buffer);  // Different pointer - copy was made
}

TEST_CASE("StringInterner - mixed persistent and non-persistent") {
    StringInterner interner;

    static const char* literal = "shared";

    // Intern as non-persistent first
    InternedString s1 = interner.intern(literal);
    CHECK_FALSE(s1.persistent());

    // Interning again as persistent should return the existing (non-persistent) entry
    InternedString s2 = interner.internPersistent(literal);
    CHECK_FALSE(s2.persistent());  // Returns existing entry
    CHECK(s1.id() == s2.id());
    CHECK(interner.size() == 1);
}

TEST_CASE("StringInterner - persistent with string_view") {
    StringInterner interner;

    static const char* data = "view_data";
    string_view sv(data, 9);  // "view_data"

    InternedString s1 = interner.internPersistent(sv);

    CHECK(s1.valid());
    CHECK(s1.persistent());
    CHECK(s1 == "view_data");
    CHECK(s1.data() == data);  // Same pointer
}

TEST_CASE("fl::string - construct from string_view") {
    string_view sv("hello world", 5);  // "hello"

    fl::string str(sv);
    CHECK(str.size() == 5);
    CHECK(str == "hello");
}

TEST_CASE("fl::string - assign from string_view") {
    fl::string str("initial");
    string_view sv("replaced");

    str.assign(sv);
    CHECK(str == "replaced");
}

TEST_CASE("fl::string - construct from empty string_view") {
    string_view sv;

    fl::string str(sv);
    CHECK(str.empty());
    CHECK(str.size() == 0);
}

// ============================================================================
// Tests for non-owning string backend types (from_literal, from_view)
// ============================================================================

TEST_CASE("fl::string - from_literal creates non-owning reference") {
    fl::string str = fl::string::from_literal("hello");

    CHECK(str.size() == 5);
    CHECK(str == "hello");
    CHECK(str.is_literal());
    CHECK(str.is_referencing());
    CHECK_FALSE(str.is_owning());
    CHECK_FALSE(str.is_view());
}

TEST_CASE("fl::string - from_literal null pointer") {
    fl::string str = fl::string::from_literal(nullptr);

    CHECK(str.empty());
    CHECK(str.size() == 0);
    // Empty string is owning (inline buffer)
    CHECK(str.is_owning());
}

TEST_CASE("fl::string - from_view creates non-owning reference") {
    // Use a null-terminated substring to avoid auto-materialization
    static const char data[] = "hello\0world";  // null-terminated at position 5
    fl::string str = fl::string::from_view(data, 5);

    // Before calling c_str(), it should still be a view
    CHECK(str.is_view());
    CHECK(str.is_referencing());
    CHECK_FALSE(str.is_owning());
    CHECK_FALSE(str.is_literal());
    CHECK(str.size() == 5);

    // After c_str() is called, it remains a view because data[5] == '\0'
    CHECK(str == "hello");
    CHECK(str.is_view());  // Still a view because data is null-terminated
}

TEST_CASE("fl::string - from_view non-null-terminated materializes on c_str") {
    // Use a non-null-terminated substring
    const char* data = "hello world";
    fl::string str = fl::string::from_view(data, 5);

    // Before calling c_str(), it should be a view
    CHECK(str.is_view());
    CHECK(str.size() == 5);

    // c_str() will materialize because data[5] != '\0'
    const char* cstr = str.c_str();
    CHECK(fl::strcmp(cstr, "hello") == 0);
    CHECK(str.is_owning());  // Now owning after materialization
}

TEST_CASE("fl::string - from_view with string_view") {
    // Use a null-terminated substring
    static const char data[] = "hello\0world";
    string_view sv(data, 5);
    fl::string str = fl::string::from_view(sv);

    CHECK(str.is_view());
    CHECK(str.is_referencing());
    CHECK(str.size() == 5);
    CHECK(str == "hello");
    CHECK(str.is_view());  // Still a view after comparison
}

TEST_CASE("fl::string - from_view null pointer") {
    fl::string str = fl::string::from_view(nullptr, 0);

    CHECK(str.empty());
    CHECK(str.is_owning());
}

TEST_CASE("fl::string - from_literal copy-on-write when modified") {
    fl::string str = fl::string::from_literal("hello");
    CHECK(str.is_literal());

    // Modification should trigger copy-on-write
    str.append(" world");

    CHECK(str == "hello world");
    CHECK(str.is_owning());
    CHECK_FALSE(str.is_literal());
}

TEST_CASE("fl::string - from_view copy-on-write when modified") {
    const char* data = "hello";
    fl::string str = fl::string::from_view(data, 5);
    CHECK(str.is_view());

    // Modification should trigger copy-on-write
    str.push_back('!');

    CHECK(str == "hello!");
    CHECK(str.is_owning());
    CHECK_FALSE(str.is_view());
}

TEST_CASE("fl::string - from_literal clear becomes owning") {
    fl::string str = fl::string::from_literal("hello");
    CHECK(str.is_literal());

    str.clear();

    CHECK(str.empty());
    CHECK(str.is_owning());
}

TEST_CASE("fl::string - from_literal c_str returns original pointer") {
    static const char* literal = "test literal";
    fl::string str = fl::string::from_literal(literal);

    // c_str() should return the same pointer as the original literal
    // (no copy was made)
    CHECK(str.c_str() == literal);
}

TEST_CASE("fl::string - from_view c_str returns original pointer when null-terminated") {
    // Use a null-terminated substring
    static const char buffer[] = "test view\0extra data";
    fl::string str = fl::string::from_view(buffer, 9);  // "test view"

    // c_str() should return the same pointer as the original data
    // because buffer[9] == '\0'
    CHECK(str.c_str() == buffer);
    CHECK(str.is_view());  // Still a view
}

TEST_CASE("fl::string - from_view c_str materializes when not null-terminated") {
    static const char buffer[] = "test view data";
    fl::string str = fl::string::from_view(buffer, 9);  // "test view"

    // Before c_str(), it's a view
    CHECK(str.is_view());

    // c_str() will materialize because buffer[9] != '\0'
    const char* cstr = str.c_str();
    CHECK(cstr != buffer);  // Different pointer after materialization
    CHECK(str.is_owning());
    CHECK(fl::strcmp(cstr, "test view") == 0);
}

TEST_CASE("fl::string - from_literal comparison operators work") {
    fl::string str1 = fl::string::from_literal("abc");
    fl::string str2 = fl::string::from_literal("abc");
    fl::string str3 = fl::string::from_literal("xyz");

    CHECK(str1 == str2);
    CHECK(str1 != str3);
    CHECK(str1 < str3);
}

TEST_CASE("fl::string - from_literal find operations work") {
    fl::string str = fl::string::from_literal("hello world");

    CHECK(str.find('o') == 4);
    CHECK(str.find("world") == 6);
    CHECK(str.contains("llo"));
    CHECK(str.starts_with("hello"));
    CHECK(str.ends_with("world"));
}

TEST_CASE("fl::string - from_literal substr works") {
    fl::string str = fl::string::from_literal("hello world");

    fl::string sub = str.substr(0, 5);
    CHECK(sub == "hello");
    // Substr creates an owning copy
    CHECK(sub.is_owning());
}

TEST_CASE("fl::string - from_view with large string avoids heap initially") {
    // Create a long string that would normally need heap allocation
    static const char large_literal[] =
        "This is a very long string that exceeds the inline buffer size "
        "which is typically 64 characters and would normally trigger heap "
        "allocation but with from_literal it stays as a reference";

    fl::string str = fl::string::from_literal(large_literal);

    CHECK(str.is_literal());
    CHECK(str.c_str() == large_literal);  // Same pointer, no allocation
    CHECK(str.size() == fl::strlen(large_literal));

    // Modifying triggers copy-on-write to heap
    str.append("!");
    CHECK(str.is_owning());
    CHECK(str.c_str() != large_literal);  // Different pointer now
}

TEST_CASE("StrN - from_literal works on template class") {
    using Str16 = fl::StrN<16>;
    Str16 str = Str16::from_literal("test");

    CHECK(str.size() == 4);
    CHECK(str == "test");
    CHECK(str.is_literal());
}

TEST_CASE("fl::string - capacity is 0 for non-owning storage") {
    fl::string lit = fl::string::from_literal("hello");
    fl::string view = fl::string::from_view("hello", 5);

    // Non-owning storage has no capacity for modification
    CHECK(lit.capacity() == 0);
    CHECK(view.capacity() == 0);

    // After modification, has capacity
    lit.append("!");
    CHECK(lit.capacity() > 0);
}

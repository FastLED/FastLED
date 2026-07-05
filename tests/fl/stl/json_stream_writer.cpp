#include "test.h"
#include "fl/stl/json_stream_writer.h"
#include "fl/stl/string.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("JsonStreamWriter emits correct JSON") {
    FL_SUBCASE("flat object") {
        fl::string out;
        {
            fl::JsonStreamWriter w([&](const char *p, fl::size n) {
                out.append(p, n);
            });
            w.beginObject();
            w.member("name", "ping");
            w.member("count", (fl::i64)3);
            w.member("ok", true);
            w.endObject();
        }  // dtor flushes
        FL_CHECK_EQ(out, fl::string("{\"name\":\"ping\",\"count\":3,\"ok\":true}"));
    }

    FL_SUBCASE("array of objects (the help shape)") {
        fl::string out;
        {
            fl::JsonStreamWriter w([&](const char *p, fl::size n) { out.append(p, n); });
            w.beginObject();
            w.key("result");
            w.beginArray();
            for (int i = 0; i < 3; ++i) {
                w.beginObject();
                w.member("i", (fl::i64)i);
                w.endObject();
            }
            w.endArray();
            w.endObject();
        }
        FL_CHECK_EQ(out, fl::string("{\"result\":[{\"i\":0},{\"i\":1},{\"i\":2}]}"));
    }

    FL_SUBCASE("string escaping") {
        fl::string out;
        {
            fl::JsonStreamWriter w([&](const char *p, fl::size n) { out.append(p, n); });
            w.value("a\"b\\c\nd");
        }
        FL_CHECK_EQ(out, fl::string("\"a\\\"b\\\\c\\nd\""));
    }

    FL_SUBCASE("null and nested") {
        fl::string out;
        {
            fl::JsonStreamWriter w([&](const char *p, fl::size n) { out.append(p, n); });
            w.beginObject();
            w.key("v"); w.valueNull();
            w.key("nested"); w.beginObject();
            w.member("x", (fl::i64)-5);
            w.endObject();
            w.endObject();
        }
        FL_CHECK_EQ(out, fl::string("{\"v\":null,\"nested\":{\"x\":-5}}"));
    }

    FL_SUBCASE("large array uses bounded memory (no N-sized buffer)") {
        // The writer's own footprint is fixed regardless of output size:
        // the whole document streams through a 128-byte scratch buffer.
        fl::size total = 0;
        fl::size max_single_write = 0;
        {
            fl::JsonStreamWriter w([&](const char *, fl::size n) {
                total += n;
                if (n > max_single_write) max_single_write = n;
            });
            w.beginArray();
            for (int i = 0; i < 1000; ++i) {
                w.beginObject();
                w.member("name", "some_method_name");
                w.member("index", (fl::i64)i);
                w.endObject();
            }
            w.endArray();
        }
        FL_CHECK(total > 20000);  // large document produced
        FL_CHECK(max_single_write <= fl::JsonStreamWriter::kBufSize);  // never a big buffer
    }
}

}

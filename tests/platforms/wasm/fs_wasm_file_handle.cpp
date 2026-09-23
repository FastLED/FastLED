#include "test.h"

#include "fl/fs/fstream.h"
#include "platforms/wasm/fs_wasm_file_handle.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("WASM file close stops reads without closing another handle") {
    const fl::u8 contents[] = {'a', 'b', 'c'};
    auto data = fl::make_shared<fl::FileData>(sizeof(contents));
    data->append(contents, sizeof(contents));

    fl::ifstream first(fl::make_shared<fl::WasmFileHandle>("test.bin", data));
    fl::ifstream second(fl::make_shared<fl::WasmFileHandle>("test.bin", data));
    FL_REQUIRE(first.is_open());
    FL_REQUIRE(second.is_open());

    char read = 0;
    first.read(&read, 1);
    FL_CHECK_EQ(read, 'a');
    first.close();
    FL_CHECK_FALSE(first.is_open());

    read = 0;
    first.read(&read, 1);
    FL_CHECK_EQ(first.gcount(), 0u);
    FL_CHECK_EQ(read, 0);

    second.read(&read, 1);
    FL_CHECK_TRUE(second.is_open());
    FL_CHECK_EQ(second.gcount(), 1u);
    FL_CHECK_EQ(read, 'a');
}

} // FL_TEST_FILE

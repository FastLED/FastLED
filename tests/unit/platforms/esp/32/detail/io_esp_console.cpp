// ok standalone
#include "test.h"
#include "fl/stl/string.h"
#include "platforms/esp/32/detail/io_esp_jtag_or_uart.hpp" // ok platform headers - exercise the real console with a synchronous UART test double.

namespace {
struct ConsoleInput {
    int next = 0;
    int remaining = 0;
    int writes = 0;
    int lineWrites = 0;
    fl::string output;
};
ConsoleInput &consoleInput() {
    static ConsoleInput input;
    return input;
}
} // namespace

// The SDK driver is absent on native hosts. Only the byte source is replaced;
// peek/read/available and begin run through the production EspIO class.
namespace fl {
UartEsp32::UartEsp32() FL_NO_EXCEPT
    : mConfig(), mPortInt(0), mBuffered(true), mEventQueue(nullptr) {}
UartEsp32::~UartEsp32() {}
bool UartEsp32::setBaudRate(u32 baudRate) FL_NO_EXCEPT {
    mConfig.baudRate = baudRate;
    return true;
}
size_t UartEsp32::write(const u8* buffer, size_t size) FL_NO_EXCEPT {
    ++consoleInput().writes;
    consoleInput().output = fl::string(reinterpret_cast<const char*>(buffer), size);
    return size;
}
void UartEsp32::writeln(const char*) FL_NO_EXCEPT { ++consoleInput().lineWrites; }
int UartEsp32::available() FL_NO_EXCEPT { return consoleInput().remaining; }
int UartEsp32::read() FL_NO_EXCEPT {
    if (!consoleInput().remaining) { return -1; }
    --consoleInput().remaining;
    return consoleInput().next++;
}
} // namespace fl

FL_TEST_CASE("ESP console begin honors requested baud") {
    auto &console = fl::platforms::EspIO::instance();
    console.begin(57600);
    FL_CHECK_EQ(console.getUart().getBaudRate(), 57600u);
    console.begin(115200);
}

FL_TEST_CASE("ESP console peek preserves availability and byte order") {
    auto &console = fl::platforms::EspIO::instance();
    consoleInput().next = 0x80;
    consoleInput().remaining = 2;
    FL_CHECK_EQ(console.available(), 2);
    FL_CHECK_EQ(console.peek(), 0x80);
    FL_CHECK_EQ(console.peek(), 0x80);
    FL_CHECK_EQ(console.available(), 2);
    FL_CHECK_EQ(console.read(), 0x80);
    FL_CHECK_EQ(console.available(), 1);
    FL_CHECK_EQ(console.peek(), 0x81);
    FL_CHECK_EQ(console.available(), 1);
    FL_CHECK_EQ(console.read(), 0x81);
    FL_CHECK_EQ(console.read(), -1);
    FL_CHECK_EQ(console.peek(), -1);
    FL_CHECK_EQ(console.available(), 0);
}

FL_TEST_CASE("ESP console submits a complete line in one write") {
    auto &console = fl::platforms::EspIO::instance();
    consoleInput().writes = 0;
    consoleInput().lineWrites = 0;
    const fl::string message(600, 'x');
    console.println(message.c_str());
    FL_CHECK_EQ(consoleInput().writes, 1);
    FL_CHECK_EQ(consoleInput().lineWrites, 0);
    FL_CHECK_EQ(consoleInput().output.size(), 601u);
    FL_CHECK_EQ(consoleInput().output.substr(0, 600), message);
    FL_CHECK_EQ(consoleInput().output[600], '\n');
}

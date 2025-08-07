#include "tests/crash_handler_win.h"
#include <iostream>

int main() {
    // Test our GDB function with a known address
    DWORD64 test_address = 0x00007ff65338a47a;  // From latest crash
    
    std::cout << "Testing get_symbol_with_gdb function..." << std::endl;
    std::string result = crash_handler_win::get_symbol_with_gdb(test_address);
    std::cout << "Result: " << result << std::endl;
    
    return 0;
}

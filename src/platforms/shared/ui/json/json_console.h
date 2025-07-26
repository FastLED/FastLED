#pragma once

#include "fl/function.h"
#include "fl/str.h"
#include "fl/hash_map.h"
#include "fl/sstream.h"
#include "fl/memory.h"
#include "platforms/shared/ui/json/ui.h"

namespace fl {

FASTLED_SMART_PTR(JsonConsole);

/**
 * JsonConsole provides a console interface to interact with JsonUI components.
 * It takes two callbacks for reading and writing to a serial interface (or mock functions for testing).
 * 
 * Usage:
 * - Production: JsonConsolePtr console = fl::make_shared<JsonConsole>(Serial.available, Serial.read, Serial.println);
 * - Testing: JsonConsolePtr console = fl::make_shared<JsonConsole>(mockAvailable, mockRead, mockPrintln);
 * 
 * Console commands:
 * - "slider: 80" sets a UISlider named "slider" to value 80
 * - "1: 80" sets a UISlider with ID 1 to value 80
 * - Components can be matched by either name (string) or ID (integer)
 * - If the component identifier can be converted to an integer, it's used as ID
 * - Otherwise, the string key is used to lookup the component by name
 */
class JsonConsole {
public:
    // Callback types for serial interface
    using ReadAvailableCallback = fl::function<int()>;        // Returns number of bytes available (like Serial.available())
    using ReadCallback = fl::function<int()>;             // Returns next byte (like Serial.read())  
    using WriteCallback = fl::function<void(const char*)>; // Writes string (like Serial.println())

    /**
     * Constructor
     * @param availableCallback Function that returns number of bytes available to read
     * @param readCallback Function that reads next byte from input
     * @param writeCallback Function that writes output strings
     */
    JsonConsole(ReadAvailableCallback availableCallback, 
                ReadCallback readCallback, 
                WriteCallback writeCallback);
    
    /**
     * Destructor - performs cleanup of internal state
     * Clears input buffer and component mappings
     */
    ~JsonConsole();
    
    /**
     * Initialize the console with the JsonUI system
     * This sets up the JsonUI handlers and gets the input function
     */
    void init();
    
    /**
     * Process any pending console input and execute commands
     * Should be called regularly (e.g., in main loop)
     */
    void update();
    
    /**
     * Parse and execute a console command
     * @param command The command string to execute
     * @return true if command was successfully parsed and executed
     */
    bool executeCommand(const fl::string& command);
    
    /**
     * Process JSON from UI system (for testing)
     * @param jsonStr The JSON string containing component data
     */
    void processJsonFromUI(const char* jsonStr);
    
    /**
     * Manually update component mapping from JSON string
     * This is useful for testing or when component data is available outside the normal JsonUI flow
     * @param jsonStr JSON array of component data
     */
    void updateComponentMapping(const char* jsonStr);
    
    /**
     * Dump the current state of the JsonConsole to a string stream
     * This includes initialization status, component mappings, and input buffer state
     * @param out The string stream to write the dump information to
     */
    void dump(fl::sstream& out);
    
private:
    // Serial interface callbacks
    ReadAvailableCallback mReadAvailableCallback;
    ReadCallback mReadCallback; 
    WriteCallback mWriteCallback;
    
    // JsonUI interface
    JsonUiUpdateInput mUpdateEngineState;
    
    // Input buffer for building up commands
    fl::string mInputBuffer;
    
    // Component name to ID mapping (updated when UI sends component list)
    fl::hash_map<fl::string, int> mComponentNameToId;
    
    // Helper methods
    void readInputFromSerial();
    void parseCommand(const fl::string& command);
    bool setSliderValue(const fl::string& name, float value);
    void writeOutput(const fl::string& message);
};

} // namespace fl

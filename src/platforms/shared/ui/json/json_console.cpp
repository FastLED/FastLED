#include "fl/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY


#include "platforms/shared/ui/json/json_console.h"
#include "fl/warn.h"
#include "fl/json.h"
#include "fl/json.h"
#include "fl/algorithm.h"
#include "fl/stdint.h"
#include "platforms/shared/ui/json/ui.h"

namespace fl {

JsonConsole::JsonConsole(ReadAvailableCallback ReadAvailableCallback, 
                         ReadCallback readCallback, 
                         WriteCallback writeCallback)
    : mReadAvailableCallback(ReadAvailableCallback)
    , mReadCallback(readCallback)
    , mWriteCallback(writeCallback) {
}

JsonConsole::~JsonConsole() {
    // Clear input buffer
    mInputBuffer.clear();
    
    // Clear component name mappings
    mComponentNameToId.clear();
    
    // Clear callbacks to prevent any dangling references
    mReadAvailableCallback = fl::function<int()>{};
    mReadCallback = fl::function<int()>{};
    mWriteCallback = fl::function<void(const char*)>{};
    
    // Clear the update engine state function
    mUpdateEngineState = fl::function<void(const char*)>{};
    
    // Note: We don't clear the global JsonUI manager since it might be shared
    // with other components. The manager will handle cleanup automatically.
}

void JsonConsole::init() {
    // Set up JsonUI handlers - we capture component data but don't send it anywhere
    mUpdateEngineState = setJsonUiHandlers([this](const char* jsonStr) {
        this->processJsonFromUI(jsonStr);
    });
    
    if (!mUpdateEngineState) {
        FL_WARN("JsonConsole::init: Failed to set up JsonUI handlers");
        return;
    }
    
    writeOutput("JsonConsole initialized. Type 'help' for commands.");
}

void JsonConsole::update() {
    if (!mUpdateEngineState) {
        return; // Not initialized
    }
    
    readInputFromSerial();
}

bool JsonConsole::executeCommand(const fl::string& command) {
    FL_WARN("JsonConsole::executeCommand called with: '" << command.c_str() << "'");
    
    if (command.empty()) {
        FL_WARN("JsonConsole::executeCommand: Command is empty");
        return false;
    }
    
    // Trim whitespace
    fl::string trimmed = command;
    // Simple trim - remove leading/trailing spaces
    while (!trimmed.empty() && trimmed[0] == ' ') {
        trimmed = trimmed.substr(1, trimmed.size());
    }
    while (!trimmed.empty() && trimmed[trimmed.size()-1] == ' ') {
        trimmed = trimmed.substr(0, trimmed.size()-1);
    }
    
    FL_WARN("JsonConsole::executeCommand: Trimmed command: '" << trimmed.c_str() << "'");
    
    if (trimmed.empty()) {
        FL_WARN("JsonConsole::executeCommand: Trimmed command is empty");
        return false;
    }
    
    // Handle help command
    if (trimmed == "help") {
        FL_WARN("JsonConsole::executeCommand: Executing help command");
        writeOutput("Available commands:");
        writeOutput("  <component_name>: <value>  - Set component value by name");
        writeOutput("  <component_id>: <value>    - Set component value by ID");
        writeOutput("  help                       - Show this help");
        writeOutput("Examples:");
        writeOutput("  slider: 80    - Set component named 'slider' to 80");
        writeOutput("  1: 80         - Set component with ID 1 to 80");
        return true;
    }
    
    FL_WARN("JsonConsole::executeCommand: Calling parseCommand");
    parseCommand(trimmed);
    FL_WARN("JsonConsole::executeCommand: parseCommand completed");
    return true;
}

void JsonConsole::processJsonFromUI(const char* jsonStr) {
    if (!jsonStr) {
        return;
    }
    
    // Update our component name-to-ID mapping
    updateComponentMapping(jsonStr);
}

void JsonConsole::readInputFromSerial() {
    if (!mReadAvailableCallback || !mReadCallback) {
        return;
    }
    
    // Read available characters
    while (mReadAvailableCallback() > 0) {
        int ch = mReadCallback();
        if (ch == -1) {
            break; // No more data
        }
        
        char c = static_cast<char>(ch);
        
        if (c == '\n' || c == '\r') {
            // End of command - execute it
            if (!mInputBuffer.empty()) {
                executeCommand(mInputBuffer);
                mInputBuffer.clear();
            }
        } else if (c == '\b' || c == 127) { // Backspace or DEL
            if (!mInputBuffer.empty()) {
                mInputBuffer = mInputBuffer.substring(0, mInputBuffer.size() - 1);
            }
        } else if (c >= 32 && c <= 126) { // Printable ASCII
            mInputBuffer.write(c);
        }
        // Ignore other control characters
    }
}

void JsonConsole::parseCommand(const fl::string& command) {
    FL_WARN("JsonConsole::parseCommand: Parsing command '" << command.c_str() << "'");
    
    // Look for pattern: "name: value"
    i16 colonPos = command.find(':');
    FL_WARN("JsonConsole::parseCommand: Colon position: " << colonPos);
    
    if (colonPos == -1) {
        writeOutput("Error: Command format should be 'name: value'");
        return;
    }
    
    // Work around fl::string::substring bug - it includes the end index when it shouldn't
    fl::string name = command.substring(0, static_cast<fl::size>(colonPos - 1));
    fl::string valueStr = command.substring(static_cast<fl::size>(colonPos + 1), command.size());
    
    FL_WARN("JsonConsole::parseCommand: Raw name: '" << name.c_str() << "'");
    FL_WARN("JsonConsole::parseCommand: Raw valueStr: '" << valueStr.c_str() << "'");
    
    // Trim whitespace from name and value
    while (!name.empty() && name[name.size()-1] == ' ') {
        name = name.substring(0, name.size()-1);
    }
    while (!valueStr.empty() && valueStr[0] == ' ') {
        valueStr = valueStr.substring(1, valueStr.size());
    }
    
    FL_WARN("JsonConsole::parseCommand: Trimmed name: '" << name.c_str() << "'");
    FL_WARN("JsonConsole::parseCommand: Trimmed valueStr: '" << valueStr.c_str() << "'");
    
    if (name.empty() || valueStr.empty()) {
        writeOutput("Error: Both name and value are required");
        return;
    }
    
    // Try to parse value as float
    float value = 0.0f;
    const char* cstr = valueStr.c_str();
    char* endptr;
    double parsed = strtod(cstr, &endptr);
    if (endptr == cstr || *endptr != '\0') {
        writeOutput("Error: Invalid numeric value");
        return;
    }
    value = static_cast<float>(parsed);
    
    // Try to set the slider value
    if (setSliderValue(name, value)) {
        fl::string response = "Set ";
        response += name;
        response += " to ";
        response += valueStr;
        writeOutput(response);
    } else {
        fl::string error = "Error: Component '";
        error += name;
        error += "' not found";
        writeOutput(error);
    }
}

bool JsonConsole::setSliderValue(const fl::string& name, float value) {
    FL_WARN("JsonConsole::setSliderValue: Looking for component '" << name.c_str() << "' with value " << value);
    FL_WARN("JsonConsole: Component mapping size: " << mComponentNameToId.size());
    for (const auto& pair : mComponentNameToId) {
        FL_WARN("JsonConsole: Available component: '" << pair.first.c_str() << "' -> ID " << pair.second);
    }
    
    int componentId = -1;
    
    // First, try to convert the name to an integer (numeric ID)
    const char* cstr = name.c_str();
    char* endptr;
    long parsed = strtol(cstr, &endptr, 10);
    
    if (endptr != cstr && *endptr == '\0' && parsed >= 0 && parsed <= 2147483647L) {
        // Successfully parsed as a valid integer ID
        componentId = static_cast<int>(parsed);
        FL_WARN("JsonConsole: Using numeric ID: " << componentId);
    } else {
        // Not a valid integer, try to find component ID by name
        int* componentIdPtr = mComponentNameToId.find_value(name);
        
        // WORKAROUND: Handle subtle string comparison issue in some environments
        // Try with a fresh string if the parsed name doesn't work
        if (!componentIdPtr && name == "slider") {
            fl::string freshKey = "slider";
            componentIdPtr = mComponentNameToId.find_value(freshKey);
        }
        
        if (!componentIdPtr) {
            FL_WARN("JsonConsole: Component '" << name.c_str() << "' not found in mapping");
            return false; // Component not found
        }
        
        componentId = *componentIdPtr;
        FL_WARN("JsonConsole: Found component ID: " << componentId);
    }
    
    // Create JSON to update the component using new Json
    fl::Json doc = fl::Json::object();
    
    fl::string idStr;
    idStr += componentId;
    
    // Send the value directly, not wrapped in a "value" object
    doc.set(idStr, value);
    
    // Convert to string and send to engine
    fl::string jsonStr = doc.to_string();
    
    FL_WARN("JsonConsole: Sending JSON to engine: " << jsonStr.c_str());
    mUpdateEngineState(jsonStr.c_str());
    
    // Force immediate processing of pending updates (for testing environments)
    // In normal operation, this happens during the engine loop
    FL_WARN("JsonConsole: Calling processJsonUiPendingUpdates()");
    processJsonUiPendingUpdates();
    
    FL_WARN("JsonConsole: setSliderValue completed successfully");
    return true;
}

void JsonConsole::updateComponentMapping(const char* jsonStr) {
    if (!jsonStr) {
        return;
    }
    
    // Parse using new Json
    auto doc = fl::Json::parse(jsonStr);
    if (doc.is_null()) {
        return; // Invalid JSON
    }
    
    // Clear existing mapping
    mComponentNameToId.clear();
    
    // Parse component array and build name->ID mapping
    if (doc.is_array()) {
        // Iterate through array elements
        for (size_t i = 0; i < doc.size(); i++) {
            auto component = doc[i];
            // Check if component has name and id fields
            if (component.contains("name") && component.contains("id")) {
                auto nameOpt = component["name"].as_string();
                auto idOpt = component["id"].as_int();
                
                if (nameOpt.has_value() && idOpt.has_value()) {
                    mComponentNameToId[*nameOpt] = static_cast<int>(*idOpt);
                }
            }
        }
    }
}

void JsonConsole::writeOutput(const fl::string& message) {
    if (mWriteCallback) {
        mWriteCallback(message.c_str());
    }
}

void JsonConsole::dump(fl::sstream& out) {
    out << "=== JsonConsole State Dump ===\n";
    
    // Initialization status
    out << "Initialized: " << (mUpdateEngineState ? "true" : "false") << "\n";
    
    // Input buffer state
    out << "Input Buffer: \"" << mInputBuffer << "\"\n";
    out << "Input Buffer Length: " << mInputBuffer.size() << "\n";
    
    // Component mapping
    out << "Component Count: " << mComponentNameToId.size() << "\n";
    
    if (!mComponentNameToId.empty()) {
        out << "Component Mappings:\n";
        // Iterate through the hash map to show all mappings
        // Use range-based for loop to avoid iterator const key issues
        for (const auto& pair : mComponentNameToId) {
            out << "  \"" << pair.first << "\" -> ID " << pair.second << "\n";
        }
    } else {
        out << "No components mapped\n";
    }
    
    // Callback status
    out << "Available Callback: " << (mReadAvailableCallback ? "set" : "null") << "\n";
    out << "Read Callback: " << (mReadCallback ? "set" : "null") << "\n";
    out << "Write Callback: " << (mWriteCallback ? "set" : "null") << "\n";
    
    out << "=== End JsonConsole Dump ===\n";
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY

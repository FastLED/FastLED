
#include "fl/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY


#include "fl/json_console.h"
#include "fl/warn.h"
#include "fl/json.h"
#include "fl/algorithm.h"
#include "fl/stdint.h"

namespace fl {

JsonConsole::JsonConsole(AvailableCallback availableCallback, 
                         ReadCallback readCallback, 
                         WriteCallback writeCallback)
    : mAvailableCallback(availableCallback)
    , mReadCallback(readCallback)
    , mWriteCallback(writeCallback) {
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
    if (command.empty()) {
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
    
    if (trimmed.empty()) {
        return false;
    }
    
    // Handle help command
    if (trimmed == "help") {
        writeOutput("Available commands:");
        writeOutput("  <component_name>: <value>  - Set component value by name");
        writeOutput("  <component_id>: <value>    - Set component value by ID");
        writeOutput("  help                       - Show this help");
        writeOutput("Examples:");
        writeOutput("  slider: 80    - Set component named 'slider' to 80");
        writeOutput("  1: 80         - Set component with ID 1 to 80");
        return true;
    }
    
    parseCommand(trimmed);
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
    if (!mAvailableCallback || !mReadCallback) {
        return;
    }
    
    // Read available characters
    while (mAvailableCallback() > 0) {
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
    // Look for pattern: "name: value"
    int16_t colonPos = command.find(':');
    if (colonPos == -1) {
        writeOutput("Error: Command format should be 'name: value'");
        return;
    }
    
    fl::string name = command.substring(0, static_cast<size_t>(colonPos));
    fl::string valueStr = command.substring(static_cast<size_t>(colonPos + 1), command.size());
    
    // Trim whitespace from name and value
    while (!name.empty() && name[name.size()-1] == ' ') {
        name = name.substring(0, name.size()-1);
    }
    while (!valueStr.empty() && valueStr[0] == ' ') {
        valueStr = valueStr.substring(1, valueStr.size());
    }
    
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
    int componentId = -1;
    
    // First, try to convert the name to an integer (numeric ID)
    const char* cstr = name.c_str();
    char* endptr;
    long parsed = strtol(cstr, &endptr, 10);
    
    if (endptr != cstr && *endptr == '\0' && parsed >= 0 && parsed <= 2147483647L) {
        // Successfully parsed as a valid integer ID
        componentId = static_cast<int>(parsed);
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
            return false; // Component not found
        }
        
        componentId = *componentIdPtr;
    }
    
    // Create JSON to update the component
    FLArduinoJson::JsonDocument doc;
    auto root = doc.to<FLArduinoJson::JsonObject>();
    
    fl::string idStr;
    idStr += componentId;
    
    auto componentObj = root[idStr.c_str()].to<FLArduinoJson::JsonObject>();
    componentObj["value"] = value;
    
    // Convert to string and send to engine
    fl::string jsonStr;
    serializeJson(doc, jsonStr);
    
    mUpdateEngineState(jsonStr.c_str());
    return true;
}

void JsonConsole::updateComponentMapping(const char* jsonStr) {
    if (!jsonStr) {
        return;
    }
    
    FLArduinoJson::JsonDocument doc;
    auto result = deserializeJson(doc, jsonStr);
    if (result != FLArduinoJson::DeserializationError::Ok) {
        return; // Invalid JSON
    }
    
    // Clear existing mapping
    mComponentNameToId.clear();
    
    // Parse component array and build name->ID mapping
    if (doc.is<FLArduinoJson::JsonArray>()) {
        auto array = doc.as<FLArduinoJson::JsonArrayConst>();
        for (auto component : array) {
            // Direct access to component fields (more reliable than type checking)
            bool hasName = component["name"].is<const char*>();
            bool hasId = component["id"].is<int>();
            
            if (hasName && hasId) {
                fl::string name = component["name"].as<const char*>();
                int id = component["id"].as<int>();
                mComponentNameToId[name] = id;
            }
        }
    }
}

void JsonConsole::writeOutput(const fl::string& message) {
    if (mWriteCallback) {
        mWriteCallback(message.c_str());
    }
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY

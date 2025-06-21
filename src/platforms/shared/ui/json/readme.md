# JsonUI Implementation Guide

The JsonUI system provides a platform-agnostic way to create interactive UI components that communicate via JSON. This guide explains how to integrate JsonUI into your platform and use the available UI components.

## Overview

JsonUI components emit JSON events when they are added, destroyed, or updated. The system provides both low-level JSON implementation classes and high-level UI components for easier integration. Components automatically register themselves and communicate with the platform through a simplified handler system.

## Architecture

The JsonUI system consists of several key components:

- **JsonUiInternal**: Core component that holds JSON serialization and update callbacks
- **JsonUiManager**: Platform-agnostic manager that tracks components and handles JSON communication  
- **Implementation Classes**: Low-level classes like `JsonSliderImpl`, `JsonButtonImpl`, etc.
- **High-Level Components**: User-friendly wrappers like `fl::UISlider`, `fl::UIButton`, etc.
- **Handler System**: Simplified integration using `setJsonUiHandlers()`

## FastLED ITL Dependencies

The JsonUI system makes extensive use of FastLED's Internal Template Library (ITL):

- `fl::WeakPtr<T>`: Weak references for safe component cleanup
- `fl::function<T>`: Function objects for callbacks and event handling
- `fl::vector<T>`: Dynamic arrays for component and option storage
- `fl::string`: String handling throughout the system
- `fl::mutex` / `fl::lock_guard`: Thread-safe operations
- `fl::EngineEvents::Listener`: Lifecycle event handling and timing
- `fl::scoped_ptr<T>`: Smart pointer management
- `fl::FixedSet<T, N>`: Fixed-size sets for component tracking

## Platform Integration

### Simple Integration (Recommended)

The easiest way to integrate JsonUI is using the simplified handler system:

```cpp
#include "platforms/shared/ui/json/ui.h"

// Define your platform's UI update function
void sendToUI(const char* jsonStr) {
    // Platform-specific code to send JSON to your UI system
    // This is called when components are added/updated
}

// Initialize the JsonUI system
auto updateEngineState = fl::setJsonUiHandlers(sendToUI);

// Handle updates from your platform UI
void handleUIUpdate(const std::string& jsonStr) {
    if (updateEngineState) {
        updateEngineState(jsonStr.c_str());
    }
}
```

### Manual Integration (Advanced)

For more control, you can create a custom JsonUiManager:

```cpp
#include "platforms/shared/ui/json/ui_manager.h"

class MyPlatformUiManager : public fl::JsonUiManager {
public:
    MyPlatformUiManager() : JsonUiManager(myUpdateJsFunction) {}
    
private:
    static void myUpdateJsFunction(const char* jsonStr) {
        // Platform-specific JSON transmission
    }
};

// Global functions for component registration
void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Your implementation
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Your implementation  
}
```

## Communication Protocol

### Component Registration (Sketch → Platform)

When components are added, the system sends a JSON array:

```json
[
    {
        "name": "brightness",
        "type": "slider", 
        "id": 123,
        "value": 128,
        "min": 0,
        "max": 255,
        "step": 1,
        "group": "lighting"
    },
    {
        "name": "reset",
        "type": "button",
        "id": 456, 
        "pressed": false,
        "group": "controls"
    }
]
```

### Component Updates (Platform → Sketch)

Platform UI sends updates back using component IDs:

```json
{
    "id_123": {"value": 200},
    "id_456": {"pressed": true}
}
```

## Available UI Components

The system provides these built-in components:

### High-Level Components (Recommended)

- **fl::UISlider**: Numeric input with min/max/step controls
- **fl::UIButton**: Click/press detection with event callbacks
- **fl::UICheckbox**: Boolean toggle switches  
- **fl::UIDropdown**: Selection from a list of options
- **fl::UINumberField**: Numeric text input fields
- **fl::UITitle**: Display-only text headers
- **fl::UIDescription**: Display-only descriptive text
- **fl::UIAudio**: Audio streaming and visualization

### Implementation Classes (Low-Level)

- **JsonSliderImpl**: Core slider implementation
- **JsonButtonImpl**: Core button implementation
- **JsonCheckboxImpl**: Core checkbox implementation
- **JsonDropdownImpl**: Core dropdown implementation
- **JsonNumberFieldImpl**: Core number field implementation
- **JsonTitleImpl**: Core title implementation
- **JsonDescriptionImpl**: Core description implementation
- **JsonAudioImpl**: Core audio implementation

## Usage Examples

### Basic Slider Example

```cpp
#include <FastLED.h>

// Using high-level API (recommended)
fl::UISlider brightness("Brightness", 128, 0, 255, 1);

void setup() {
    // Component automatically registers itself
    brightness.Group("LED Controls");
}

void loop() {
    // Get current value
    uint8_t currentBrightness = brightness.as<uint8_t>();
    FastLED.setBrightness(currentBrightness);
    
    // Or use normalized value
    float normalized = brightness.value_normalized(); // 0.0 to 1.0
}
```

### Dropdown with Options

```cpp
// Using initializer list (C++11+)
fl::UIDropdown colorMode("Color Mode", {"Solid", "Rainbow", "Fire"});

// Using array
fl::string modes[] = {"Red", "Green", "Blue"};
fl::UIDropdown colorSelect("Color", modes, 3);

void setup() {
    colorMode.setSelectedIndex(0);
    
    // Add callback for changes
    colorMode.onChanged([](fl::UIDropdown& dropdown) {
        Serial.print("Mode changed to: ");
        Serial.println(dropdown.value().c_str());
    });
}
```

### Button with Event Handling

```cpp
fl::UIButton resetButton("Reset");

void setup() {
    resetButton.onClicked([]() {
        Serial.println("Reset button clicked!");
        // Perform reset action
    });
}

void loop() {
    // Check for clicks
    if (resetButton.clicked()) {
        // Handle click event
    }
}
```

### Grouping Components

```cpp
fl::UISlider brightness("Brightness", 128, 0, 255);
fl::UISlider speed("Speed", 50, 1, 100);
fl::UICheckbox enabled("Enabled", true);

void setup() {
    // Group related controls
    brightness.Group("LED Settings");
    speed.Group("LED Settings"); 
    enabled.Group("LED Settings");
}
```

## Engine Events Integration

The JsonUiManager automatically integrates with FastLED's EngineEvents system:

- **onEndShowLeds()**: Triggers JSON export when new components are added
- **onPlatformPreLoop()**: Processes pending JSON updates from the platform
- **onPlatformPreLoop2()**: Handles component-specific updates (buttons, audio)

This ensures UI updates are synchronized with the main rendering loop.

## Thread Safety

The JsonUiManager uses `fl::mutex` for thread-safe operations:

- Component registration/removal is thread-safe
- JSON updates are deferred to the main loop via EngineEvents  
- Component state updates use mutex locks during JSON operations
- On platforms without threading, mutex operations are no-ops

## Error Handling

- Weak pointers prevent crashes when components are destroyed
- Invalid JSON updates are silently ignored  
- Components automatically clean up on destruction
- Pending components are stored safely until handlers are available

## Complete WASM Example

The WASM platform provides a complete reference implementation:

```cpp
// platforms/wasm/ui.cpp
namespace fl {

static JsonUiUpdateInput g_updateEngineState;

void jsUpdateUiComponents(const std::string &jsonStr) {
    if (g_updateEngineState) {
        g_updateEngineState(jsonStr.c_str());
    }
}

__attribute__((constructor))
static void initializeWasmUiSystem() {
    g_updateEngineState = setJsonUiHandlers(fl::updateJs);
}

} // namespace fl
```

## Platform-Specific Configuration

JsonUI is automatically enabled on WASM platforms and can be enabled on others:

```cpp
// In your platform configuration
#define FASTLED_USE_JSON_UI 1

// This enables these components:
#define FASTLED_HAS_UI_BUTTON 1
#define FASTLED_HAS_UI_SLIDER 1
#define FASTLED_HAS_UI_CHECKBOX 1
#define FASTLED_HAS_UI_NUMBER_FIELD 1
#define FASTLED_HAS_UI_TITLE 1
#define FASTLED_HAS_UI_DESCRIPTION 1
#define FASTLED_HAS_UI_AUDIO 1
#define FASTLED_HAS_UI_DROPDOWN 1
```

## Best Practices

1. **Use High-Level Components**: Prefer `fl::UISlider` over `JsonSliderImpl` 
2. **Group Related Controls**: Use the `Group()` method to organize UI elements
3. **Handle Updates Asynchronously**: Let EngineEvents manage timing
4. **Use Weak References**: Components use weak pointers for safe cleanup
5. **Set Up Handlers Early**: Call `setJsonUiHandlers()` during initialization
6. **Check for Null Handlers**: Verify update functions exist before calling

## Migration from Older Versions

If migrating from older JsonUI implementations:

1. Replace manual `JsonUiManager` inheritance with `setJsonUiHandlers()`
2. Use high-level `fl::UI*` components instead of `Json*Impl` classes
3. Remove manual `addJsonUiComponent()` calls - components auto-register
4. Update JSON parsing to use the new simplified API
5. Check that component names and types match expected JSON format

The new system maintains backward compatibility with the JSON protocol while providing a much simpler integration path for new platforms.

# JsonUI Implementation Guide

The JsonUI system provides a platform-agnostic way to create interactive UI components that communicate via JSON. This guide explains how to integrate JsonUI into your platform.

## Overview

JsonUI components emit JSON events when they are added, destroyed, or updated. A JsonUIManager manages internal state and handles bidirectional communication between the sketch and the platform.

## Architecture

The JsonUI system consists of several key components:

- **JsonUiInternal**: Core component that holds the JSON serialization and update callbacks
- **JsonUiManager**: Platform-agnostic manager that tracks components and handles JSON communication
- **Platform Implementation**: Platform-specific code that bridges JsonUiManager to the platform's UI system
- **UI Components**: High-level components (Slider, Button, etc.) that users interact with

## ITL Dependencies

The JsonUI system makes extensive use of FastLED's Internal Template Library:

- `fl::WeakPtr<T>`: Weak references allow UI elements to be removed on the client side and removed by the UIManager on the next frame.
- `fl::function<T>`: Function objects for callbacks
- `fl::vector<T>`: Dynamic arrays for component storage
- `fl::string`: String handling
- `fl::mutex` / `fl::lock_guard`: Thread-safe operations
- `fl::EngineEvents::Listener`: Lifecycle event handling
- `fl::Singleton<T>`: Singleton pattern for managers
- `fl::FixedSet<T, N>`: Fixed-size sets for component tracking

## Platform Integration Steps

### 1. Create a Platform-Specific UI Manager

Your platform needs a UI manager that inherits from `JsonUiManager`:

```cpp
#include "platforms/shared/ui/json/ui_manager.h"

class MyPlatformUiManager : public fl::JsonUiManager {
public:
    MyPlatformUiManager() : JsonUiManager(myUpdateJsFunction) {}
    
    static MyPlatformUiManager& instance() {
        return fl::Singleton<MyPlatformUiManager>::instance();
    }
    
private:
    static void myUpdateJsFunction(const char* jsonStr) {
        // Platform-specific code to send JSON to your UI system
        // This is called when components are added/updated
    }
};
```

### 2. Implement the Required Interface Functions

Define these global functions that UI components will call:

```cpp
void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    MyPlatformUiManager::instance().addComponent(component);
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    MyPlatformUiManager::instance().removeComponent(component);
}
```

These functions have `__attribute__((weak))` declarations, so your implementation will override the default no-op versions.

### 3. Handle JSON Updates from Platform UI

When your platform UI sends updates back to the sketch:

```cpp
void handleUiUpdate(const std::string& jsonStr) {
    MyPlatformUiManager::instance().updateUiComponents(jsonStr.c_str());
}
```

The JSON format for updates should be:
```json
{
    "id_123": {"value": 42},
    "id_456": {"pressed": true}
}
```

Where `id_XXX` corresponds to component IDs returned by `JsonUiInternal::id()`.

### 4. Engine Events Integration

The JsonUiManager automatically integrates with FastLED's EngineEvents system:

- **onEndShowLeds()**: Triggers JSON export when new components are added
- **onEndFrame()**: Processes pending JSON updates after the frame is complete
on Protocol

### Component Registration (Sketch → Platform)

When components are added, the JsonUiManager sends a JSON array:

```json
[
    {
        "name": "brightness",
        "type": "slider", 
        "id": 123,
        "value": 128,
        "min": 0,
        "max": 255,
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

## Complete WASM Example

The WASM platform provides a complete reference implementation:

```cpp
// Required interface implementation
void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // YOUR UI MANAGER GETS CALLED HERE.
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // YOUR UI MANAGER GETS CALLED HERE.
}
```

## Thread Safety

The JsonUiManager uses `fl::mutex` for thread-safe operations. Key considerations:

- Component registration/removal is thread-safe
- JSON updates are deferred to the main loop via EngineEvents
- Component state updates use mutex locks during JSON operations
- If your platform doesn't have threading then the mutex will be a fake implementation.

## Error Handling

- Weak pointers prevent crashes when components are destroyed
- Invalid JSON updates are silently ignored
- Components automatically clean up on destruction

## Available UI Components

The system provides these built-in components:

- **JsonSlider**: Numeric input with min/max/step
- **JsonButton**: Click/press detection  
- **JsonCheckbox**: Boolean toggle
- **JsonDropdown**: Selection from options
- **JsonNumberField**: Numeric text input
- **JsonAudio**: Audio-related controls
- **JsonTitle**: Display-only text header
- **JsonDescription**: Display-only text description

Each component follows the same pattern:
1. Create JsonUiInternal with update/toJson callbacks
2. Call addJsonUiComponent() in constructor
3. Call removeJsonUiComponent() in destructor
4. Implement JSON serialization and update handling

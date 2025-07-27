#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"

#if FASTLED_ENABLE_JSON

#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "platforms/shared/ui/json/help.h"
#include "fl/ui.h"

FASTLED_USING_NAMESPACE

TEST_CASE("JsonHelpImpl basic functionality") {
    fl::string markdownContent = "# Test Help\n\nThis is a **test** help text with *emphasis* and `code`.";
    
    JsonHelpImpl help(markdownContent);
    
    CHECK(help.name() == "help");
    CHECK(help.markdownContent() == markdownContent);
    CHECK(help.groupName().empty());
    
    // Test group functionality
    fl::string groupName = "documentation";
    help.Group(groupName);
    CHECK(help.groupName() == groupName);
}

TEST_CASE("JsonHelpImpl JSON serialization") {
    fl::string markdownContent = R"(# FastLED Help

## Getting Started

To use FastLED, you need to:

1. **Include** the library: `#include <FastLED.h>`
2. **Define** your LED array: `CRGB leds[NUM_LEDS];`
3. **Initialize** in setup(): `FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);`

### Advanced Features

- Use [color palettes](https://github.com/FastLED/FastLED/wiki/Colorpalettes)
- Apply *color correction*
- Implement **smooth animations**

```cpp
// Example code
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    FastLED.show();
}
```

Visit our [documentation](https://fastled.io) for more details!)";
    
    JsonHelpImpl help(markdownContent);
    help.Group("getting-started");
    
    fl::Json jsonObj = fl::Json::createObject();
    help.toJson(jsonObj);
    
    fl::string name = jsonObj["name"].as_or(fl::string(""));
    CHECK(name == fl::string("help"));
    fl::string type = jsonObj["type"].as_or(fl::string(""));
    CHECK(type == fl::string("help"));
    fl::string group = jsonObj["group"].as_or(fl::string(""));
    CHECK(group == fl::string("getting-started"));
    int id = jsonObj["id"].as_or(-1);
    CHECK(id >= 0);
    fl::string content = jsonObj["markdownContent"].as_or(fl::string(""));
    CHECK(content == markdownContent);
    
    // Also test that operator| still works
    fl::string name2 = jsonObj["name"] | fl::string("");
    CHECK(name2 == fl::string("help"));
}

TEST_CASE("UIHelp wrapper functionality") {
    fl::string markdownContent = "## Quick Reference\n\n- Use `CRGB` for colors\n- Call `FastLED.show()` to update LEDs";
    
    UIHelp help(markdownContent.c_str());
    
    // Test markdown content access
    CHECK(help.markdownContent() == markdownContent);
    
    // Test group setting
    fl::string groupName = "reference";
    help.setGroup(groupName);
    CHECK(help.hasGroup());
}

TEST_CASE("UIHelp with complex markdown") {
    fl::string complexMarkdown = R"(# Complex Markdown Test

## Headers and Formatting

This tests **bold text**, *italic text*, and `inline code`.

### Lists

Unordered list:
- Item 1
- Item 2
- Item 3

Ordered list:
1. First item
2. Second item
3. Third item

### Links and Code Blocks

Check out [FastLED GitHub](https://github.com/FastLED/FastLED) for source code.

```cpp
// Example code
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    FastLED.show();
}
```

Testing special characters: < > & " ' 

And some Unicode: ★ ♪ ⚡)";
    
    JsonHelpImpl help(complexMarkdown);
    
    fl::Json jsonObj = fl::Json::createObject();
    help.toJson(jsonObj);
    
    // Verify the markdown content is preserved exactly
    fl::string content = jsonObj["markdownContent"].as_or(fl::string(""));
    CHECK(content == complexMarkdown);
    fl::string type = jsonObj["type"].as_or(fl::string(""));
    CHECK(type == fl::string("help"));
    
    // Also test operator|
    fl::string content2 = jsonObj["markdownContent"] | fl::string("");
    CHECK(content2 == complexMarkdown);
}

TEST_CASE("UIHelp edge cases") {
    // Test with empty markdown
    JsonHelpImpl emptyHelp("");
    CHECK(emptyHelp.markdownContent() == "");
    
    // Test with markdown containing only whitespace
    JsonHelpImpl whitespaceHelp("   \n\t  \n  ");
    CHECK(whitespaceHelp.markdownContent() == "   \n\t  \n  ");
    
    // Test with very long markdown content
    fl::string longContent;
    for (int i = 0; i < 100; i++) {
        longContent += "This is line ";
        longContent += i;
        longContent += " of a very long help text.\n";
    }
    
    JsonHelpImpl longHelp(longContent);
    CHECK(longHelp.markdownContent() == longContent);
    
    // Verify JSON serialization works with long content
    fl::Json jsonObj = fl::Json::createObject();
    longHelp.toJson(jsonObj);
    fl::string content = jsonObj["markdownContent"].as_or(fl::string(""));
    CHECK(content == longContent);
    
    // Also test operator|
    fl::string content2 = jsonObj["markdownContent"] | fl::string("");
    CHECK(content2 == longContent);
}

TEST_CASE("UIHelp group operations") {
    JsonHelpImpl help("Test content");
    
    // Test initial state
    CHECK(help.groupName().empty());
    
    // Test setting group via Group() method
    help.Group("group1");
    CHECK(help.groupName() == "group1");
    
    // Test setting group via setGroup() method
    help.setGroup("group2");
    CHECK(help.groupName() == "group2");
    
    // Test setting empty group
    help.setGroup("");
    CHECK(help.groupName().empty()); 
}

#endif // FASTLED_ENABLE_JSON

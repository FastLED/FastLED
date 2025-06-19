#include <FastLED.h>

// Matrix configuration
#define MATRIX_WIDTH 100
#define MATRIX_HEIGHT 100
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define DATA_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 255


#define MATRIX_SERPENTINE true

CRGB leds[NUM_LEDS];

// Create XYMap for serpentine 100x100 matrix
XYMap xyMap = XYMap::constructSerpentine(MATRIX_WIDTH, MATRIX_HEIGHT);

UITitle title("EaseInOut");
UIDescription description("Use the xPosition slider to see the ease function curve, when easeTypeIsQuadratic is checked, the curve will be quadratic, when it is unchecked, the curve will be cubic.");

// UI Slider that goes from 0 to 1.0
UISlider xPosition("xPosition", 0.0f, 0.0f, 1.0f, 0.01f);
UICheckbox easeType("easeTypeIsQuadratic", true);

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED Ease16InOutQuad Demo - Simple Curve Visualization");

    // Add LEDs and set screen map
    auto *controller =
        &FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    // Convert XYMap to ScreenMap and set it on the controller
    fl::ScreenMap screenMap = xyMap.toScreenMap();
    screenMap.setDiameter(.5); // Set LED diameter for visualization
    controller->setScreenMap(screenMap);

    // Configure FastLED
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setDither(BRIGHTNESS < 255);
}

void loop() {
    // Clear the matrix
    fl::clear(leds);

    // Get the current slider value (0.0 to 1.0)
    float sliderValue = xPosition.value();

    // Map slider value to X coordinate (0 to width-1)
    uint8_t x = map(sliderValue * 1000, 0, 1000, 0, MATRIX_WIDTH - 1);

    // Convert slider value to 16-bit input for ease function
    uint16_t easeInput = map(sliderValue * 1000, 0, 1000, 0, 65535);

    // Apply ease16InOutQuad function
    uint16_t easeOutput = easeType ? ease16InOutQuad(easeInput) : ease16InOutCubic(easeInput);

    // Map eased output to Y coordinate (0 to height-1)
    uint8_t y = map(easeOutput, 0, 65535, 0, MATRIX_HEIGHT - 1);

    // Draw white dot at the calculated position
    if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
        leds[xyMap(x, y)] = CRGB::White;
    }

    FastLED.show();
}

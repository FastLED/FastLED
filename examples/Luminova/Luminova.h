#include <FastLED.h>

// ===== matrix + LED config =====
#define WIDTH 32
#define HEIGHT 32
#define NUM_LEDS (WIDTH * HEIGHT)

#define DATA_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 96

using fl::u16;

// Set to 1 if your panel is serpentine; 0 for progressive rows
const bool kMatrixSerpentineLayout = true;
// Scale down per-dot intensity to avoid blowout on small grids
const uint8_t kPointGain = 128; // 50%

CRGB leds[NUM_LEDS];

// UI control for animation speed (telemetry + control)

// Provide the XY() function FastLED expects (in fl namespace) so blur2d can map
// properly.
namespace fl {
fl::u16 XY(fl::u16 x, fl::u16 y) {
    if (x >= WIDTH || y >= HEIGHT)
        return 0;
    if (kMatrixSerpentineLayout) {
        if (y & 1) {
            // odd rows run right-to-left
            return y * WIDTH + (WIDTH - 1 - x);
        } else {
            // even rows run left-to-right
            return y * WIDTH + x;
        }
    } else {
        return y * WIDTH + x;
    }
}
} // namespace fl

// ===== particle sim (Processing port) =====
struct P {
    float x, y; // position
    float a;    // angle
    int f;      // direction (+1 or -1)
    int g;      // group (I in the original)
    float s;    // "stroke weight" / intensity
    bool alive; // quick guard in case we want to reuse slots
};

static uint32_t t = 0;       // frame-ish counter
static const int MAXP = 256; // fewer particles for small grids
P ps[MAXP];

void resetParticle(P &p, uint32_t tt) {
    // Original: x=360,y=360 (center), a=t*1.25 + noise(I)*W, f=t%2*2-1, g=I,
    // s=5
    int I = (int)(tt / 50);
    p.x = (WIDTH - 1) / 2.0f;
    p.y = (HEIGHT - 1) / 2.0f;

    // noise(I)*W   -> use 1D Perlin noise via inoise8
    uint8_t n1 = inoise8(I * 19); // arbitrary scale
    float noiseW = (n1 / 255.0f) * WIDTH;

    p.a = tt * 1.25f + noiseW; // base angle component
    p.f = (tt & 1) ? +1 : -1;  // alternate direction
    p.g = I;
    p.s = 3.0f; // lower initial intensity for small grids
    p.alive = true;
}

inline void plotDot(int x, int y, uint8_t v) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
        return;
    // Additive white (Processing used stroke(W), i.e., white) with gain control
    leds[fl::XY((fl::u8)x, (fl::u8)y)] += CHSV(0, 0, scale8(v, kPointGain));
}

// draw a small disk ~ strokeWeight; 1..3 pixels radius
void plotSoftDot(float fx, float fy, float s) {
    // map s (decays from 5) to a pixel radius 1..3
    float r = constrain(s * 0.5f, 1.0f, 3.0f);
    int R = (int)ceilf(r);
    int cx = (int)roundf(fx);
    int cy = (int)roundf(fy);
    float r2 = r * r;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            float d2 = dx * dx + dy * dy;
            if (d2 <= r2) {
                // falloff toward edge
                float fall = 1.0f - (d2 / (r2 + 0.0001f));
                uint8_t v = (uint8_t)constrain(255.0f * fall, 0.0f, 255.0f);
                plotDot(cx + dx, cy + dy, v);
            }
        }
    }
}

void setup() {
    CLEDController *controller =
        &FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
    // When user adjusts UI, reflect into model speed
    // Provide screen map to UI with a specific LED diameter
    fl::XYMap xy = kMatrixSerpentineLayout
                       ? fl::XYMap::constructSerpentine(WIDTH, HEIGHT)
                       : fl::XYMap::constructRectangularGrid(WIDTH, HEIGHT);
    fl::ScreenMap screenmap = xy.toScreenMap();
    screenmap.setDiameter(0.15f);
    controller->setScreenMap(screenmap);
    // init particles as "dead"
    for (int i = 0; i < MAXP; ++i)
        ps[i].alive = false;
}



// Needed for proper blur mapping.
u16 xy_map_function(u16 x, u16 y, u16 width, u16 height) { return fl::XY(x, y); }
fl::XYMap xymap = fl::XYMap::constructWithUserFunction(WIDTH, HEIGHT, xy_map_function);

void loop() {
    // Processing does: background(0, t?9:!createCanvas(...)+W); filter(BLUR)
    // We emulate a very light global fade + blur to leave trails.
    // First a tiny global fade, then a mild Gaussian-ish blur.
    fadeToBlackBy(leds, NUM_LEDS, 18);      // stronger fade to prevent blowout
    blur2d(leds, WIDTH, HEIGHT, 24, xymap); // slightly gentler blur for 32x32

    // Spawn/overwrite one particle per frame, round-robin
    resetParticle(ps[t % MAXP], t);

    // Update & draw all particles
    for (int i = 0; i < MAXP; ++i) {
        if (!ps[i].alive)
            continue;
        P &p = ps[i];

        // strokeWeight(p.s *= .997)
        p.s *= 0.997f;
        if (p.s < 0.5f) {
            p.alive = false;
            continue;
        } // cheap cull

        // a += (noise(t/99, p.g) - .5) / 9
        // Use 2D noise: (t/99, g). inoise8 returns 0..255; center at ~128.
        float tOver99 = (float)t / 99.0f;
        uint8_t n2 = inoise8((uint16_t)(tOver99 * 4096), (uint16_t)(p.g * 37));
        float n2c = ((int)n2 - 128) / 255.0f; // ~ -0.5 .. +0.5
        p.a += (n2c) / 9.0f;

        // x += cos((a)*f), y += sin(a*f)   (original had (a+=...)*f inside cos)
        float aa = p.a * (float)p.f;
        p.x += cosf(aa);
        p.y += sinf(aa);

        // draw white point with softness according to s
        plotSoftDot(p.x, p.y, p.s);
    }

    FastLED.show();
    t++;
    // Cap frame rate a bit like Processing's draw loop
    FastLED.delay(16); // ~60 FPS
}

/*** Tips
 * - Want sharper trails? Lower blur2d strength or raise fadeToBlackBy amount.
 * - Too many or too few particles? Tweak MAXP.
 * - Want color instead of white? Replace plotDot/plotSoftDot to use CHSV with
 * hue based on p.g or p.a.
 * - If your matrix isn't serpentine, set kMatrixSerpentineLayout=false.
 */

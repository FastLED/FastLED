/// @file pixel_iterator_any_dither_bug.cpp
/// @brief Debug investigation of dither synchronization between Legacy and Channel APIs
///
/// This test isolates the dither synchronization issue where:
/// - Legacy creates 1 PixelController (R increments once)
/// - Channel creates 2 PixelControllers via PixelIteratorAny (R increments twice?)
///
/// Goal: Use debugger to trace R counter and dither array (d[], e[]) values

#include "test.h"
#include "FastLED.h"
#include "fl/pixel_iterator_any.h"
#include "pixel_controller.h"
#include "fl/dbg.h"

// Helper to print dither state
void print_dither_state(const char* label, uint8_t R, const uint8_t* d, const uint8_t* e) {
    FL_INFO(label << ": R=" << (int)R << ", d=[" << (int)d[0] << "," << (int)d[1] << "," << (int)d[2]
            << "], e=[" << (int)e[0] << "," << (int)e[1] << "," << (int)e[2] << "]");
}

FL_TEST_CASE("PixelIteratorAny Dither Synchronization Investigation") {
    FL_INFO("\n=== DITHER SYNCHRONIZATION DEBUG TEST ===\n");

    // Setup test data
    CRGB test_leds[4];
    for (int i = 0; i < 4; i++) {
        test_leds[i] = CRGB(100, 150, 200);
    }

    ColorAdjustment adj;
    adj.premixed = CRGB(200, 200, 200);  // Premixed with brightness 200
    #if FASTLED_HD_COLOR_MIXING
    adj.color = CRGB(255, 255, 255);
    adj.brightness = 200;
    #endif

    // =========================================================================
    // PATH 1: LEGACY (Single PixelController)
    // =========================================================================
    FL_INFO("--- PATH 1: Legacy (Single PixelController) ---");

    // Reset dither counter to 0 (simulates CFastLED::show() reset)
    PixelController<RGB, 1, 0xFFFFFFFF>::reset_dither_counter();
    uint8_t R_before_legacy = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    FL_INFO("R before Legacy PC creation: " << (int)R_before_legacy);

    // Create Legacy PixelController (BREAKPOINT HERE to trace init_binary_dithering)
    PixelController<RGB, 1, 0xFFFFFFFF> legacyPC(test_leds, 4, adj, BINARY_DITHER);

    uint8_t R_after_legacy = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    print_dither_state("Legacy PC after creation", R_after_legacy, legacyPC.d, legacyPC.e);

    // =========================================================================
    // PATH 2: CHANNEL (Two PixelControllers via PixelIteratorAny)
    // =========================================================================
    FL_INFO("\n--- PATH 2: Channel (Two PixelControllers via PixelIteratorAny) ---");

    // Reset dither counter to 0 (simulates CFastLED::show() reset)
    PixelController<RGB, 1, 0xFFFFFFFF>::reset_dither_counter();

    // Simulate Legacy having already run and incremented R (production scenario)
    PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter()++;
    uint8_t R_initial = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    FL_INFO("R initial (after simulated Legacy): " << (int)R_initial);

    // Pre-decrement R (current fix attempt)
    PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter()--;
    uint8_t R_after_decrement = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    FL_INFO("R after pre-decrement: " << (int)R_after_decrement);

    // Create first PixelController (BREAKPOINT HERE)
    PixelController<RGB, 1, 0xFFFFFFFF> channelPC1(test_leds, 4, adj, BINARY_DITHER);

    uint8_t R_after_pc1 = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    print_dither_state("Channel PC1 after creation", R_after_pc1, channelPC1.d, channelPC1.e);

    // Create PixelIteratorAny which creates second PixelController via copy (BREAKPOINT HERE)
    FL_INFO("\nCreating PixelIteratorAny (will create PC2 via copy constructor)...");
    fl::PixelIteratorAny any(channelPC1, GRB, fl::Rgbw());

    uint8_t R_after_any = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    FL_INFO("R after PixelIteratorAny creation: " << (int)R_after_any);

    // Access the internal PixelController to inspect dither state
    // Note: We can't directly access mAnyController, so we'll check R value

    // =========================================================================
    // COMPARISON
    // =========================================================================
    FL_INFO("\n--- COMPARISON ---");
    FL_INFO("Legacy final R: " << (int)R_after_legacy);
    FL_INFO("Channel final R: " << (int)R_after_any);
    FL_INFO("R values match: " << ((R_after_legacy == R_after_any) ? "YES" : "NO"));

    FL_INFO("\nLegacy dither: d=[" << (int)legacyPC.d[0] << "," << (int)legacyPC.d[1] << "," << (int)legacyPC.d[2]
            << "], e=[" << (int)legacyPC.e[0] << "," << (int)legacyPC.e[1] << "," << (int)legacyPC.e[2] << "]");
    FL_INFO("Channel PC1 dither: d=[" << (int)channelPC1.d[0] << "," << (int)channelPC1.d[1] << "," << (int)channelPC1.d[2]
            << "], e=[" << (int)channelPC1.e[0] << "," << (int)channelPC1.e[1] << "," << (int)channelPC1.e[2] << "]");

    // The issue: We can't access PC2 inside PixelIteratorAny to check its dither arrays
    // But we can verify R progression

    FL_CHECK(R_after_legacy == R_after_any);  // Should both be 1 if fix works
    FL_CHECK(legacyPC.d[0] == channelPC1.d[0]);  // Dither arrays should match
    FL_CHECK(legacyPC.d[1] == channelPC1.d[1]);
    FL_CHECK(legacyPC.d[2] == channelPC1.d[2]);

    FL_INFO("\n=== END DEBUG TEST ===");
}

// Additional test to examine copy constructor behavior
FL_TEST_CASE("PixelController Copy Constructor Dither Behavior") {
    FL_INFO("\n=== COPY CONSTRUCTOR DITHER TEST ===\n");

    CRGB test_leds[4];
    for (int i = 0; i < 4; i++) {
        test_leds[i] = CRGB(100, 150, 200);
    }

    ColorAdjustment adj;
    adj.premixed = CRGB(200, 200, 200);  // Premixed with brightness 200
    #if FASTLED_HD_COLOR_MIXING
    adj.color = CRGB(255, 255, 255);
    adj.brightness = 200;
    #endif

    // Reset R
    PixelController<RGB, 1, 0xFFFFFFFF>::reset_dither_counter();

    // Create original PixelController
    PixelController<RGB, 1, 0xFFFFFFFF> original(test_leds, 4, adj, BINARY_DITHER);
    uint8_t R_after_original = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    print_dither_state("Original PC", R_after_original, original.d, original.e);

    // Copy to different color order (BREAKPOINT: does this increment R?)
    FL_INFO("\nCopying to GRB PixelController...");
    PixelController<GRB, 1, 0xFFFFFFFF> copied(original);
    uint8_t R_after_copy = PixelController<RGB, 1, 0xFFFFFFFF>::getDitherCounter();
    print_dither_state("Copied PC (GRB)", R_after_copy, copied.d, copied.e);

    FL_INFO("\nR change during copy: " << (int)R_after_original << " -> " << (int)R_after_copy
            << " (delta: " << (int)(R_after_copy - R_after_original) << ")");
    FL_INFO("Copy constructor increments R: " << ((R_after_copy > R_after_original) ? "YES (BUG!)" : "NO (expected)"));

    // Verify dither arrays are copied (not re-initialized)
    FL_CHECK(original.d[0] == copied.d[0]);
    FL_CHECK(original.d[1] == copied.d[1]);
    FL_CHECK(original.d[2] == copied.d[2]);
    FL_CHECK(R_after_copy == R_after_original);  // Copy should NOT increment R

    FL_INFO("\n=== END COPY CONSTRUCTOR TEST ===");
}

#include "fl/eorder.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::EOrder enum values") {
    SUBCASE("RGB channel ordering") {
        // RGB = 0012 (octal)
        CHECK_EQ(static_cast<int>(RGB), 0012);
        CHECK_EQ(static_cast<int>(RGB), 10); // decimal
    }

    SUBCASE("RBG channel ordering") {
        // RBG = 0021 (octal)
        CHECK_EQ(static_cast<int>(RBG), 0021);
        CHECK_EQ(static_cast<int>(RBG), 17); // decimal
    }

    SUBCASE("GRB channel ordering") {
        // GRB = 0102 (octal)
        CHECK_EQ(static_cast<int>(GRB), 0102);
        CHECK_EQ(static_cast<int>(GRB), 66); // decimal
    }

    SUBCASE("GBR channel ordering") {
        // GBR = 0120 (octal)
        CHECK_EQ(static_cast<int>(GBR), 0120);
        CHECK_EQ(static_cast<int>(GBR), 80); // decimal
    }

    SUBCASE("BRG channel ordering") {
        // BRG = 0201 (octal)
        CHECK_EQ(static_cast<int>(BRG), 0201);
        CHECK_EQ(static_cast<int>(BRG), 129); // decimal
    }

    SUBCASE("BGR channel ordering") {
        // BGR = 0210 (octal)
        CHECK_EQ(static_cast<int>(BGR), 0210);
        CHECK_EQ(static_cast<int>(BGR), 136); // decimal
    }

    SUBCASE("all orderings are distinct") {
        // Verify each ordering is unique
        CHECK(RGB != RBG);
        CHECK(RGB != GRB);
        CHECK(RGB != GBR);
        CHECK(RGB != BRG);
        CHECK(RGB != BGR);
        CHECK(RBG != GRB);
        CHECK(RBG != GBR);
        CHECK(RBG != BRG);
        CHECK(RBG != BGR);
        CHECK(GRB != GBR);
        CHECK(GRB != BRG);
        CHECK(GRB != BGR);
        CHECK(GBR != BRG);
        CHECK(GBR != BGR);
        CHECK(BRG != BGR);
    }

    SUBCASE("channel extraction") {
        // The octal notation encodes channel positions
        // For RGB=0012 (octal): equals 10 in decimal
        // Extract octal digits: rightmost is B, middle is G, leftmost is R
        int rgb_val = static_cast<int>(RGB);
        int b_pos = rgb_val % 8;         // ones position in octal
        int g_pos = (rgb_val / 8) % 8;   // eights position in octal
        int r_pos = (rgb_val / 64) % 8;  // sixty-fours position in octal

        CHECK_EQ(r_pos, 0); // Red is first
        CHECK_EQ(g_pos, 1); // Green is second
        CHECK_EQ(b_pos, 2); // Blue is third
    }
}

TEST_CASE("fl::EOrderW enum values") {
    SUBCASE("white position values") {
        CHECK_EQ(static_cast<int>(W3), 0x3);
        CHECK_EQ(static_cast<int>(W2), 0x2);
        CHECK_EQ(static_cast<int>(W1), 0x1);
        CHECK_EQ(static_cast<int>(W0), 0x0);
    }

    SUBCASE("white default value") {
        CHECK_EQ(WDefault, W3);
        CHECK_EQ(static_cast<int>(WDefault), 0x3);
    }

    SUBCASE("white positions are sequential") {
        CHECK_EQ(static_cast<int>(W0), 0);
        CHECK_EQ(static_cast<int>(W1), 1);
        CHECK_EQ(static_cast<int>(W2), 2);
        CHECK_EQ(static_cast<int>(W3), 3);
    }

    SUBCASE("all white positions are distinct") {
        CHECK(W0 != W1);
        CHECK(W0 != W2);
        CHECK(W0 != W3);
        CHECK(W1 != W2);
        CHECK(W1 != W3);
        CHECK(W2 != W3);
    }
}

TEST_CASE("fl::EOrder usage patterns") {
    SUBCASE("assignment and comparison") {
        EOrder order1 = RGB;
        EOrder order2 = RGB;
        EOrder order3 = BGR;

        CHECK(order1 == order2);
        CHECK(order1 != order3);
    }

    SUBCASE("switch statement") {
        EOrder order = GRB;
        int result = 0;

        switch(order) {
            case RGB: result = 1; break;
            case RBG: result = 2; break;
            case GRB: result = 3; break;
            case GBR: result = 4; break;
            case BRG: result = 5; break;
            case BGR: result = 6; break;
        }

        CHECK_EQ(result, 3);
    }
}

TEST_CASE("fl::EOrderW usage patterns") {
    SUBCASE("assignment and comparison") {
        EOrderW pos1 = W2;
        EOrderW pos2 = W2;
        EOrderW pos3 = W0;

        CHECK(pos1 == pos2);
        CHECK(pos1 != pos3);
    }

    SUBCASE("switch statement") {
        EOrderW pos = W1;
        int result = 0;

        switch(pos) {
            case W0: result = 0; break;
            case W1: result = 1; break;
            case W2: result = 2; break;
            case W3: result = 3; break;
        }

        CHECK_EQ(result, 1);
    }

    SUBCASE("default white position") {
        EOrderW default_pos = WDefault;
        CHECK(default_pos == W3);
    }
}

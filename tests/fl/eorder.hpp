#include "fl/eorder.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::EOrder enum values") {
    FL_SUBCASE("RGB channel ordering") {
        // RGB = 0012 (octal)
        FL_CHECK_EQ(static_cast<int>(RGB), 0012);
        FL_CHECK_EQ(static_cast<int>(RGB), 10); // decimal
    }

    FL_SUBCASE("RBG channel ordering") {
        // RBG = 0021 (octal)
        FL_CHECK_EQ(static_cast<int>(RBG), 0021);
        FL_CHECK_EQ(static_cast<int>(RBG), 17); // decimal
    }

    FL_SUBCASE("GRB channel ordering") {
        // GRB = 0102 (octal)
        FL_CHECK_EQ(static_cast<int>(GRB), 0102);
        FL_CHECK_EQ(static_cast<int>(GRB), 66); // decimal
    }

    FL_SUBCASE("GBR channel ordering") {
        // GBR = 0120 (octal)
        FL_CHECK_EQ(static_cast<int>(GBR), 0120);
        FL_CHECK_EQ(static_cast<int>(GBR), 80); // decimal
    }

    FL_SUBCASE("BRG channel ordering") {
        // BRG = 0201 (octal)
        FL_CHECK_EQ(static_cast<int>(BRG), 0201);
        FL_CHECK_EQ(static_cast<int>(BRG), 129); // decimal
    }

    FL_SUBCASE("BGR channel ordering") {
        // BGR = 0210 (octal)
        FL_CHECK_EQ(static_cast<int>(BGR), 0210);
        FL_CHECK_EQ(static_cast<int>(BGR), 136); // decimal
    }

    FL_SUBCASE("all orderings are distinct") {
        // Verify each ordering is unique
        FL_CHECK(RGB != RBG);
        FL_CHECK(RGB != GRB);
        FL_CHECK(RGB != GBR);
        FL_CHECK(RGB != BRG);
        FL_CHECK(RGB != BGR);
        FL_CHECK(RBG != GRB);
        FL_CHECK(RBG != GBR);
        FL_CHECK(RBG != BRG);
        FL_CHECK(RBG != BGR);
        FL_CHECK(GRB != GBR);
        FL_CHECK(GRB != BRG);
        FL_CHECK(GRB != BGR);
        FL_CHECK(GBR != BRG);
        FL_CHECK(GBR != BGR);
        FL_CHECK(BRG != BGR);
    }

    FL_SUBCASE("channel extraction") {
        // The octal notation encodes channel positions
        // For RGB=0012 (octal): equals 10 in decimal
        // Extract octal digits: rightmost is B, middle is G, leftmost is R
        int rgb_val = static_cast<int>(RGB);
        int b_pos = rgb_val % 8;         // ones position in octal
        int g_pos = (rgb_val / 8) % 8;   // eights position in octal
        int r_pos = (rgb_val / 64) % 8;  // sixty-fours position in octal

        FL_CHECK_EQ(r_pos, 0); // Red is first
        FL_CHECK_EQ(g_pos, 1); // Green is second
        FL_CHECK_EQ(b_pos, 2); // Blue is third
    }
}

FL_TEST_CASE("fl::EOrderW enum values") {
    FL_SUBCASE("white position values") {
        FL_CHECK_EQ(static_cast<int>(W3), 0x3);
        FL_CHECK_EQ(static_cast<int>(W2), 0x2);
        FL_CHECK_EQ(static_cast<int>(W1), 0x1);
        FL_CHECK_EQ(static_cast<int>(W0), 0x0);
    }

    FL_SUBCASE("white default value") {
        FL_CHECK_EQ(WDefault, W3);
        FL_CHECK_EQ(static_cast<int>(WDefault), 0x3);
    }

    FL_SUBCASE("white positions are sequential") {
        FL_CHECK_EQ(static_cast<int>(W0), 0);
        FL_CHECK_EQ(static_cast<int>(W1), 1);
        FL_CHECK_EQ(static_cast<int>(W2), 2);
        FL_CHECK_EQ(static_cast<int>(W3), 3);
    }

    FL_SUBCASE("all white positions are distinct") {
        FL_CHECK(W0 != W1);
        FL_CHECK(W0 != W2);
        FL_CHECK(W0 != W3);
        FL_CHECK(W1 != W2);
        FL_CHECK(W1 != W3);
        FL_CHECK(W2 != W3);
    }
}

FL_TEST_CASE("fl::EOrder usage patterns") {
    FL_SUBCASE("assignment and comparison") {
        EOrder order1 = RGB;
        EOrder order2 = RGB;
        EOrder order3 = BGR;

        FL_CHECK(order1 == order2);
        FL_CHECK(order1 != order3);
    }

    FL_SUBCASE("switch statement") {
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

        FL_CHECK_EQ(result, 3);
    }
}

FL_TEST_CASE("fl::EOrderW usage patterns") {
    FL_SUBCASE("assignment and comparison") {
        EOrderW pos1 = W2;
        EOrderW pos2 = W2;
        EOrderW pos3 = W0;

        FL_CHECK(pos1 == pos2);
        FL_CHECK(pos1 != pos3);
    }

    FL_SUBCASE("switch statement") {
        EOrderW pos = W1;
        int result = 0;

        switch(pos) {
            case W0: result = 0; break;
            case W1: result = 1; break;
            case W2: result = 2; break;
            case W3: result = 3; break;
        }

        FL_CHECK_EQ(result, 1);
    }

    FL_SUBCASE("default white position") {
        EOrderW default_pos = WDefault;
        FL_CHECK(default_pos == W3);
    }
}

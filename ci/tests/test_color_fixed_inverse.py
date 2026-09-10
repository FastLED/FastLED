"""A Q16 bind-time inverse is accurate enough; the profile is what is left.

#4043's survey calls the fixed-point bind-time derivation "the one that is
genuinely blocked on nothing but effort", and gives the reason it is not small:
`EmitterProfile` stores floats, is P2's public type, and the legacy RGBW stack
consumes it.

That is a statement about the *profile*. It assumes the narrower question --
whether a Q16 3x3 inverse is accurate enough to replace the float one at all --
is already answered yes. It was not measured. If the answer were no, the
profile conversion would buy nothing, because the derivation would still have
to reach float to stay inside A1.

These measure it.
"""

from __future__ import annotations

import unittest

from ci.color_fixed_inverse_study import (
    collapsed_primaries,
    compare_inverses,
    corpus_primaries,
    invert3x3_q16,
    quantize_matrix,
    rounded_div,
    to_q16,
)
from ci.color_gamut_study import emitter_matrix


# A1's implementation-fidelity budget. The shipped mapper scores 0.15 of it
# with eight halvings, so this is the room the whole pipeline has, not the room
# this one step may take.
DELTA_E_BUDGET = 0.5


class TestTheFixedPointInverseIsAccurateEnough(unittest.TestCase):
    def test_real_primary_sets_agree_to_a_ulp(
        self: "TestTheFixedPointInverseIsAccurateEnough",
    ) -> None:
        # sRGB, BT.2020 and Display P3. The two derivations end in the same
        # format, so a disagreement here is the arithmetic in between and
        # nothing else.
        for name, primaries in corpus_primaries()[:3]:
            with self.subTest(device=name):
                result = compare_inverses(name, primaries, 8)
                self.assertLessEqual(result.worst_coefficient_ulps, 1)
                self.assertLess(result.worst_delta_e, 0.01)

    def test_the_error_stays_far_inside_the_budget(
        self: "TestTheFixedPointInverseIsAccurateEnough",
    ) -> None:
        for name, primaries in corpus_primaries():
            with self.subTest(device=name):
                result = compare_inverses(name, primaries, 8)
                self.assertLess(result.worst_delta_e, DELTA_E_BUDGET / 10.0)


class TestWhereItGivesOut(unittest.TestCase):
    def test_coefficient_error_explodes_as_the_matrix_collapses(
        self: "TestWhereItGivesOut",
    ) -> None:
        # The limit has to be approached rather than asserted, and this is the
        # half that behaves as expected: a shrinking determinant costs the
        # coefficients their precision, by four orders of magnitude.
        gentle = compare_inverses("gentle", collapsed_primaries(0.3), 4)
        severe = compare_inverses("severe", collapsed_primaries(0.999), 4)
        self.assertLessEqual(gentle.worst_coefficient_ulps, 1)
        self.assertGreater(severe.worst_coefficient_ulps, 1000)

    def test_but_the_colour_error_does_not(self: "TestWhereItGivesOut") -> None:
        # And this is the half that does not, which is the finding. Those
        # thousands of ULPs sit in directions a near-collinear device can
        # barely produce, so they do not become visible error: even at the
        # edge of singularity the disagreement stays an order of magnitude
        # inside A1.
        for fraction in (0.9, 0.99, 0.999):
            with self.subTest(fraction=fraction):
                result = compare_inverses(
                    "collapsing", collapsed_primaries(fraction), 4
                )
                self.assertLess(result.worst_delta_e, DELTA_E_BUDGET / 5.0)


class TestTheArithmetic(unittest.TestCase):
    def test_a_singular_matrix_is_refused(self: "TestTheArithmetic") -> None:
        # Two identical primaries: the columns are equal, so the determinant
        # is zero in Q16 as well as in float, and the caller gets None rather
        # than a division by zero.
        duplicated = ((0.6400, 0.3300), (0.6400, 0.3300), (0.1500, 0.0600))
        quantized = quantize_matrix(emitter_matrix(duplicated))
        self.assertIsNone(invert3x3_q16(quantized))

    def test_the_inverse_reproduces_the_identity(self: "TestTheArithmetic") -> None:
        # Independent of the float path: multiplying the Q16 inverse by the
        # Q16 forward matrix must give the identity, which catches a
        # transposed cofactor that comparing against float cannot -- the float
        # path would be transposed the same way.
        #
        # The bound is a measurement, not a target. Across the corpus the
        # worst residual is 7 ULP (BT.2020), which is 1.07e-04 of unity and is
        # the Q16 rounding of a three-term dot product with coefficients of
        # magnitude three to five. 16 leaves room without letting a real
        # transposition through: swapping two cofactors moves an off-diagonal
        # entry by order unity, which is 65536 ULP.
        for name, primaries in corpus_primaries():
            with self.subTest(device=name):
                forward = quantize_matrix(emitter_matrix(primaries))
                inverse = invert3x3_q16(forward)
                assert inverse is not None
                for row in range(3):
                    for col in range(3):
                        total = 0
                        for k in range(3):
                            total += inverse[row][k] * forward[k][col]
                        product = (total + (1 << 15)) >> 16
                        expected = to_q16(1.0) if row == col else 0
                        self.assertLess(abs(product - expected), 16)

    def test_rounding_is_symmetric_about_zero(self: "TestTheArithmetic") -> None:
        # Truncating would bias every coefficient toward zero, and a solve
        # sums three of them, so the bias would not cancel.
        self.assertEqual(rounded_div(7, 2), 4)
        self.assertEqual(rounded_div(-7, 2), -4)
        self.assertEqual(rounded_div(7, -2), -4)
        self.assertEqual(rounded_div(-7, -2), 4)
        self.assertEqual(rounded_div(5, 10), 1)
        self.assertEqual(rounded_div(-5, 10), -1)
        with self.assertRaises(ValueError):
            rounded_div(1, 0)


class TestInputValidation(unittest.TestCase):
    def test_degenerate_arguments_are_refused(self: "TestInputValidation") -> None:
        from ci.color_fixed_inverse_study import sweep_drives

        with self.assertRaises(ValueError):
            sweep_drives(0)
        with self.assertRaises(ValueError):
            collapsed_primaries(-0.1)
        with self.assertRaises(ValueError):
            collapsed_primaries(1.5)


if __name__ == "__main__":
    unittest.main()

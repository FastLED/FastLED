# APA102 / Dotstar Gamma Bit shifting gamma correction formula

APA102 has a global brightness per-led setting that is 5-bits.

There is an interesting property where each shift in global brightness shifts the brightness of each component by the same amount.

For example, say we have format RGBV(8,8,8,5), where V is the Global brightness setting and each value is the bit-depth.

Then it follows that when Global brightness is shifted, so too do each of the components.
RGBV(R << 1, G << 1, B << 1, V) == RGBV(R, G, B, V << 1)
Likewise shifting down.

# Algorithm

  * gamma correct RGB(8,8,8) -> RGB(16, 16, 16)
  * V = 0xff
  * while (V >> 1) > 1 && (max32(R16,G16,B16) << 2) < int32(0xffff):
  *   R16 = R16 << 2
  *   B16 = B16 << 2
  *   G16 = G16 << 2
  *   V = V >> 1
  * convert RGB16 + V back to RGB + V
  *   R8 = R16 / 255
  *   G8 = G16 / 255
  *   B8 = B16 / 255
  * Output: RGBV(R8, G8, B8, V)



# Notes
8-bits r,g,b
5-bits brightness

8-bit -> 16-bit
RGB(0xFF, 0xFF, 0xFF) -> Gamma correction -> RGB16(0xFFFF, 0xFFFF, 0xFFFF)

# Further work

There is likely a multiplication method for this as well that scales 8-bit and adjusts the 5 bit color accordingly.

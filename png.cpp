#ifdef __unix__

#define FASTLED_INTERNAL
#include <string>
#include "FastLED.h"
#include "png.hpp"

FASTLED_NAMESPACE_BEGIN

// Simple fast n x 1 pixel uncompressed PNG file creator library.

Png::Png(size_t width, const unsigned char *data)
    : chunk_offset(0), block_offset(0) {
  make_crc_table();
  buf = std::string(png_magic, sizeof(png_magic));
  ihdr(width);
  // In theory we could set gAMA = 1.0 and send the linear RGB
  // values unchanged. But most browsers don't understand this
  // and show the resulting image too dimly. Instead, we
  // require and declare sRGB values.
  srgb(0);
  idata(data, 3 * width);
  iend();
}

// CRC code from https://www.w3.org/TR/2003/REC-PNG-20031110/#D-CRCAppendix

// Make the table for a fast CRC.
void Png::make_crc_table() {
  for (int n = 0; n < 256; n++) {
    unsigned long c = (unsigned long)n;
    for (int k = 0; k < 8; k++) {
      if (c & 1) {
        c = 0xedb88320L ^ (c >> 1);
      } else {
        c = c >> 1;
      }
    }
    crc_table[n] = c;
  }
}

// Update a running CRC with the bytes buf[0..len-1]--the CRC
// should be initialized to all 1's, and the transmitted value
// is the 1's complement of the final running CRC (see the
// crc() routine below).
unsigned long Png::update_crc(unsigned long crc, const unsigned char *buf,
                              int len) {
  unsigned long c = crc;

  for (int n = 0; n < len; n++) {
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  return c;
}

// Return the CRC of the bytes buf[0..len-1].
unsigned long Png::crc(const unsigned char *buf, int len) {
  return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

// Update a running Adler-32 checksum with the bytes buf[0..len-1]
// and return the updated checksum. The Adler-32 checksum should be
// initialized to 1.
//
// Usage example:
//
//   unsigned long adler = 1L;
//
//   while (read_buffer(buffer, length) != EOF) {
//     adler = update_adler32(adler, buffer, length);
//   }
//   if (adler != original_adler) error();
//
unsigned long Png::update_adler32(unsigned long adler, const unsigned char *buf,
                                  int len) {
  unsigned long s1 = adler & 0xffff;
  unsigned long s2 = (adler >> 16) & 0xffff;

  for (int n = 0; n < len; n++) {
    s1 = (s1 + buf[n]) % ADLER_BASE;
    s2 = (s2 + s1) % ADLER_BASE;
  }
  return (s2 << 16) + s1;
}

// Return the adler32 of the bytes buf[0..len-1]
unsigned long Png::adler32(const unsigned char *buf, int len) {
  return update_adler32(1L, buf, len);
}

FASTLED_NAMESPACE_END

#endif
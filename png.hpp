#ifndef __INC_PNG_HPP
#define __INC_PNG_HPP
#include <string>
#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

// Simple fast n x 1 pixel uncompressed PNG file creator library.

class Png {
public:
  Png(size_t width, const unsigned char *data);

  inline void ihdr(size_t width) {
    beginchunk("IHDR");
    appendlong(width); // width
    appendlong(1);     // height
    appendbyte(8);     // bit depth
    appendbyte(2);     // color type - true colour
    appendbyte(0);     // compression
    appendbyte(0);     // filter
    appendbyte(0);     // interlace
    endchunk();
  }

  inline void srgb(uint8_t intent) {
    beginchunk("sRGB");
    appendbyte(intent);
    endchunk();
  }

  inline void gama(double gamma) {
    beginchunk("gAMA");
    appendlong((unsigned int)(100000.0 * gamma));
    endchunk();
  }

  inline void idata(const unsigned char *buf, size_t len) {
    beginchunk("IDAT");
    // RFC1950 ZLIB format using RFC1951 DEFLATE data format
    appendword(((0x0800 + 30) / 31) * 31); // compression method: none
    beginblock(true);
    appendbyte(0); // filter: None
    appendstr((const char *)buf, len);
    endblock();
    endchunk();
  }

  inline void iend() {
    beginchunk("IEND");
    endchunk();
  }

  inline void beginchunk(const char *name) {
    chunk_offset = pos();
    appendlong(0); // space for length
    appendstr(name, 4);
  }

  inline void endchunk() {
    unsigned long zlen = pos() - chunk_offset - 8;
    unsigned long zcrc = crc(data(chunk_offset + 4), zlen + 4);
    appendlong(zcrc);                // checksum
    replacelong(chunk_offset, zlen); // fill in length
  }

  inline void beginblock(bool final) {
    block_offset = pos();
    appendbyte(final ? 0x01 : 0x00); // not compressed, maybe final block
    appendwordX(0);                  // space for length
    appendwordX(0);                  // space for length inverted
  }

  inline void endblock() {
    unsigned long zlen = pos() - block_offset - 5;
    unsigned long zadler = adler32(data(block_offset + 5), zlen);
    appendlong(zadler);                    // checksum
    replacewordX(block_offset + 1, zlen);  // length
    replacewordX(block_offset + 3, ~zlen); // inverted length
  }

  inline size_t pos() { return buf.length(); }

  inline const unsigned char *data(size_t offset) {
    return (const unsigned char *)(buf.data() + offset);
  }

  inline void append(const std::string &str) { buf.append(str); }

  inline void appendstr(const char *str, size_t len) { buf.append(str, len); }

  inline void appendbyte(unsigned char b) { buf.push_back(char(b)); }

  inline void appendword(unsigned int w) {
    buf.push_back(char((w >> 8) & 0xff));
    buf.push_back(char(w & 0xff));
  }

  inline void appendwordX(unsigned int w) {
    buf.push_back(char(w & 0xff));
    buf.push_back(char((w >> 8) & 0xff));
  }

  inline void appendlong(unsigned int v) {
    buf.push_back(char((v >> 24) & 0xff));
    buf.push_back(char((v >> 16) & 0xff));
    buf.push_back(char((v >> 8) & 0xff));
    buf.push_back(char(v & 0xff));
  }

  inline void replacewordX(size_t pos, unsigned int w) {
    buf[pos] = char(w & 0xff);
    buf[pos + 1] = char((w >> 8) & 0xff);
  }

  inline void replacelong(size_t pos, unsigned int v) {
    buf[pos] = char((v >> 24) & 0xff);
    buf[pos + 1] = char((v >> 16) & 0xff);
    buf[pos + 2] = char((v >> 8) & 0xff);
    buf[pos + 3] = char(v & 0xff);
  }

  inline const char *data() const { return buf.data(); }

  inline size_t len() const { return buf.length(); }

  // CRC code from https://www.w3.org/TR/2003/REC-PNG-20031110/#D-CRCAppendix

  // Make the table for a fast CRC.
  void make_crc_table();

  // Update a running CRC with the bytes buf[0..len-1]--the CRC
  // should be initialized to all 1's, and the transmitted value
  // is the 1's complement of the final running CRC (see the
  // crc() routine below). `make_crc_table` must have been called first.
  unsigned long update_crc(unsigned long crc, const unsigned char *buf,
                           int len);

  // Return the CRC of the bytes buf[0..len-1].
  unsigned long crc(const unsigned char *buf, int len);

  // Adler-32 code from https://tools.ietf.org/html/rfc1950#section-9

  const unsigned long ADLER_BASE = 65521; // largest prime smaller than 65536

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
  unsigned long update_adler32(unsigned long adler, const unsigned char *buf,
                               int len);

  // Return the adler32 of the bytes buf[0..len-1]
  unsigned long adler32(const unsigned char *buf, int len);

private:
  const char png_magic[8] = {(char)137, 80, 78, 71, 13, 10, 26, 10};
  std::string buf;
  size_t chunk_offset;
  size_t block_offset;

  // Table of CRCs of all 8-bit messages.
  unsigned long crc_table[256];
};

FASTLED_NAMESPACE_END

#endif

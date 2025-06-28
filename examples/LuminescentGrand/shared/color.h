#ifndef COLOR_H_
#define COLOR_H_

#include "fl/stdint.h"

struct Color3i {
  static Color3i Black() { return Color3i(0x0, 0x0, 0x0); }
  static Color3i White() { return Color3i(0xff, 0xff, 0xff); }
  static Color3i Red() { return Color3i(0xff, 0x00, 0x00); }
  static Color3i Orange() { return Color3i(0xff, 0xff / 2,00); }
  static Color3i Yellow() { return Color3i(0xff, 0xff,00); }
  static Color3i Green() { return Color3i(0x00, 0xff, 0x00); }
  static Color3i Cyan() { return Color3i(0x00, 0xff, 0xff); }
  static Color3i Blue() { return Color3i(0x00, 0x00, 0xff); }
  Color3i(uint8_t r, uint8_t g, uint8_t b) { Set(r,g,b); }
  Color3i() { Set(0xff, 0xff, 0xff); }
  Color3i(const Color3i& other) { Set(other); }
  Color3i& operator=(const Color3i& other) {
    if (this != &other) {
      Set(other);
    }
    return *this;
  }

  void Set(uint8_t r, uint8_t g, uint8_t b) { r_ = r; g_ = g; b_ = b; }
  void Set(const Color3i& c) { Set(c.r_, c.g_, c.b_); }
  void Mul(const Color3i& other_color);
  void Mulf(float scale);  // Input range is 0.0 -> 1.0
  void Mul(uint8_t val) {
    Mul(Color3i(val, val, val));
  }
  void Sub(const Color3i& color);
  void Add(const Color3i& color);
  uint8_t Get(int rgb_index) const;
  void Set(int rgb_index, uint8_t val);
  void Fill(uint8_t val) { Set(val, val, val); }
  uint8_t MaxRGB() const {
    uint8_t max_r_g = r_ > g_ ? r_ : g_;
    return max_r_g > b_ ? max_r_g : b_;
  }

  template <typename PrintStream>
  inline void Print(PrintStream* stream) const {
	  stream->print("RGB:\t");
    stream->print(r_); stream->print(",\t");
    stream->print(g_); stream->print(",\t");
    stream->print(b_);
    stream->print("\n");
  }

  void Interpolate(const Color3i& other_color, float t);

  uint8_t* At(int rgb_index);
  const uint8_t* At(int rgb_index) const;

  uint8_t r_, g_, b_;
};


struct ColorHSV {
  ColorHSV() : h_(0), s_(0), v_(0) {}
  ColorHSV(float h, float s, float v) {
    Set(h,s,v);
  }
  explicit ColorHSV(const Color3i& color) {
    FromRGB(color);
  }
  ColorHSV& operator=(const Color3i& color) {
    FromRGB(color);
    return *this;
  }
  ColorHSV(const ColorHSV& other) {
    Set(other);
  }
  ColorHSV& operator=(const ColorHSV& other) {
    if (this != &other) {
      Set(other);
    }
    return *this;
  }
  void Set(const ColorHSV& other) {
	  Set(other.h_, other.s_, other.v_);
  }
  void Set(float h, float s, float v) {
    h_ = h;
    s_ = s;
    v_ = v;
  }

  template <typename PrintStream>
  inline void Print(PrintStream* stream) {
	  stream->print("HSV:\t");
    stream->print(h_); stream->print(",\t");
    stream->print(s_); stream->print(",\t");
    stream->print(v_); stream->print("\n");
  }

  bool operator==(const ColorHSV& other) const {
    return h_ == other.h_ && s_ == other.s_ && v_ == other.v_;
  }
  bool operator!=(const ColorHSV& other) const {
    return !(*this == other);
  }
  void FromRGB(const Color3i& rgb);

  Color3i ToRGB() const;

  float h_, s_, v_;
};


#endif  // COLOR_H_

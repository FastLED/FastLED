

#include <Arduino.h>

#include "./color.h"
#include "./util.h"
#include "fl/math_macros.h"


///////////////////////////////////////////////////////////////////////////////
void Color3i::Mul(const Color3i& other_color) {
  int r = r_;
  int g = g_;
  int b = b_;

  r = r * int(other_color.r_) / 255;
  g = g * int(other_color.g_) / 255;
  b = b * int(other_color.b_) / 255;
  Set(r, g, b);
}

///////////////////////////////////////////////////////////////////////////////
void Color3i::Mulf(float scale) {
  const int s = static_cast<int>(scale * 255.0f);

  int r = static_cast<int>(r_) * s / 255;
  int g = static_cast<int>(g_) * s / 255;
  int b = static_cast<int>(b_) * s / 255;

  Set(r, g, b);
}

///////////////////////////////////////////////////////////////////////////////
void Color3i::Sub(const Color3i& color) {
  if (r_ < color.r_) r_ = 0;
  else               r_ -= color.r_;
  if (g_ < color.g_) g_ = 0;
  else               g_ -= color.g_;
  if (b_ < color.b_) b_ = 0;
  else               b_ -= color.b_;
}

///////////////////////////////////////////////////////////////////////////////
void Color3i::Add(const Color3i& color) {
  if ((255 - r_) < color.r_) r_ = 255;
  else                       r_ += color.r_;
  if ((255 - g_) < color.g_) g_ = 255;
  else                       g_ += color.g_;
  if ((255 - b_) < color.b_) b_ = 255;
  else                       b_ += color.b_;
}

///////////////////////////////////////////////////////////////////////////////
uint8_t Color3i::Get(int rgb_index) const {
  const uint8_t* rgb = At(rgb_index);
  return rgb ? *rgb : 0;
}

///////////////////////////////////////////////////////////////////////////////
void Color3i::Set(int rgb_index, uint8_t val) {
  uint8_t* rgb = At(rgb_index);
  if (rgb) {
    *rgb = val;
  }
}

///////////////////////////////////////////////////////////////////////////////
void Color3i::Interpolate(const Color3i& other_color, float t) {
  if (0.0f >= t) {
    Set(other_color);
  } else if (1.0f <= t) {
    return;
  }

  Color3i new_color = other_color;
  new_color.Mul(1.0f - t);
  this->Mul(t);
  this->Add(new_color);
}

///////////////////////////////////////////////////////////////////////////////
uint8_t* Color3i::At(int rgb_index) {
  switch(rgb_index) {
    case 0: return &r_;
    case 1: return &g_;
    case 2: return &b_;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
const uint8_t* Color3i::At(int rgb_index) const {
  switch(rgb_index) {
    case 0: return &r_;
    case 1: return &g_;
    case 2: return &b_;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
void ColorHSV::FromRGB(const Color3i& rgb) {
  typedef double FloatT;
  FloatT r = (FloatT) rgb.r_/255.f;
  FloatT g = (FloatT) rgb.g_/255.f;
  FloatT b = (FloatT) rgb.b_/255.f;
  FloatT max_rgb = MAX(r, MAX(g, b));
  FloatT min_rgb = MIN(r, MIN(g, b));
  v_ = max_rgb;

  FloatT d = max_rgb - min_rgb;
  s_ = max_rgb == 0 ? 0 : d / max_rgb;

  if (max_rgb == min_rgb) {
    h_ = 0; // achromatic
  } else {
    if (max_rgb == r) {
      h_ = (g - b) / d + (g < b ? 6 : 0);
    } else if (max_rgb == g) {
      h_ = (b - r) / d + 2;
    } else if (max_rgb == b) {
      h_ = (r - g) / d + 4;
    }
    h_ /= 6;
  }
}

///////////////////////////////////////////////////////////////////////////////
Color3i ColorHSV::ToRGB() const {
  typedef double FloatT;
  FloatT r = 0;
  FloatT g = 0;
  FloatT b = 0;

  int i = int(h_ * 6);
  FloatT f = h_ * 6.0 - static_cast<FloatT>(i);
  FloatT p = v_ * (1.0 - s_);
  FloatT q = v_ * (1.0 - f * s_);
  FloatT t = v_ * (1.0 - (1.0 - f) * s_);

  switch(i % 6){
    case 0: r = v_, g = t, b = p; break;
    case 1: r = q, g = v_, b = p; break;
    case 2: r = p, g = v_, b = t; break;
    case 3: r = p, g = q, b = v_; break;
    case 4: r = t, g = p, b = v_; break;
    case 5: r = v_, g = p, b = q; break;
  }

  return Color3i(round(r * 255), round(g * 255), round(b * 255));
}

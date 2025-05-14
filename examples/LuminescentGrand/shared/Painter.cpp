

#include <Arduino.h>

#include "./Painter.h"
#include "./led_layout_array.h"
#include "./dprint.h"
#include "./Keyboard.h"
#include "fl/math_macros.h"
#include "fl/math.h"
#include "fl/warn.h"

namespace {

float LuminanceDecay(float time) {
  typedef InterpData<float, float> Datum;
  static const Datum kData[] = {
    Datum(0,  0),
    Datum(1,  0),
    Datum(10, 0),
    Datum(47, 60),
    Datum(120, 100),
    Datum(230, 160),
    Datum(250, 255),
    Datum(254, 255),
    Datum(255, 64), 
  };
  
  const float key = time * 255.f;
  static const int n = sizeof(kData) / sizeof(kData[0]);    
  float approx_val = Interp(key, kData, n);
  

  static const float k = (1.0f / 255.f);
  const float out = approx_val * k;    
  return out;
}

float CalcLuminance(float time_delta_ms,
                    bool sustain_pedal_on,
                    const Key& key,
                    int key_idx) {
                             
  if (key.curr_color_.v_ <= 0.0) {
    return 0.0;
  }
                             
  const bool dampened_key = (key_idx < kFirstNoteNoDamp);
  
  const float decay_factor = CalcDecayFactor(sustain_pedal_on,
                                             key.on_,
                                             key_idx,
                                             key.velocity_ * (1.f/127.f),  // Normalizing
                                             dampened_key,
                                             time_delta_ms);
                                                 
  if (key.on_) {
    //const float brigthness_factor = sin(key.orig_color_.v_ * PI / 2.0);
    float brigthness_factor = 0.0f;
    
    if (kUseLedCurtin) {
      brigthness_factor = sqrt(sqrt(key.orig_color_.v_));
    } else {
      //brigthness_factor = key.orig_color_.v_ * key.orig_color_.v_;
      brigthness_factor = key.orig_color_.v_;
    }
    return LuminanceDecay(decay_factor) * brigthness_factor;
    //return 1.0f;
  } else {
    return decay_factor * key.orig_color_.v_;
  }
}

float CalcSaturation(float time_delta_ms, const ColorHSV& color, bool key_on) {
  if (color.v_ <= 0.0) {
    return color.s_;
  }
  if (!key_on) {
    return 1.0f;
  }
  static const float kDefaultSaturationTime = 0.05f * 1000.f;
  
  // At time = 0.0 the saturation_factor will be at 0.0 and then transition to 1.0
  float saturation_factor = mapf(time_delta_ms,
                                 0.0f, kDefaultSaturationTime,
                                 0.0f, 1.0f);
  // As time increases the saturation factor will continue
  // to grow past 1.0. We use min to clamp it back to 1.0.
  saturation_factor = MIN(1.0f, saturation_factor);
  // TODO - make the saturation interpolate between the original
  // color and the unsaturated state.
 return saturation_factor;
}

}  // namespace.


void Painter::Paint(uint32_t now_ms,
                    uint32_t delta_ms,
                    VisState vis_state,
                    KeyboardState* keyboard,
                    LedRopeInterface* light_rope) {
  for (int i = 0; i < KeyboardState::kNumKeys; ++i) {
    Key& key = keyboard->keys_[i];
    
    const float time_delta_ms = static_cast<float>(now_ms - key.event_time_);
        
    const float lum = CalcLuminance(time_delta_ms, keyboard->sustain_pedal_, key, i);
    const float sat = CalcSaturation(time_delta_ms, key.curr_color_, key.on_);

    //if (key.idx_ == 56) {
    //  dprint("lum: "); dprint(lum*255.f); dprint(" sat:"); dprintln(sat*255.f);
    //}
    
    key.curr_color_.v_ = lum;
    key.curr_color_.s_ = sat;

    // Removing this line breaks one of the visualizers...
    // TODO: Figure out a cleaner solution.
    light_rope->Set(i, key.curr_color_.ToRGB());
  }

  LedColumns led_columns = LedLayoutArray();

  switch (vis_state) {
    case Painter::kBlockNote: {
      light_rope->DrawSequentialRepeat(kNumLightsPerNote);
      break;
    }
    case Painter::kColumnNote: {
      light_rope->DrawRepeat(led_columns.array, kNumKeys);
      break;
    }
    case Painter::kVUNote: {
      PaintVuNotes(now_ms, *keyboard, led_columns.array, kNumKeys, light_rope);
      break; 
    }
    case Painter::kVUMidNote: {
      PaintVuMidNotesFade(delta_ms, *keyboard, led_columns.array, kNumKeys, light_rope);
      break;
    }
    
    case Painter::kVegas: { // aka "vegas mode?"
      VegasVisualizer(*keyboard, led_columns.array, kNumKeys, light_rope);
      break;
    }
    
    case Painter::kBrightSurprise: {
      PaintBrightSurprise(*keyboard, led_columns.array, kNumKeys, light_rope);
      break;
    }
    
    case Painter::kVUSpaceInvaders: {
      PaintVuSpaceInvaders(now_ms, *keyboard, led_columns.array, kNumKeys, light_rope);
      break; 
    }

    default:
      dprint("Unknown mode: "); dprint(vis_state); dprint(".\n");
      break;
  }
}

void Painter::PaintVuNotes(uint32_t /*now_ms*/,
                           const KeyboardState& keyboard,
                           const int* led_column_table, int led_column_table_length,
                           LedRopeInterface* led_rope) {


 FASTLED_WARN("\n\n############## VU NOTES ################\n\n");
 
 led_rope->RawBeginDraw();
                     
 for (int i = 0; i < led_column_table_length; ++i) {
    const Key& key = keyboard.keys_[i];
    
    
    // Map the white keys to the bottom and the black keys to the top.
    bool black_key = false;
    switch (key.idx_ % 12) {
      case 1:
      case 4:
      case 6:
      case 9:
      case 11:
        black_key = true;
        break;
    }
    
    const int pixel_count = led_column_table[i];
    const int draw_pixel_count = ceil(pixel_count * sqrt(key.curr_color_.v_));
    
    const int black_pixel_count = pixel_count - draw_pixel_count;
    const Color3i& c = *led_rope->GetIterator(i);
    

    const bool reverse_correct = black_key == (key.idx_ % 2);

    if (reverse_correct) {
      for (int j = 0; j < draw_pixel_count; ++j) {
        if (j < draw_pixel_count - 1) {
          led_rope->RawDrawPixel(c);
        } else {
          // Last pixel.
          ColorHSV hsv(random(512) / 512.f, random(512) / 512.f, 1.0);
          led_rope->RawDrawPixel(hsv.ToRGB());
        }
      }
      
      for (int j = 0; j < black_pixel_count; ++j) {
        led_rope->RawDrawPixel(Color3i::Black());
      }       
    } else {
      for (int j = 0; j < black_pixel_count; ++j) {
        led_rope->RawDrawPixel(Color3i::Black());
      } 
      
      for (int j = draw_pixel_count - 1; j >= 0; --j) {
        if (j < draw_pixel_count - 1) {
          led_rope->RawDrawPixel(c);
        } else {
          // Last pixel.
          ColorHSV hsv(random(512) / 512.f, random(512) / 512.f, 1.0);
          led_rope->RawDrawPixel(hsv.ToRGB());
        }
      }
    }
  }
  led_rope->RawCommitDraw();
}

void Painter::PaintVuMidNotesFade(uint32_t /*delta_ms*/,
                                  const KeyboardState& keyboard,
                                  const int* led_column_table, int led_column_table_length,
                                  LedRopeInterface* led_rope) {

  FASTLED_WARN("\n\n############## VU MID NOTES FADE ################\n\n");

  struct DrawPoints {
    int n_black0;
    int n_fade0;
    int n_fill;
    int n_fade1;
    int n_black1;
    float fade_factor;  // 0->1.0

    float SumBrightness() const {
      float out = 0;
      out += n_fill;
      out += (fade_factor * n_fade0);
      out += (fade_factor * n_fade1);
      return out;
    }
  };

  // Generator for the DrawPoints struct above.
  //  n_led: How many led's there are in total.
  //  factor: 0->1, indicates % of led's "on".
  struct F {
    static DrawPoints Generate(int n_led, float factor) {
      DrawPoints out;
      memset(&out, 0, sizeof(out));
      if (n_led == 0 || factor == 0.0f) {
        out.n_black0 = n_led;
        return out;
      }
      const int is_odd = (n_led % 2);
      const int n_half_lights = n_led / 2 + is_odd;
      const float f_half_fill = n_half_lights * factor;
      const int n_half_fill = static_cast<int>(f_half_fill);  // Truncates float.
      
      float fade_pix_perc = f_half_fill - static_cast<float>(n_half_fill);
      int n_fade_pix = fade_pix_perc < 1.0f;
      if (n_half_fill == 0) {
        n_fade_pix = 1;
      }
      int n_half_black = n_half_lights - n_half_fill - n_fade_pix;
      
      int n_fill_pix = 0;
      if (n_half_fill > 0) {
        n_fill_pix = n_half_fill * 2 + (is_odd ? -1 : 0);  
      }
      
      out.n_black0 = n_half_black;
      out.n_fade0 = n_fade_pix;
      out.n_fill = n_fill_pix;
      out.n_fade1 = n_fade_pix;
      if (!n_fill_pix && is_odd) {
        out.n_fade1 = 0;
      }
      out.n_black1 = n_half_black;
      out.fade_factor = fade_pix_perc;
      return out;
    }
 };
                                  
 
 led_rope->RawBeginDraw();
                           
 for (int i = 0; i < led_column_table_length; ++i) {
    const Key& key = keyboard.keys_[i];

    float active_lights_factor = key.IntensityFactor();

    //if (key.curr_color_.v_ <= 0.f) {
    //  active_lights_factor = 0.0;
    //}

    const int n_led = led_column_table[i];

    if (active_lights_factor > 0.0f) {
      DrawPoints dp = F::Generate(n_led, active_lights_factor);
      
      ColorHSV hsv = key.curr_color_;
      hsv.v_ = 1.0;
      Color3i color = hsv.ToRGB();
      // Now figure out optional fade color
      Color3i fade_col;
      ColorHSV c = key.curr_color_;
      c.v_ = dp.fade_factor;
      fade_col = c.ToRGB();
      
      // Output to graphics.
      led_rope->RawDrawPixels(Color3i::Black(), dp.n_black0);
      led_rope->RawDrawPixels(fade_col, dp.n_fade0);
      led_rope->RawDrawPixels(color, dp.n_fill);
      led_rope->RawDrawPixels(fade_col, dp.n_fade1);
      led_rope->RawDrawPixels(Color3i::Black(), dp.n_black1);

#ifdef DEBUG_PAINTER
      if (active_lights_factor > 0.0) {
        int total_lights_on = dp.SumBrightness();
        //dprint("total_lights_on: "); dprint(total_lights_on);
        //dprint(", total lights written: "); dprintln(total_lights_on + dp.n_black0 + dp.n_black1);

        //float total = (dp.n_fade0 * dp.fade_factor) + (dp.n_fade1 * dp.fade_factor) + static_cast<float>(dp.n_fill);
        #define P(X) dprint(", "#X ": "); dprint(X);

        //dprint("active_lights_factor: "); dprintln(active_lights_factor);

        //P(dp.n_black0); P(dp.n_fade0); P(dp.n_fill); P(dp.n_fade1); P(dp.n_black1); P(dp.fade_factor);
        P(total_lights_on);
        P(active_lights_factor);
        //P(total);
        dprintln("");
      }
#endif
    } else {
      led_rope->RawDrawPixels(Color3i::Black(), n_led);
    }



  }

  
  led_rope->RawCommitDraw();
}

void Painter::VegasVisualizer(const KeyboardState& keyboard,
                              const int* led_column_table, int led_column_table_length,
                              LedRopeInterface* led_rope) {
 
 led_rope->RawBeginDraw();
                           
 uint32_t skipped_lights = 0;
 for (int i = 0; i < led_column_table_length; ++i) {
    const Key& key = keyboard.keys_[i];
    uint32_t painted_lights = 0;
    
    // % of lights that are active.
    const float active_lights_factor = led_column_table[i] * sqrt(key.curr_color_.v_);
    const float inactive_lights_factor = 1.0f - active_lights_factor;
    const float taper_point_1 = inactive_lights_factor / 2.0f;
    const float taper_point_2 = taper_point_1 + active_lights_factor; 
    
    const int taper_idx_1 = static_cast<int>(floor(taper_point_1 * led_column_table[i]));
    const int taper_idx_2 = static_cast<int>(floor(taper_point_2 * led_column_table[i]));
    
    const Color3i c = key.curr_color_.ToRGB();
    
    for (int i = 0; i < taper_idx_1 / 2; ++i) {
      led_rope->RawDrawPixel(Color3i::Black());
      painted_lights++;
    }
    
    int length = taper_idx_2 - taper_idx_1;
    for (int i = 0; i < min(200, length); ++i) {
      led_rope->RawDrawPixel(c);
      painted_lights++;
    }

    length = led_column_table[i] - taper_idx_2;
    for (int i = 0; i < length; ++i) {
      led_rope->RawDrawPixel(Color3i::Black());
      painted_lights++;
    }
    skipped_lights += MAX(0, static_cast<int32_t>(led_column_table[i]) - static_cast<int32_t>(painted_lights));
  }
  
  for (uint32_t i = 0; i < skipped_lights; ++i) {
    led_rope->RawDrawPixel(Color3i::Black());
  }

  led_rope->RawCommitDraw();
}

void Painter::PaintBrightSurprise(
    const KeyboardState& keyboard,
    const int* led_column_table, int led_column_table_length,
    LedRopeInterface* led_rope) {

  led_rope->RawBeginDraw();
  int total_counted = 0;
  
  float r, g, b;
  r = g = b = 0;
  
  for (int i = 0; i < KeyboardState::kNumKeys; ++i) {
    const Key& key = keyboard.keys_[i];
    

    if (key.curr_color_.v_ > 0.0f) {
      const Color3i rgb = key.curr_color_.ToRGB();
      r += rgb.r_;
      g += rgb.g_;
      b += rgb.b_;
      ++total_counted;
    }
  }
  
  float denom = total_counted ? total_counted : 1;
  r /= denom;
  g /= denom;
  b /= denom;
  

  const Color3i rgb(r, g, b);
  
  for (int i = 0; i < led_column_table_length; ++i) {
    const int n = led_column_table[i];
    for (int i = 0; i < n; ++i) {
      led_rope->RawDrawPixel(rgb);
    }
  }
  led_rope->RawCommitDraw();
}

void Painter::PaintVuSpaceInvaders(uint32_t /*now_ms*/,
                                   const KeyboardState& keyboard,
                                   const int* led_column_table, int led_column_table_length,
                                   LedRopeInterface* led_rope) {
 led_rope->RawBeginDraw();
 
 Color3i black = Color3i::Black();
                           
 for (int i = 0; i < led_column_table_length; ++i) {
    const Key& key = keyboard.keys_[i];
    
    const int pixel_count = led_column_table[i];
    const int draw_pixel_count = ceil(pixel_count * sqrt(key.curr_color_.v_));
    
    const int black_pixel_count = pixel_count - draw_pixel_count;
    
    // If i is even
    if (i % 2 == 0) {
     for (int j = 0; j < black_pixel_count; ++j) {
        led_rope->RawDrawPixel(*led_rope->GetIterator(i));
      }
     for (int j = 0; j < draw_pixel_count; ++j) {
        led_rope->RawDrawPixel(black);
     }
    } else {

      for (int j = 0; j < draw_pixel_count; ++j) {
        led_rope->RawDrawPixel(black);
      }
      
      for (int j = 0; j < black_pixel_count; ++j) {
        led_rope->RawDrawPixel(*led_rope->GetIterator(i));
      }
    }
  }
  led_rope->RawCommitDraw();
}

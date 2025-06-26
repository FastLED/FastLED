

#include <Arduino.h>


#include <stdint.h>


#include "./color_mapper.h"

#include "./color.h"
#include "./util.h"

// Serves as the pallet for selecting a color.
struct ColorScheme {
  ColorScheme(const ColorHSV& c0,
              const ColorHSV& c1,
              const ColorHSV& c2,
              const ColorHSV& c3,
              const ColorHSV& c4,
              const ColorHSV& c5,
              const ColorHSV& c6,
              const ColorHSV& c7,
              const ColorHSV& c8,
              const ColorHSV& c9,
              const ColorHSV& c10,
              const ColorHSV& c11) {
    data[0] = c0;
    data[1] = c1;
    data[2] = c2;
    data[3] = c3;
    data[4] = c4;
    data[5] = c5;
    data[6] = c6;
    data[7] = c7;
    data[8] = c8;
    data[9] = c9;
    data[10] = c10;
    data[11] = c11;
  }
  ColorHSV data[12];
};

const ColorScheme& SelectColorScheme(int indx) {
  static ColorScheme color_schemes[] = {

    // Coda
    ColorScheme(
      ColorHSV(Color3i(0xff, 0x00, 0x00)), // C
      ColorHSV(Color3i(0x00, 0x80, 0xff)), // C
      ColorHSV(Color3i(0xff, 0xff, 0x00)), // D
      ColorHSV(Color3i(0x80, 0x00, 0xff)), // D#
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // E
      ColorHSV(Color3i(0xff, 0x00, 0x80)), // F
      ColorHSV(Color3i(0x00, 0xff, 0xff)), // F#
      ColorHSV(Color3i(0xff, 0x80, 0x00)), // G
      ColorHSV(Color3i(0x00, 0x00, 0xff)), // G#
      ColorHSV(Color3i(0x80, 0xff, 0x00)), // A
      ColorHSV(Color3i(0xff, 0x00, 0xff)), // A#
      ColorHSV(Color3i(0x00, 0xff, 0x80))
    ),

    // Frequency
    ColorScheme(
      ColorHSV(Color3i(0xfc, 0xff, 0x00)),  // C
      ColorHSV(Color3i(0x00, 0xff, 0x73)),  // C#
      ColorHSV(Color3i(0x00, 0xa7, 0xff)),
      ColorHSV(Color3i(0x00, 0x20, 0xff)),
      ColorHSV(Color3i(0x35, 0x00, 0xff)),
      ColorHSV(Color3i(0x56, 0x00, 0xb6)),
      ColorHSV(Color3i(0x4e, 0x00, 0x6c)),
      ColorHSV(Color3i(0x9f, 0x00, 0x00)),  // G
      ColorHSV(Color3i(0xdb, 0x00, 0x00)),
      ColorHSV(Color3i(0xff, 0x36, 0x00)),  // A
      ColorHSV(Color3i(0xff, 0xc1, 0x00)),
      ColorHSV(Color3i(0xbf, 0xff, 0x00))   // B
    ),
    
    // SCRIABIN
    ColorScheme(
      ColorHSV(Color3i(0xff, 0x00, 0x00)), // C
      ColorHSV(Color3i(0x8f, 0x00, 0xff)), // C#
      ColorHSV(Color3i(0xff, 0xff, 0x00)), // D
      ColorHSV(Color3i(0x71, 0x63, 0x95)), // D#
      ColorHSV(Color3i(0x4f, 0xa1, 0xc2)), // E
      ColorHSV(Color3i(0xc1, 0x01, 0x01)), // F
      ColorHSV(Color3i(0x00, 0x00, 0xff)), // F#
      ColorHSV(Color3i(0xff, 0x66, 0x00)), // G
      ColorHSV(Color3i(0x96, 0x00, 0xff)), // G#
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // A
      ColorHSV(Color3i(0x71, 0x63, 0x95)), // A#
      ColorHSV(Color3i(0x4f, 0xa1, 0xc2)) // B
    ),
    
    // LUIS BERTRAND CASTEL
    ColorScheme(
      ColorHSV(Color3i(0x00, 0x00, 0xff)), // C
      ColorHSV(Color3i(0x0d, 0x98, 0xba)), // C#
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // D
      ColorHSV(Color3i(0x80, 0x80, 0x00)), // D#
      ColorHSV(Color3i(0xff, 0xff, 0x00)), // E
      ColorHSV(Color3i(0xff, 0xd7, 0x00)), // F
      ColorHSV(Color3i(0xff, 0x5a, 0x00)), // F#
      ColorHSV(Color3i(0xff, 0x00, 0x00)), // G
      ColorHSV(Color3i(0xdc, 0x14, 0x3c)), // G#
      ColorHSV(Color3i(0x8f, 0x00, 0xff)), // A
      ColorHSV(Color3i(0x22, 0x00, 0xcd)), // A#
      ColorHSV(Color3i(0x5a, 0x00, 0x95)) // B
    ),
    
    // H VON HELMHOHOLTZ
    ColorScheme(
      ColorHSV(Color3i(0xff, 0xff, 0x06)), // C
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // C#
      ColorHSV(Color3i(0x21, 0x9e, 0xbd)), // D
      ColorHSV(Color3i(0x00, 0x80, 0xff)), // D#
      ColorHSV(Color3i(0x6f, 0x00, 0xff)), // E
      ColorHSV(Color3i(0x8f, 0x00, 0xff)), // F
      ColorHSV(Color3i(0xff, 0x00, 0x00)), // F#
      ColorHSV(Color3i(0xff, 0x20, 0x00)), // G
      ColorHSV(Color3i(0xff, 0x38, 0x00)), // G#
      ColorHSV(Color3i(0xff, 0x3f, 0x00)), // A
      ColorHSV(Color3i(0xff, 0x3f, 0x34)), // A#
      ColorHSV(Color3i(0xff, 0xa5, 0x00)) // B
    ),

    // ZIEVERINK
    ColorScheme(
      ColorHSV(Color3i(0x9a, 0xcd, 0x32)), // C
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // C#
      ColorHSV(Color3i(0x00, 0xdd, 0xdd)), // D
      ColorHSV(Color3i(0x00, 0x00, 0xff)), // D#
      ColorHSV(Color3i(0x6f, 0x00, 0xff)), // E
      ColorHSV(Color3i(0x8f, 0x00, 0xff)), // F
      ColorHSV(Color3i(0x7f, 0x1a, 0xe5)), // F#
      ColorHSV(Color3i(0xbd, 0x00, 0x20)), // G
      ColorHSV(Color3i(0xff, 0x00, 0x00)), // G#
      ColorHSV(Color3i(0xff, 0x44, 0x00)), // A
      ColorHSV(Color3i(0xff, 0xc4, 0x00)), // A#
      ColorHSV(Color3i(0xff, 0xff, 0x00)) // B
    ),
    
    // ROSICRUCIAN ORDER
    ColorScheme(
      ColorHSV(Color3i(0x9a, 0xcd, 0x32)), // C
      ColorHSV(Color3i(0x00, 0xff, 0x00)), // C#
      ColorHSV(Color3i(0x21, 0x9e, 0xbd)), // D
      ColorHSV(Color3i(0x00, 0x00, 0xff)), // D#
      ColorHSV(Color3i(0x8a, 0x2b, 0xe2)), // E
      ColorHSV(Color3i(0x8b, 0x00, 0xff)), // F
      ColorHSV(Color3i(0xf7, 0x53, 0x94)), // F#
      ColorHSV(Color3i(0xbd, 0x00, 0x20)), // G
      ColorHSV(Color3i(0xee, 0x20, 0x4d)), // G#
      ColorHSV(Color3i(0xff, 0x3f, 0x34)), // A
      ColorHSV(Color3i(0xff, 0xa5, 0x00)), // A#
      ColorHSV(Color3i(0xff, 0xff, 0x00)) // B
    ),
  };
  
  static const int n = sizeof(color_schemes) / sizeof(color_schemes[0]);
  indx = constrain(indx, 0, n - 1);
  
  return color_schemes[indx];
};

const ColorHSV SelectColor(int midi_note, float brightness, int color_selector_val) {
  const uint8_t fun_note = FundamentalNote(midi_note);  
  const ColorScheme& color_scheme = SelectColorScheme(color_selector_val);
  ColorHSV color = color_scheme.data[fun_note];
  color.v_ = brightness;
  return color;
}

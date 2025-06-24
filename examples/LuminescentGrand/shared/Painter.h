#ifndef PAINTER_H
#define PAINTER_H

#include "./Keyboard.h"
#include "./ApproximatingFunction.h"
#include "./util.h"
#include "./settings.h"
#include "./led_rope_interface.h"

struct Painter {
  
  enum VisState {
    kVUMidNote = 0,
    kColumnNote,
    kBlockNote,
    kVUNote,
    kVUSpaceInvaders,
    kVegas,
    kBrightSurprise,

    kNumVisStates,
  };

  ////////////////////////////////////////////////////
  static void Paint(uint32_t now_ms,
                    uint32_t delta_ms,
                    VisState vis_state,
                    KeyboardState* keyboard,
                    LedRopeInterface* light_rope);
private:
  static void PaintVuNotes(uint32_t now_ms,
                           const KeyboardState& keyboard,
                           const int* led_column_table, int led_column_table_length,
                           LedRopeInterface* led_rope);
  
  static void PaintVuMidNotesFade(uint32_t delta_ms,
                                  const KeyboardState& keyboard,
                                  const int* led_column_table, int led_column_table_length,
                                  LedRopeInterface* led_rope);
  
  // This is a crazy effect, lets keep this around.
  static void VegasVisualizer(const KeyboardState& keyboard,
                              const int* led_column_table, int led_column_table_length,
                              LedRopeInterface* led_rope);
  
  static void PaintBrightSurprise(const KeyboardState& keyboard,
                                  const int* led_column_table, int led_column_table_length,
                                  LedRopeInterface* led_rope);
  
  
          
  static void PaintVuSpaceInvaders(uint32_t now_ms,
                                   const KeyboardState& keyboard,
                                   const int* led_column_table, int led_column_table_length,
                                   LedRopeInterface* led_rope);
};

#endif  // PAINTER_H

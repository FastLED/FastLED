/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "FastLED", "index.html", [
    [ "FastLED - The Universal LED Library", "index.html", "index" ],
    [ "FastLED Source Tree (<tt>src/</tt>)", "d3/dcc/md__r_e_a_d_m_e.html", [
      [ "Table of Contents", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md75", null ],
      [ "Overview and Quick Start", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md77", [
        [ "What lives in src/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md78", null ],
        [ "Include policy", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md79", null ]
      ] ],
      [ "Directory Map (7 major areas)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md81", [
        [ "Public headers and glue", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md82", null ],
        [ "Core foundation: fl/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md83", null ],
        [ "Effects and graphics: fx/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md84", null ],
        [ "Platforms and HAL: platforms/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md85", null ],
        [ "Sensors and input: sensors/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md86", null ],
        [ "Fonts and assets: fonts/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md87", null ],
        [ "Third‑party and shims: third_party/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md88", null ]
      ] ],
      [ "Quick Usage Examples", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md90", [
        [ "Classic strip setup", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md91", null ],
        [ "2D matrix with fl::Leds + XYMap", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md92", null ],
        [ "Resampling pipeline (downscale/upscale)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md93", null ],
        [ "JSON UI (WASM)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md94", null ]
      ] ],
      [ "Deep Dives by Area", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md96", [
        [ "Public API surface", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md97", null ],
        [ "Core foundation cross‑reference", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md98", null ],
        [ "FX engine building blocks", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md99", null ],
        [ "Platform layer and stubs", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md100", null ],
        [ "WASM specifics", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md101", null ],
        [ "Testing and compile‑time gates", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md102", null ]
      ] ],
      [ "Guidance for New Users", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md104", null ],
      [ "Guidance for C++ Developers", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md105", null ]
    ] ],
    [ "FastLED Examples Agent Guidelines", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html", [
      [ "🚨 CRITICAL: .INO FILE CREATION RULES", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md109", [
        [ "⚠️ THINK BEFORE CREATING .INO FILES ⚠️", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md110", null ],
        [ "🚫 WHEN NOT TO CREATE .INO FILES:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md111", null ],
        [ "✅ WHEN TO CREATE .INO FILES:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md112", [
          [ "Temporary Testing (.ino)", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md113", null ],
          [ "Significant New Feature Examples", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md114", null ]
        ] ],
        [ "📋 CREATION CHECKLIST:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md115", null ],
        [ "🔍 REVIEW CRITERIA:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md116", null ],
        [ "❌ EXAMPLES OF WHAT NOT TO CREATE:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md117", null ],
        [ "✅ EXAMPLES OF JUSTIFIED CREATIONS:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md118", null ],
        [ "🧹 CLEANUP RESPONSIBILITY:", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md119", null ]
      ] ],
      [ "Code Standards for Examples", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md120", [
        [ "Use fl:: Namespace (Not std::)", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md121", null ],
        [ "Memory Management", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md122", null ],
        [ "Debug Printing", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md123", null ],
        [ "No Emoticons or Emojis", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md124", null ],
        [ "JSON Usage - Ideal API Patterns", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md125", null ]
      ] ],
      [ "Example Compilation Commands", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md126", [
        [ "Platform Compilation", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md127", null ],
        [ "WASM Compilation", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md128", null ],
        [ "WASM Testing Requirements", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md129", null ]
      ] ],
      [ "Compiler Warning Suppression", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md130", null ],
      [ "Exception Handling", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md131", null ],
      [ "Memory Refresh Rule", "df/dbc/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_a_g_e_n_t_s.html#autotoc_md132", null ]
    ] ],
    [ "NoiseRing Enhanced Design Document", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html", [
      [ "Overview", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md182", null ],
      [ "Core Features", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md183", [
        [ "Automatic Cycling", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md184", null ],
        [ "User Interface Controls", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md185", null ]
      ] ],
      [ "10 Noise Variations - Detailed Algorithmic Implementation", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md186", [
        [ "\"Cosmic Swirl\" - Enhanced Perlin Flow", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md187", null ],
        [ "\"Electric Storm\" - High-Frequency Chaos", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md188", null ],
        [ "\"Lava Lamp\" - Slow Blobby Movement", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md189", null ],
        [ "\"Digital Rain\" - Matrix Cascade", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md190", null ],
        [ "\"Plasma Waves\" - FEATURED IMPLEMENTATION", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md191", null ],
        [ "\"Glitch City\" - Chaotic Digital Artifacts", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md192", null ],
        [ "\"Ocean Depths\" - Underwater Currents", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md193", null ],
        [ "\"Fire Dance\" - Upward Flame Simulation", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md194", null ],
        [ "\"Nebula Drift\" - Cosmic Cloud Simulation", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md195", null ],
        [ "\"Binary Pulse\" - Digital Heartbeat", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md196", null ]
      ] ],
      [ "5 Color Palettes", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md197", [
        [ "\"Sunset Boulevard\"", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md198", null ],
        [ "\"Ocean Breeze\"", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md199", null ],
        [ "\"Neon Nights\"", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md200", null ],
        [ "\"Forest Whisper\"", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md201", null ],
        [ "\"Galaxy Express\"", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md202", null ]
      ] ],
      [ "Implementation Strategy", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md203", [
        [ "Core Mathematical Framework", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md204", null ],
        [ "Enhanced Control System with Smooth Transitions", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md205", null ],
        [ "Data Structures and UI Integration", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md206", null ],
        [ "Integration with Existing Framework", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md207", null ]
      ] ],
      [ "Technical Considerations", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md208", [
        [ "Performance Optimization", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md209", null ],
        [ "Memory Management", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md210", null ],
        [ "Mathematical Precision", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md211", null ],
        [ "User Experience", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md212", null ]
      ] ],
      [ "First Pass Implementation: Plasma Waves", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md213", [
        [ "Why Start with Plasma Waves?", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md214", null ],
        [ "Development Strategy", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md215", null ],
        [ "Testing and Validation", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md216", null ],
        [ "Incremental Development Plan", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md217", null ]
      ] ],
      [ "Advanced Graphics Techniques Demonstrated", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md218", [
        [ "Wave Interference Mathematics", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md219", null ],
        [ "Color Theory Implementation", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md220", null ],
        [ "Performance Optimization Strategies", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md221", null ]
      ] ],
      [ "Future Enhancements", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md222", [
        [ "Advanced Features", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md223", null ],
        [ "Platform Extensions", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md224", null ],
        [ "Algorithm Enhancements", "dc/def/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_fx_noise_ring_2_n_o_i_s_e___r_i_n_g___i_d_e_a_s.html#autotoc_md225", null ]
      ] ]
    ] ],
    [ "Platform Porting Guide", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html", [
      [ "Fast porting for a new board on existing hardware", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md260", [
        [ "Setting up the basic files/folders", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md261", null ],
        [ "Porting fastpin.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md262", null ],
        [ "Porting fastspi.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md263", null ],
        [ "Porting clockless.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md264", null ]
      ] ]
    ] ],
    [ "Todo List", "dd/da0/todo.html", null ],
    [ "Deprecated List", "da/d58/deprecated.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", "functions_type" ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", "functions_eval" ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", "globals_vars" ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", "globals_defs" ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"d0/d4b/classfl_1_1_video_a407205594718b624b2ec288c9c18bbbe.html#a407205594718b624b2ec288c9c18bbbe",
"d0/d98/class_color_palette_manager_ad4418fbad41db98705a0e599a2e3d749.html#ad4418fbad41db98705a0e599a2e3d749",
"d0/dce/classfl_1_1_x_y_map_a464acdd5a6ce465a32fba1fda9862fa5.html#a464acdd5a6ce465a32fba1fda9862fa5",
"d0/dee/struct_pixel_controller_aad6f8ce8a3aa9429025e67050e312929.html#aad6f8ce8a3aa9429025e67050e312929",
"d1/d56/classfl_1_1_ptr_a4b6b952b8d8e861cdb64cc1a3672ce8d.html#a4b6b952b8d8e861cdb64cc1a3672ce8d",
"d1/d74/classfl_1_1shared__ptr_a38cc8143d8d5fb96079309306196b677.html#a38cc8143d8d5fb96079309306196b677",
"d1/dc0/classfl_1_1_byte_stream_memory_a425f737b4840ff4054f4400b4ab2e23f.html#a425f737b4840ff4054f4400b4ab2e23f",
"d1/ddf/align_8h.html",
"d2/d1f/classfl_1_1_catmull_rom_path_ae5bc34153257d7cb49ca32008f3109db.html#ae5bc34153257d7cb49ca32008f3109db",
"d2/d78/classfl_1_1_screen_map_a0f7699554fd9fe34cd58b5d781203405.html#a0f7699554fd9fe34cd58b5d781203405",
"d2/db2/classfl_1_1_pacifica_abcad182fe890999b9407d690fad1381a.html#abcad182fe890999b9407d690fad1381a",
"d2/de0/namespacefl_1_1printf__detail_ac9026cabf78c126af3df04b25bccad80.html#ac9026cabf78c126af3df04b25bccad80",
"d3/d40/classfl_1_1_audio_reactive_a1d04a627e9ba859efb9b14b48605c1b8.html#a1d04a627e9ba859efb9b14b48605c1b8",
"d3/d5c/namespacefl_1_1anonymous__namespace_02audio_8cpp_03_a63dcc803fe703f176b52e25bde0c3adf.html#a63dcc803fe703f176b52e25bde0c3adf",
"d3/d92/classfl_1_1scoped__array_a7e83544ab19ec9c3683fabb90cf951c3.html#a7e83544ab19ec9c3683fabb90cf951c3",
"d3/dca/classfl_1_1_x_y_raster_sparse___c_r_g_b_af46be2cd25b5a9dadf4c079a711c8ae2.html#af46be2cd25b5a9dadf4c079a711c8ae2",
"d4/d13/classfl_1_1_json_a2d493982196c474118851ba938057a33.html#a2d493982196c474118851ba938057a33",
"d4/d36/namespacefl.html#a3d2a6649d5dd2c57e673d06b034656d5",
"d4/d36/namespacefl_a3375a843e45a8c85394a7524b1956fc0.html#a3375a843e45a8c85394a7524b1956fc0",
"d4/d36/namespacefl_a964d9f51bd69ec7269946cefb6507088.html#a964d9f51bd69ec7269946cefb6507088",
"d4/d3a/classfl_1_1_u_i_group_a5162d9d84849036e0e16a79c06945c33.html#a5162d9d84849036e0e16a79c06945c33",
"d4/d8d/classfl_1_1_string_formatter_a2b3dab310854c4235985f5629454312f.html#a2b3dab310854c4235985f5629454312f",
"d4/dca/classfl_1_1array_a2d9d34e3be2fc2d4d99aab328e8ba038.html#a2d9d34e3be2fc2d4d99aab328e8ba038",
"d4/df2/classfl_1_1_luminova.html#ad5cb32ae4c07a537e2d5d4aa291c3302",
"d5/d2b/structfl_1_1default__delete_a29f061f8e51d198ff649f1aa8dafc673.html#a29f061f8e51d198ff649f1aa8dafc673",
"d5/d5d/structfl_1_1vec2_aa2db78760b0afb1f5dd16caec2a93c6a.html#aa2db78760b0afb1f5dd16caec2a93c6a",
"d5/d86/classfl_1_1_file_handle_a123224c428ab62bf7151d5862e2a733e.html#a123224c428ab62bf7151d5862e2a733e",
"d5/db7/classfl_1_1_transform_float_impl_a66787dd9778800d1921e1030066c58cd.html#a66787dd9778800d1921e1030066c58cd",
"d6/d18/classfl_1_1_l_u_t_a7882b0d75665abd32106dc0f425ef8c2.html#a7882b0d75665abd32106dc0f425ef8c2",
"d6/d39/structfl_1_1is__integral_3_01short_01_4.html",
"d6/dc0/ui__impl_8h_ad67ab3bdad16257795f36ee32534183d.html#ad67ab3bdad16257795f36ee32534183d",
"d7/d35/_fx_sd_card_8ino_a4fc01d736fe50cf5b977f755b675f11d.html#a4fc01d736fe50cf5b977f755b675f11d",
"d7/d82/struct_c_r_g_b_a3b96dc7b062f42c53f11ca27e4c5adfd.html#a3b96dc7b062f42c53f11ca27e4c5adfd",
"d7/d84/cstddef_8h.html",
"d7/dda/structfl_1_1_int_conversion_visitor_af2da9963a1b28350ca1e7d6b20d807fd.html#af2da9963a1b28350ca1e7d6b20d807fd",
"d8/d11/classfl_1_1_catmull_rom_params_a5255601e58a76a89ded919cf8d5d103c.html#a5255601e58a76a89ded919cf8d5d103c",
"d8/dab/_octo_w_s2811_demo_8h_a4fc01d736fe50cf5b977f755b675f11d.html#a4fc01d736fe50cf5b977f755b675f11d",
"d8/dd0/midi___defs_8h_a60199bde936cb246c54a6f895a852bf5.html#a60199bde936cb246c54a6f895a852bf5",
"d8/de4/classfl_1_1_slice_aa2c7b7e34b3bc0180abbcd706cbc7e7e.html#aa2c7b7e34b3bc0180abbcd706cbc7e7e",
"d9/d33/classfl_1_1_object_f_l_e_d_acbf4e642637c1caaab67e42ee95b00dd.html#acbf4e642637c1caaab67e42ee95b00dd",
"d9/d82/classfl_1_1_u_i_title_a63501411ae1747af0f49a880585af4fb.html#a63501411ae1747af0f49a880585af4fb",
"d9/df4/classfl_1_1_x_y_path_acd5085c9cf75a7a7448ed22f9e529762.html#acd5085c9cf75a7a7448ed22f9e529762",
"da/d46/structfl_1_1_error_a97d719109e638cccb6d4d4f33f851f38.html#a97d719109e638cccb6d4d4f33f851f38",
"da/d89/crgb__hsv16_8cpp_a2d8dda85477a00deab830effc5159d33.html#a2d8dda85477a00deab830effc5159d33",
"da/dc7/advanced_8h_ad7b7eb0b3e04af91bdd64715226c3c68.html#ad7b7eb0b3e04af91bdd64715226c3c68",
"da/df8/structfl_1_1_u_i_slider_1_1_listener_a38793a1ec622a954d9fa39e0fe350694.html#a38793a1ec622a954d9fa39e0fe350694",
"db/d58/group___fractional_types.html#de/d28/struct_i_e_e_e754binary32__t_8____unnamed0____",
"db/d97/_fast_l_e_d_8h_ac06549dc4a351e2c8f1dda8ae81f7926.html#ac06549dc4a351e2c8f1dda8ae81f7926a3b67cd8706aa03d1d3e662a6199a87e2",
"db/ddd/classfl_1_1_heap_vector_ad6c709ec7c24001303e05b2116c4cc50.html#ad6c709ec7c24001303e05b2116c4cc50",
"dc/d3e/group___dimming_ga3f94d2455e0aa92133f77af8747b5914.html#ga3f94d2455e0aa92133f77af8747b5914",
"dc/d4d/_wasm_screen_coords_8ino.html",
"dc/d7c/structfl_1_1vec3_a9560a53faac5c070e5f3ba147dffb310.html#a9560a53faac5c070e5f3ba147dffb310",
"dc/d96/classfl_1_1_str_n_a9343fec5bc90719387e46abd5619491a.html#a9343fec5bc90719387e46abd5619491a",
"dc/dd3/_esp32_s3_i2_s_demo_8h_adad67fe595ea440c8f8247ec2cddf070.html#adad67fe595ea440c8f8247ec2cddf070",
"dc/dfe/classfl_1_1deque_a7ee09406b19f270546d50d14a6387ec9.html#a7ee09406b19f270546d50d14a6387ec9",
"dd/d29/classfl_1_1_point_path_a2efcd8c3d8d0580e1d16aea1cdc43516.html#a2efcd8c3d8d0580e1d16aea1cdc43516",
"dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md264",
"dd/dd8/classfl_1_1_optional_a6ac21e2492ec9412ec8afc5db6fd979a.html#a6ac21e2492ec9412ec8afc5db6fd979a",
"de/d2b/_first_light_8ino_a1b4f26a01e11d7eb2848bc41b0f6dd9d.html#a1b4f26a01e11d7eb2848bc41b0f6dd9d",
"de/d63/structfl_1_1_f_f_t___args_aee3b8b6f26499727b0dc418ef42a33d0.html#aee3b8b6f26499727b0dc418ef42a33d0",
"de/db5/classfl_1_1_twinkle_fox_a7b4a385bedd751f63937469cc850ee1a.html#a7b4a385bedd751f63937469cc850ee1a",
"de/ded/bitswap_8h.html#aed576f78d45069eebafdfda8a42602f5",
"df/d37/classfl_1_1_file_system_a2350b8cc8235a759724d1356b2fb0359.html#a2350b8cc8235a759724d1356b2fb0359",
"df/d5e/classanimartrix__detail_1_1_a_n_i_mart_r_i_x_ab46628347b5ab16e89c2f48f03c57ce0.html#ab46628347b5ab16e89c2f48f03c57ce0",
"df/d84/avr__test_8h_source.html",
"df/dc6/classgen_1_1_point.html",
"functions_func.html",
"index.html#autotoc_md306"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
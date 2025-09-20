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
"d0/d6e/structfl_1_1_hash_3_01double_01_4_a7b4d2bae732af296f463f5519321b9b4.html#a7b4d2bae732af296f463f5519321b9b4",
"d0/db5/classfl_1_1_gradient_inlined_a5e55c9d8640c6842e2e4a370ba3ab0ec.html#a5e55c9d8640c6842e2e4a370ba3ab0ec",
"d0/de1/classfl_1_1_audio_sample_a669008e0891c3bfff60a3cd847b57e4a.html#a669008e0891c3bfff60a3cd847b57e4a",
"d1/d3a/_r_g_b_w_emulated_8ino_a6ac4e89ba1c11811ee992404edc59ded.html#a6ac4e89ba1c11811ee992404edc59ded",
"d1/d5c/classfl_1_1_x_y_raster_u8_sparse_adcaa69da1a8e3e7daa421bb1b648dbb1.html#adcaa69da1a8e3e7daa421bb1b648dbb1",
"d1/d9f/classfl_1_1_gielis_curve_path.html",
"d1/dd6/classfl_1_1weak__ptr_a8ce181894e04a9edab5dad1df2b32990.html#a8ce181894e04a9edab5dad1df2b32990",
"d1/dff/fft__impl_8cpp_a0b7d3509aaa80be4c33c5d503be7aded.html#a0b7d3509aaa80be4c33c5d503be7aded",
"d2/d5e/classfl_1_1_byte_stream_ab7dd7c75ac8e22f0b44e08421422c217.html#ab7dd7c75ac8e22f0b44e08421422c217",
"d2/da2/structfl_1_1detail_1_1_control_block_ae99dd5c934187f96ac116e6feb149c93.html#ae99dd5c934187f96ac116e6feb149c93",
"d2/dbc/classfl_1_1_x_y_path_renderer_ae58cfc59bc00e1d1a4085270f789edc5.html#ae58cfc59bc00e1d1a4085270f789edc5",
"d3/d25/classfl_1_1_digital_pin_impl_a52b7c62cb136adbb22283fc1345d6918.html#a52b7c62cb136adbb22283fc1345d6918",
"d3/d54/structfl_1_1_matrix3x3f_abd4d4643a26c670edaec8fe533bed475.html#abd4d4643a26c670edaec8fe533bed475",
"d3/d74/__kiss__fft__guts_8h_a6e09db0ed1114fa9513498fce8b991d7.html#a6e09db0ed1114fa9513498fce8b991d7",
"d3/dc4/structfl_1_1type__rank_a5cd851b6a194004998bd9dc03e8531d5.html#a5cd851b6a194004998bd9dc03e8531d5",
"d4/d06/curr_8h_aa0514ec020f07c5ed4f3368ad9d2b1d8.html#aa0514ec020f07c5ed4f3368ad9d2b1d8",
"d4/d13/classfl_1_1_json_ad80033e22c621192a4dd46e25fba6205.html#ad80033e22c621192a4dd46e25fba6205",
"d4/d36/namespacefl_a0222a86d3eefd0e9bd026cdb95331675.html#a0222a86d3eefd0e9bd026cdb95331675",
"d4/d36/namespacefl_a5a8a33cdb0434211edc66189968155ea.html#a5a8a33cdb0434211edc66189968155ea",
"d4/d36/namespacefl_ad72c4923c127f74a54ab7e156b28d5d5.html#ad72c4923c127f74a54ab7e156b28d5d5",
"d4/d60/classfl_1_1_fx_engine_a6120eaf2a428d0e1f95177a13373992b.html#a6120eaf2a428d0e1f95177a13373992b",
"d4/dc0/structfl_1_1detail_1_1_control_block_base_aff42244e7d34540a911554ded29f9826.html#aff42244e7d34540a911554ded29f9826",
"d4/dd2/framebuffer_8h.html",
"d5/d1b/examples_2_async_2async_8h_afe461d27b9c48d5921c00d521181f12f.html#afe461d27b9c48d5921c00d521181f12f",
"d5/d4b/structfl_1_1_str_stream_helper_3_01char_01_4.html",
"d5/d77/_fx_noise_ring_8h_a86e087d43e1859591dda07be6a9fef12.html#a86e087d43e1859591dda07be6a9fef12",
"d5/d94/classfl_1_1_wave_simulation2_d___real_aec98e360ebbfea843b0aca51df0deedf.html#aec98e360ebbfea843b0aca51df0deedf",
"d5/df2/group___timekeeping_ga8057578b51f0a5cff337fb2fd5521de7.html#ga8057578b51f0a5cff337fb2fd5521de7",
"d6/d2e/classfl_1_1unique__ptr_a13d1611583494132d58adc9a9b9142ed.html#a13d1611583494132d58adc9a9b9142ed",
"d6/d7f/classfl_1_1_fx_layer_a73efe876c7d5871c28762bc148785619.html#a73efe876c7d5871c28762bc148785619",
"d6/def/class_u_i_group_a76d59b25811df5a51e5c07d87f1e8404.html#a76d59b25811df5a51e5c07d87f1e8404",
"d7/d58/raster__sparse_8cpp.html",
"d7/d82/struct_c_r_g_b_aeb40a08b7cb90c1e21bd408261558b99.html#aeb40a08b7cb90c1e21bd408261558b99a4423a951375268e08f036d80d04301e3",
"d7/da1/old_8h_a0209867ce457e38f7ec511498de87c9a.html#a0209867ce457e38f7ec511498de87c9a",
"d7/de6/class_x_y_map_acf5f49fc3b4044cad5b3a83b9ab5cfea.html#acf5f49fc3b4044cad5b3a83b9ab5cfea",
"d8/d4e/blend_8h_source.html",
"d8/dc1/classfl_1_1_wave_simulation1_d_a735a76a9d6713618d54200baf26505e4.html#a735a76a9d6713618d54200baf26505e4",
"d8/dd0/midi___defs_8h_aa1cfd7d9d1fe50ec27b566e854e98263.html#aa1cfd7d9d1fe50ec27b566e854e98263a9bb7dfbb53f5b4a0557ed32e990296e4",
"d8/df5/classfl_1_1_animartrix_a7ca4364578c43ddd5e301238e33f789e.html#a7ca4364578c43ddd5e301238e33f789e",
"d9/d41/classfl_1_1_noise_palette_a3edcb0dfbc5280fffcd9776bc59f6899.html#a3edcb0dfbc5280fffcd9776bc59f6899",
"d9/dc9/console__font__6x8_8h_a06d38d355126315743faa55816332cf3.html#a06d38d355126315743faa55816332cf3",
"da/d06/classfl_1_1_pir_low_level_a74709d09faa737ade2ba3a45156030ce.html#a74709d09faa737ade2ba3a45156030ce",
"da/d47/classfl_1_1_wave_simulation2_d_a2f98e67e8d31b0c6546533937116eea2.html#a2f98e67e8d31b0c6546533937116eea2",
"da/d8c/classfl_1_1_hash_set_a7d736d66f529089fa3d9efe3b4589781.html#a7d736d66f529089fa3d9efe3b4589781",
"da/dc9/platforms_8h_source.html",
"da/df9/classfl_1_1_wave_simulation1_d___real_a07169506b17b5bf5f98253f278241115.html#a07169506b17b5bf5f98253f278241115",
"db/d58/group___fractional_types.html#aa0012c437f9cb0d4f78aa3072fc8781a",
"db/d97/_fast_l_e_d_8h_a5dd5101b1f189300219b06aaaaacff2e.html#a5dd5101b1f189300219b06aaaaacff2ea272baf8a63080477fe5bffb2937db7c0",
"db/ddd/classfl_1_1_heap_vector_aa34e1f65ff2463cf4e03de647b85438f.html#aa34e1f65ff2463cf4e03de647b85438f",
"dc/d2b/classfl_1_1_blend2d_aac59daf24beddaada27a92ea7d8e4044.html#aac59daf24beddaada27a92ea7d8e4044",
"dc/d4b/classfl_1_1istream__real_a9dca9b9e297824879c2f3696bc4f00c7.html#a9dca9b9e297824879c2f3696bc4f00c7",
"dc/d7c/structfl_1_1vec3_a5bde5ab92ec77fe57df7fe9789348869.html#a5bde5ab92ec77fe57df7fe9789348869",
"dc/d96/classfl_1_1_str_n_a6f3fee5553f985b5b5d995e7232a4a71.html#a6f3fee5553f985b5b5d995e7232a4a71",
"dc/dc8/classfl_1_1_line_path_params_ac4e58fffdcc2aae2a7bd04eafc739139.html#ac4e58fffdcc2aae2a7bd04eafc739139",
"dc/dfe/classfl_1_1deque_a5265ab618714d88d7d26e55c5d0e83cb.html#a5265ab618714d88d7d26e55c5d0e83cb",
"dd/d28/classfl_1_1_red_black_tree_ad3a60570419c729ef6f1b970fabd066b.html#ad3a60570419c729ef6f1b970fabd066b",
"dd/d95/console__font__5x8_8h_afaad2e166b573d981d17e55d455aa541.html#afaad2e166b573d981d17e55d455aa541",
"dd/dd8/classfl_1_1_optional_a274cc6e262a25e7e099b86a37bbd5d46.html#a274cc6e262a25e7e099b86a37bbd5d46",
"de/d1d/class_c_every_n_time_a9e72ce7dcf1e5b321ec9fa61f6c4fbd5.html#a9e72ce7dcf1e5b321ec9fa61f6c4fbd5",
"de/d62/class_serial_m_i_d_i_a21bcf9bc4a2b9225ad6d7e554a05ba1e.html#a21bcf9bc4a2b9225ad6d7e554a05ba1e",
"de/da4/group___noise_fill.html",
"de/ded/bitswap_8h.html#a0f1ca41b2037c07c21ec6a2c72565193",
"df/d1d/classfl_1_1_phyllotaxis_path_abf1417535e22faf4cbc19a911fe984a1.html#abf1417535e22faf4cbc19a911fe984a1",
"df/d5e/classanimartrix__detail_1_1_a_n_i_mart_r_i_x_a48af607ca217bd59cdd62686888f2cd0.html#a48af607ca217bd59cdd62686888f2cd0",
"df/d6a/class_midi_interface_a410065f0b74649571a24632165aab9a1.html#a410065f0b74649571a24632165aab9a1",
"df/dab/classfl_1_1_null_file_handle_a478b08479df0919e7699be234f3e3c10.html#a478b08479df0919e7699be234f3e3c10",
"dir_16996705dca5e5845caca64f4257e696.html",
"globals_defs.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
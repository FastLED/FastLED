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
    [ "FastLED - The Universal LED Library for Embedded and Arduino", "index.html", "index" ],
    [ "Audio Detector Testing Guide", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html", [
      [ "Development Workflow: Tune, Then Delete", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md49", null ],
      [ "Overview", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md50", null ],
      [ "Tier 1: Synthetic Signal Testing (Permanent)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md51", [
        [ "Signal Generators", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md52", null ],
        [ "Test Patterns", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md53", null ],
        [ "Expected Results (Synthetic)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md54", null ],
        [ "Amplitude Sweep", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md55", null ]
      ] ],
      [ "Tier 2: Real-World Audio Calibration (Development Only)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md56", [
        [ "Step 1: Source Audio Samples", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md57", null ],
        [ "Step 2: Establish Ground Truth", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md58", null ],
        [ "Step 3: Trim, Normalize, and Mix", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md59", null ],
        [ "Step 4: Create Level Variants", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md60", null ],
        [ "Step 5: Write Temporary C++ Tests", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md61", null ],
        [ "Step 6: Set Appropriate Thresholds", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md62", null ],
        [ "Step 7: Transfer to Synthetic Tests and Clean Up", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md63", null ]
      ] ],
      [ "Tier 3: Sample Rate Invariance Testing", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md64", [
        [ "Purpose", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md65", null ],
        [ "Method", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md66", null ],
        [ "Expected Results", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md67", null ]
      ] ],
      [ "The Synthetic-vs-Real Gap", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md68", null ],
      [ "Optimization: Temporal Spectral Variance", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md69", [
        [ "The Problem", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md70", null ],
        [ "The Temporal Variance Idea", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md71", null ],
        [ "Implementation: SpectralVariance Filter", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md72", null ],
        [ "Results on Real Audio", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md73", null ],
        [ "Why a Boost, Not a Primary Feature", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md74", null ],
        [ "Feature Distribution in Real Audio (0dB Voice+Guitar Mix)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md75", null ],
        [ "Feature Distribution in Real Audio (3-Way Voice+Guitar+Drums Mix)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md76", null ]
      ] ],
      [ "How the Test Audio Was Created", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md77", [
        [ "Source Audio", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md78", null ],
        [ "Trim and Normalize", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md79", null ],
        [ "Mix with Offset", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md80", null ],
        [ "3b. 3-Way Mix (Voice+Guitar+Drums)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md81", null ],
        [ "Level Variants", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md82", null ],
        [ "Encode to MP3", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md83", null ],
        [ "Test Region Layout", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md84", null ]
      ] ],
      [ "Example Directive: Improving an Audio Detector", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md85", [
        [ "Template", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md86", null ],
        [ "Worked Example: VocalDetector Enhancement", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md87", [
          [ "Problem", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md88", null ],
          [ "Changes", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md89", [
            [ "Synthetic signal generators added to test helpers", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md90", null ],
            [ "Two new spectral features", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md91", null ],
            [ "Improved formant detection", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md92", null ],
            [ "Five-feature weighted confidence scoring", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md93", null ],
            [ "Seven new test cases", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md94", null ],
            [ "Real-world MP3 calibration (Tier 2)", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md95", null ],
            [ "Results and lessons learned", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md96", null ]
          ] ],
          [ "Risk Assessment", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md97", null ]
        ] ]
      ] ],
      [ "Future Work", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md98", null ],
      [ "Running Tests", "d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md99", null ]
    ] ],
    [ "FLED v1 Container Format", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html", [
      [ "File Layout", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md153", null ],
      [ "Pixel Format Enum", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md154", null ],
      [ "JSON Envelope", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md155", null ],
      [ "Source Color Metadata", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md156", [
        [ "Default tuple", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md157", null ],
        [ "Color classes by pixel format", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md158", null ],
        [ "Playback with and without colour management (B6)", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md159", null ],
        [ "Declaration verdicts vs. consumer policy", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md160", null ],
        [ "Validation rules", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md161", null ],
        [ "Forward compatibility", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md162", null ]
      ] ],
      [ "Frame Payload", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md163", null ],
      [ "Growth Notes", "da/d9b/md_fl_2fled_2_f_l_e_d___f_o_r_m_a_t.html#autotoc_md164", null ]
    ] ],
    [ "RGB8 Performance Guide", "d8/d7d/_c_r_g_b_performance_guide.html", [
      [ "Inline (Single-Pixel) Patterns", "d8/d7d/_c_r_g_b_performance_guide.html#inline_patterns", null ],
      [ "Bulk Operation Patterns", "d8/d7d/_c_r_g_b_performance_guide.html#bulk_patterns", null ],
      [ "Performance Characteristics", "d8/d7d/_c_r_g_b_performance_guide.html#performance_characteristics", null ]
    ] ],
    [ "fl/remote Architecture", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html", [
      [ "Overview", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md312", null ],
      [ "Layer Architecture", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md313", null ],
      [ "Layers", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md314", [
        [ "Transport Layer (fl/remote/transport/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md315", [
          [ "Serial Transport (transport/serial/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md316", null ],
          [ "HTTP Streaming Transport (transport/http/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md317", null ]
        ] ],
        [ "Composition Layer (Factory Functions in transport/serial.h)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md318", null ],
        [ "Application Layer (fl/remote/remote.h)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md320", null ]
      ] ],
      [ "Design Rationale", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md321", [
        [ "Why Separate Transport and Application Logic?", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md322", null ],
        [ "Zero-Copy Optimization", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md323", null ],
        [ "Example: Using HTTP Streaming Transport", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md324", null ],
        [ "Example: Custom JSONL Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md325", null ]
      ] ],
      [ "File Organization", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md326", null ],
      [ "Protocol Formats", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md327", [
        [ "Serial Transport Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md328", null ],
        [ "HTTP Streaming Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md329", [
          [ "SYNC Mode (Immediate Response)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md330", null ],
          [ "ASYNC Mode (ACK + Result)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md331", null ],
          [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md332", null ]
        ] ]
      ] ],
      [ "RPC Modes and Response Handling", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md333", [
        [ "SYNC Mode (Default)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md334", null ],
        [ "ASYNC Mode", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md335", null ],
        [ "ASYNC_STREAM Mode", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md336", null ],
        [ "ResponseSend API", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md337", null ]
      ] ],
      [ "Migration Notes", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md338", [
        [ "From Old API (fastled3)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md339", null ],
        [ "From Serial to HTTP Transport", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md340", null ]
      ] ],
      [ "Sequence Diagrams", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md341", [
        [ "SYNC Mode (Immediate Response)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md342", null ],
        [ "ASYNC Mode (ACK + Result)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md343", null ],
        [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md344", null ]
      ] ],
      [ "Component Diagram", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md345", null ]
    ] ],
    [ "HTTP Streaming RPC Protocol Specification", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html", [
      [ "Overview", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md406", null ],
      [ "Protocol Version", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md407", null ],
      [ "HTTP Request Format (Client → Server)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md409", [
        [ "Headers", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md410", null ],
        [ "JSON-RPC Request Format", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md411", null ]
      ] ],
      [ "HTTP Response Format (Server → Client)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md413", [
        [ "Headers", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md414", null ],
        [ "JSON-RPC Response Format", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md415", [
          [ "Success Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md416", null ],
          [ "Error Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md417", null ]
        ] ]
      ] ],
      [ "RPC Modes", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md419", [
        [ "SYNC Mode (Immediate Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md420", null ],
        [ "ASYNC Mode (ACK + Later Result)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md422", null ],
        [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md424", null ]
      ] ],
      [ "Heartbeat Protocol", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md426", [
        [ "Ping (Request)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md427", null ],
        [ "Pong (Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md428", null ],
        [ "Heartbeat Timing", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md429", null ]
      ] ],
      [ "Connection Lifecycle", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md431", [
        [ "Initial Connection", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md432", null ],
        [ "Normal Operation", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md433", null ],
        [ "Reconnection", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md434", null ]
      ] ],
      [ "Error Handling", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md436", [
        [ "Connection Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md437", null ],
        [ "HTTP Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md438", null ],
        [ "JSON-RPC Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md439", null ]
      ] ],
      [ "Security Considerations", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md441", [
        [ "Transport Security", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md442", null ],
        [ "Authentication", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md443", null ],
        [ "Rate Limiting", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md444", null ]
      ] ],
      [ "Implementation Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md446", [
        [ "Client Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md447", null ],
        [ "Server Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md448", null ]
      ] ],
      [ "Example Implementations", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md450", [
        [ "SYNC Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md451", null ],
        [ "ASYNC Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md453", null ],
        [ "ASYNC_STREAM Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md455", null ]
      ] ],
      [ "Protocol Extensions", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md457", [
        [ "Batch Requests (Optional)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md458", null ],
        [ "Notifications (No Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md459", null ]
      ] ],
      [ "Compliance Checklist", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md461", [
        [ "JSON-RPC 2.0 Compliance", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md462", null ],
        [ "HTTP/1.1 Compliance", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md463", null ],
        [ "Custom Features", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md464", null ]
      ] ],
      [ "References", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md466", null ],
      [ "Changelog", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md468", null ]
    ] ],
    [ "FastLED ↔ Asio Compatibility Layer", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html", [
      [ "Type Mapping", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md529", null ],
      [ "Operation Mapping", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md530", null ],
      [ "Completion Handler Signatures", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md531", null ],
      [ "Error Handling", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md532", null ],
      [ "What's NOT Ported (By Design)", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md533", null ]
    ] ],
    [ "FastLED Source Tree (<tt>src/</tt>)", "d3/dcc/md__r_e_a_d_m_e.html", [
      [ "Table of Contents", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md630", null ],
      [ "Overview and Quick Start", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md632", [
        [ "What lives in src/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md633", null ],
        [ "Include policy", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md634", null ]
      ] ],
      [ "Directory Map (7 major areas)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md636", [
        [ "Public headers and glue", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md637", null ],
        [ "Core foundation: fl/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md638", null ],
        [ "Effects and graphics: fx/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md639", null ],
        [ "Platforms and HAL: platforms/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md640", null ],
        [ "Sensors and input: fl/sensors/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md641", null ],
        [ "Fonts: fl/font/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md642", null ],
        [ "Third‑party and shims: third_party/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md643", null ]
      ] ],
      [ "Quick Usage Examples", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md645", [
        [ "Classic strip setup", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md646", null ],
        [ "2D matrix with fl::Leds + XYMap", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md647", null ],
        [ "Resampling pipeline (downscale/upscale)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md648", null ],
        [ "JSON UI (WASM)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md649", null ]
      ] ],
      [ "Deep Dives by Area", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md651", [
        [ "Public API surface", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md652", null ],
        [ "Core foundation cross‑reference", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md653", null ],
        [ "FX engine building blocks", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md654", null ],
        [ "Platform layer and stubs", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md655", null ],
        [ "WASM specifics", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md656", null ],
        [ "Testing and compile‑time gates", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md657", null ]
      ] ],
      [ "Guidance for New Users", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md659", null ],
      [ "Guidance for C++ Developers", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md660", null ]
    ] ],
    [ "minimp3 provenance", "d6/d2d/md_third__party_2minimp3_2_p_r_o_v_e_n_a_n_c_e.html", [
      [ "Fixed-point path", "d6/d2d/md_third__party_2minimp3_2_p_r_o_v_e_n_a_n_c_e.html#autotoc_md719", null ],
      [ "Integer SIMD, and where it stops", "d6/d2d/md_third__party_2minimp3_2_p_r_o_v_e_n_a_n_c_e.html#autotoc_md720", null ],
      [ "Cortex-M4/M7 DSP evaluation", "d6/d2d/md_third__party_2minimp3_2_p_r_o_v_e_n_a_n_c_e.html#autotoc_md721", null ]
    ] ],
    [ "MoodRing redesign journal", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html", [
      [ "What MoodRing is for", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md956", null ],
      [ "Why the first version was half-broken", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md957", null ],
      [ "Round 1: listen / paint split, continuous blend", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md958", null ],
      [ "Round 2: make time musical", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md959", null ],
      [ "Round 3: simplify and harden", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md960", null ],
      [ "File map", "da/d77/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_mood_ring_2_d_e_s_i_g_n.html#autotoc_md961", null ]
    ] ],
    [ "TODO", "d1/d5a/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_t_o_d_o.html", null ],
    [ "Platform Porting Guide", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html", [
      [ "Fast porting for a new board on existing hardware", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1048", [
        [ "Setting up the basic files/folders", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1049", null ],
        [ "Porting fastpin.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1050", null ],
        [ "Porting fastspi.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1051", null ],
        [ "Porting clockless.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1052", null ]
      ] ]
    ] ],
    [ "Deprecated List", "da/d58/deprecated.html", null ],
    [ "Todo List", "dd/da0/todo.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "Typedefs", "namespacemembers_type.html", "namespacemembers_type" ],
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
"d0/d08/classfl_1_1span_adf9ad27aa9bd1c1e2cc19d0cf60d158a.html#adf9ad27aa9bd1c1e2cc19d0cf60d158a",
"d0/d16/namespacefl_1_1anonymous__namespace_02rgbw_8cpp_8hpp_03.html#abe05b57969377ea6b9c98f627bb2d6d4",
"d0/d34/classfl_1_1task_1_1_promise_ae75706ae1089e184850eb571f0b8c3aa.html#ae75706ae1089e184850eb571f0b8c3aa",
"d0/d5c/classfl_1_1_sorted_heap_vector_a5b7c577f3d1ea4a027a77d9c9712074a.html#a5b7c577f3d1ea4a027a77d9c9712074a",
"d0/d64/classfl_1_1s24x8_a2b5c9d48b7f3424ca6db349176916b21.html#a2b5c9d48b7f3424ca6db349176916b21",
"d0/d7c/midi___message_8h_source.html",
"d0/d8b/namespacefl_1_1gfx_ae6eb02735cead3a0436d0d277a06dd8b.html#ae6eb02735cead3a0436d0d277a06dd8b",
"d0/da2/classfl_1_1third__party_1_1ez_w_s2812gpio_abd365dec6866302bcf0ff7ed79f82636.html#abd365dec6866302bcf0ff7ed79f82636",
"d0/dce/classfl_1_1_x_y_map_ab06005819ba0c56b5b001d83245fcdbf.html#ab06005819ba0c56b5b001d83245fcdbfa5348a590e4fb28e59775d631657be19e",
"d0/ddb/classfl_1_1string__view_a55bae46776ecea864e3bd5e67c875f27.html#a55bae46776ecea864e3bd5e67c875f27",
"d0/de3/structfl_1_1_hash_3_01i16_01_4_abb364ad64dbb98bcf6cadc689c3419fc.html#abb364ad64dbb98bcf6cadc689c3419fc",
"d1/d02/classfl_1_1basic__string_1_1const__iterator_a3103088ce40fd36993595b11f1e449d7.html#a3103088ce40fd36993595b11f1e449d7",
"d1/d23/namespacefl_1_1test_a009f34d73047788b2ca911c944abe280.html#a009f34d73047788b2ca911c944abe280",
"d1/d2c/structfl_1_1_particles1d_1_1_particle_a5d87c5aa11ab2989b5ee06db5e35a468.html#a5d87c5aa11ab2989b5ee06db5e35a468",
"d1/d44/classfl_1_1audio_1_1detector_1_1_dynamics_analyzer_a5992a8e619d396337ad4dc5032b84e33.html#a5992a8e619d396337ad4dc5032b84e33",
"d1/d5e/classfl_1_1anonymous__namespace_02json_8cpp_8hpp_03_1_1_json_validator.html",
"d1/d74/classfl_1_1shared__ptr_a67c7a482eb02d6d199ddf7839e285d58.html#a67c7a482eb02d6d199ddf7839e285d58",
"d1/d86/namespacefl_1_1third__party.html#a2773bd4845ffdc1c9ebdd95ff1c83376",
"d1/d86/namespacefl_1_1third__party.html#d8/d1a/structfl_1_1third__party_1_1nsgif",
"d1/d86/namespacefl_1_1third__party_aaa3c17779b52e7724b4307cfefe4db83.html#aaa3c17779b52e7724b4307cfefe4db83a30086e34f378216f407fcf4108e2922d",
"d1/d93/classfl_1_1fixed__point__base_a958345939ae179861f19e0b3b1f22d31.html#a958345939ae179861f19e0b3b1f22d31",
"d1/dc5/_color_palette_8ino_accd116060cc3445af11c45ec44fd69d9.html#accd116060cc3445af11c45ec44fd69d9",
"d1/dd6/classfl_1_1weak__ptr_a4d7bbe5a9f9bf1f626099f2e1de2650b.html#a4d7bbe5a9f9bf1f626099f2e1de2650b",
"d1/de4/classfl_1_1_map_red_black_tree_1_1value__compare.html",
"d1/dfb/colorutils_8h.html",
"d2/d24/classfl_1_1_flow_field_f_p_ad7eb8ef7f2e1ca7912874978b98ca5a1.html#ad7eb8ef7f2e1ca7912874978b98ca5a1",
"d2/d26/classfl_1_1flat__map_a8ffa374f662311ea4b792cf6cc1e23d8.html#a8ffa374f662311ea4b792cf6cc1e23d8",
"d2/d48/structfl_1_1_auto_research_config_aabab603ef86a0cfd21fc91bbd8f163fe.html#aabab603ef86a0cfd21fc91bbd8f163fe",
"d2/d59/namespacefl_1_1third__party_1_1vorbis.html#a561c3ca8b05959a402bac23c4704ff7f",
"d2/d59/namespacefl_1_1third__party_1_1vorbis_a9d4ff1cfbb3f5db9236279d5367c0987.html#a9d4ff1cfbb3f5db9236279d5367c0987",
"d2/d68/structfl_1_1_u_i_button_1_1_listener_a4c73bf216be346bcb9dfe3c70ec29174.html#a4c73bf216be346bcb9dfe3c70ec29174",
"d2/d78/classfl_1_1_screen_map_ae82ea03ccd04d2db952c0a11556f060c.html#ae82ea03ccd04d2db952c0a11556f060c",
"d2/d9f/classfl_1_1_w_l_e_d_ac8c92c79b2fcb96c31fc00e19204ca7c.html#ac8c92c79b2fcb96c31fc00e19204ca7c",
"d2/db2/classfl_1_1string_a5ae19bd49fa67ff7a751bb191256354d.html#a5ae19bd49fa67ff7a751bb191256354d",
"d2/db7/classfl_1_1url_a6dcebd6dbc34c371c2fb1ac23c4f8a94.html#a6dcebd6dbc34c371c2fb1ac23c4f8a94",
"d2/dd5/classfl_1_1_x_y_raster_sparse___r_g_b8_a6cb214984a684873ee3b223b017cf82b.html#a6cb214984a684873ee3b223b017cf82b",
"d2/dec/classfl_1_1_vector_set_a5ed2d6b00b5cae9dd2ddd4d4ade93a1f.html#a5ed2d6b00b5cae9dd2ddd4d4ade93a1f",
"d2/dfa/classfl_1_1priority__queue__stable_acc865c1f1cb6b4e3eef7d2706c3e0622.html#acc865c1f1cb6b4e3eef7d2706c3e0622",
"d3/d1f/_server_real_8h.html",
"d3/d48/classfl_1_1u0x32_a8005797819ec8b8217f00ae1b2518f1b.html#a8005797819ec8b8217f00ae1b2518f1b",
"d3/d5e/classfl_1_1detail_1_1_response_aware_invoker_3_01_r_07_args_8_8_8_08_4_a2cbba93fd4b92456512a48eed546d48e.html#a2cbba93fd4b92456512a48eed546d48e",
"d3/d6b/classfl_1_1audio_1_1_processor_a709ddcfdcaedada11f91c5db2d25054a.html#a709ddcfdcaedada11f91c5db2d25054a",
"d3/d6e/native__client_8cpp_8hpp.html",
"d3/d7d/classfl_1_1_fled_a6fc9e9059fbba6b4069312b3651d4bd9.html#a6fc9e9059fbba6b4069312b3651d4bd9",
"d3/d8d/structfl_1_1_rgbw_load_binder_a963bcd91b50352fea688fcb4f5d186d6.html#a963bcd91b50352fea688fcb4f5d186d6",
"d3/da6/classfl_1_1audio_1_1detector_1_1_transient_ad80b8729fd136cd1100403d0d9876f7a.html#ad80b8729fd136cd1100403d0d9876f7a",
"d3/dc4/classfl_1_1detail_1_1_scaled_pixel_iterator_r_g_b_w_a7c8ca3453dc1b0ccdfe3c73bb433af6e.html#a7c8ca3453dc1b0ccdfe3c73bb433af6e",
"d3/dd1/classfl_1_1audio_1_1detector_1_1_backbeat_a358bb6be15e0754cd554cb9e17080d63.html#a358bb6be15e0754cd554cb9e17080d63",
"d3/df3/class_c_every_n_millis_random_aafda749035a0a666711d7d8697d96621.html#aafda749035a0a666711d7d8697d96621",
"d4/d0a/classfl_1_1audio_1_1detector_1_1_frequency_bands_a6448d3a3c387a18741e1e03c80354651.html#a6448d3a3c387a18741e1e03c80354651",
"d4/d28/group___chipsets_gab7631ed95e7d4284339b25780364402b.html#gab7631ed95e7d4284339b25780364402b",
"d4/d36/namespacefl.html#a6263eba5d5fe886fa7529395a9ca9b4d",
"d4/d36/namespacefl.html#ae3af226859c290d8f6ad211886ed666b",
"d4/d36/namespacefl_a071e4760220c8390ff571354c3c69b84.html#a071e4760220c8390ff571354c3c69b84",
"d4/d36/namespacefl_a2b10c80f10b3d6939134221e56e577bb.html#a2b10c80f10b3d6939134221e56e577bb",
"d4/d36/namespacefl_a491f26c54cd7c32f9a6e90f6b410506d.html#a491f26c54cd7c32f9a6e90f6b410506d",
"d4/d36/namespacefl_a6d93dfe697b79fe80883dfe32eb9ce56.html#a6d93dfe697b79fe80883dfe32eb9ce56a84c670758826bd5d8f75c4646814f47a",
"d4/d36/namespacefl_a92b2956db98046835d53a392813b14d2.html#a92b2956db98046835d53a392813b14d2",
"d4/d36/namespacefl_ab8c335f7d3dc59b74fb27598610f9abf.html#ab8c335f7d3dc59b74fb27598610f9abf",
"d4/d36/namespacefl_add83138a5ff2aea931074bcb3b799dc5.html#add83138a5ff2aea931074bcb3b799dc5",
"d4/d3a/classfl_1_1_u_i_group.html",
"d4/d54/compiler__control_8h_ad597a7b4a72f03310c0a569635c38e51.html#ad597a7b4a72f03310c0a569635c38e51",
"d4/d63/classfl_1_1unsorted__map__fixed_a6050625305f7775984d81da6bcb9b01c.html#a6050625305f7775984d81da6bcb9b01c",
"d4/d94/namespacemood__ring_1_1anonymous__namespace_02layers_8cpp_03_ac53a93381861ead2c5705d072e4c8537.html#ac53a93381861ead2c5705d072e4c8537",
"d4/dc2/structfl_1_1_hash_3_01fl_1_1shared__ptr_3_01_t_01_4_01_4_a04058ac25674dcd127749b25d92de238.html#a04058ac25674dcd127749b25d92de238",
"d4/dca/classfl_1_1array_ab8135a28fcfdce1929cba2aa53d186bb.html#ab8135a28fcfdce1929cba2aa53d186bb",
"d4/dd2/framebuffer_8h.html",
"d4/dfe/classfl_1_1audio_1_1detector_1_1_chord_detector_aa5ed53222c7a0f8cd4d76da804268674.html#aa5ed53222c7a0f8cd4d76da804268674",
"d5/d1e/structfl_1_1test_1_1_subcase_signature_a72f8f0504694244ba3a32a49f3e961c1.html#a72f8f0504694244ba3a32a49f3e961c1",
"d5/d38/namespacefl_1_1color_ae8069d2ad0f8eba4e3d7b75a0b6eab8b.html#ae8069d2ad0f8eba4e3d7b75a0b6eab8b",
"d5/d5d/structfl_1_1vec2_a8380ec5e1801a836f3f5de5e935571bc.html#a8380ec5e1801a836f3f5de5e935571bc",
"d5/d6f/structfl_1_1gfx_1_1blur__detail_1_1interior__row_3_010_00_01_r_g_b___t_00_01acc__t_01_4_abfdaad43ef5d5a6dffb1a66a76e7aabe.html#abfdaad43ef5d5a6dffb1a66a76e7aabe",
"d5/d83/structsimd__test_1_1_test_result_a98ba6a4b1bed6bcb8b5715910f9d0cf2.html#a98ba6a4b1bed6bcb8b5715910f9d0cf2",
"d5/d8b/classfl_1_1_channel_acd40622f0bf3b09e0f38b0dd42021c6f.html#acd40622f0bf3b09e0f38b0dd42021c6f",
"d5/d9d/classfl_1_1audio_1_1fft_1_1_impl_a8c6d2d8096967a2082c63da3fcf0fc1d.html#a8c6d2d8096967a2082c63da3fcf0fc1d",
"d5/db3/structfl_1_1pair_ac4a23cd9be72ad5bb8ea913ac841689b.html#ac4a23cd9be72ad5bb8ea913ac841689b",
"d5/dec/classfl_1_1u8x8_a7d3c4a2f23410bfbc7387223e28cb59c.html#a7d3c4a2f23410bfbc7387223e28cb59c",
"d6/d05/_animartrix_8ino_a4c4ae9a4146ce8d6a5debc90300d9abd.html#a4c4ae9a4146ce8d6a5debc90300d9abd",
"d6/d2a/classfl_1_1audio_1_1_noise_floor_tracker.html#aeb2ec18a809e3da09738398a0d56d6c2",
"d6/d30/classfl_1_1_multi_set_tree_1_1_const_iterator_wrapper_aed6739a386dc353958a54e895b12d9f8.html#aed6739a386dc353958a54e895b12d9f8",
"d6/d45/namespacefl_1_1ios_a22b1e4aa5b7104293fd67ef02998d5da.html#a22b1e4aa5b7104293fd67ef02998d5da",
"d6/d5f/classfl_1_1_wave_fx_a3d8f396b7d4f31a8c7a9c637fd7da6d0.html#a3d8f396b7d4f31a8c7a9c637fd7da6d0",
"d6/d64/fastled_8h_aea39aa3747a937b0dbefabdf22cf2bbc.html#aea39aa3747a937b0dbefabdf22cf2bbc",
"d6/d7c/structfl_1_1numeric__limits_a2b2339ce825702a70e844734319e5727.html#a2b2339ce825702a70e844734319e5727",
"d6/d7e/fltest_8h_abd0aba23d706248edaa0d010e72d3346.html#abd0aba23d706248edaa0d010e72d3346",
"d6/d8d/classfl_1_1_u_i_button_impl_a17c8d41a0befba2397d31c33454c7e95.html#a17c8d41a0befba2397d31c33454c7e95",
"d6/d9d/classfl_1_1_audio_batch_aac2726ab80841de7c12d5174bc49ac82.html#aac2726ab80841de7c12d5174bc49ac82",
"d6/dd2/classfl_1_1audio_1_1fft_1_1_context_a09e23fbc0c3d53a050530333673161b2.html#a09e23fbc0c3d53a050530333673161b2",
"d6/dd8/structfl_1_1audio_1_1_config_i2_s_a98826634330af1426a6c3aecca487fe2.html#a98826634330af1426a6c3aecca487fe2",
"d6/df1/structfl_1_1_copy_to_visitor_a568af8f1a8bc5c83e77b4d83851df43d.html#a568af8f1a8bc5c83e77b4d83851df43d",
"d6/dfd/structfl_1_1_hash_ae25f6bdd8eaba5b13070c16881f7eb1d.html#ae25f6bdd8eaba5b13070c16881f7eb1d",
"d7/d1c/classfl_1_1net_1_1_o_t_a_a5c8007108972f7c53b2bb3a97f62adbf.html#a5c8007108972f7c53b2bb3a97f62adbf",
"d7/d27/structfl_1_1_rgbw_a8e35da8b00be6f234e46de4a9d2b3e24.html#a8e35da8b00be6f234e46de4a9d2b3e24",
"d7/d3b/complex_8h_a5801a13917aa5f680d836b0543c4100b.html#a5801a13917aa5f680d836b0543c4100b",
"d7/d5d/classfl_1_1spi_1_1_transaction_a89d73ec9ffe7d8781fd013ee5cfbabf5.html#a89d73ec9ffe7d8781fd013ee5cfbabf5",
"d7/d6b/classfl_1_1span_3_01_t_00_01dynamic__extent_01_4_a5bb021a816cc4eee66ce767fd85da837.html#a5bb021a816cc4eee66ce767fd85da837",
"d7/d79/structfl_1_1_edge_time_abb6d83d85b6cd7f388050074bd052af5.html#abb6d83d85b6cd7f388050074bd052af5",
"d7/d91/audio__frame_8h_source.html",
"d7/db1/structfl_1_1is__signed_3_01long_01_4.html",
"d7/dd1/classfl_1_1math_1_1random_a910c355e5a018715e7945675ec9d445e.html#a910c355e5a018715e7945675ec9d445e",
"d7/df0/namespacefl_1_1colorimetric__detail_a0df33ba193138d35a2e8d63f9a3123c2.html#a0df33ba193138d35a2e8d63f9a3123c2",
"d8/d01/classfl_1_1_vorbis_decoder.html",
"d8/d35/classfl_1_1audio_1_1detector_1_1_multi_band_beat_a3a4b66b06bbccc2f2a48baa589759056.html#a3a4b66b06bbccc2f2a48baa589759056",
"d8/d53/_octo_w_s2811__impl_8h_a4fc01d736fe50cf5b977f755b675f11d.html#a4fc01d736fe50cf5b977f755b675f11d",
"d8/d76/classfl_1_1ifstream_ae7b397bedcf02879db36eccd987edbfa.html#ae7b397bedcf02879db36eccd987edbfa",
"d8/da0/class_driver_test_runner_a9bc3db3aa1baeffea830673b3f795d2a.html#a9bc3db3aa1baeffea830673b3f795d2a",
"d8/dc1/classfl_1_1_wave_simulation1_d_a717abac89b258cfbd9de9c39c89fb129.html#a717abac89b258cfbd9de9c39c89fb129",
"d8/dcc/classfl_1_1_i_channel.html",
"d8/dd2/classfl_1_1unordered__map__small_a3733acbb389d89793d6de2831231dba4.html#a3733acbb389d89793d6de2831231dba4",
"d8/de9/classfl_1_1fled_1_1_fled_impl_a96b29d897fd88eeabc19baa9afa1696f.html#a96b29d897fd88eeabc19baa9afa1696f",
"d8/dee/structfl_1_1greater_3_01void_01_4.html",
"d8/df9/classfl_1_1list_afa0b115afc55bef79a97567e457348d3.html#afa0b115afc55bef79a97567e457348d3",
"d9/d12/classfl_1_1_multi_map_tree_1_1_iterator_wrapper_a268abf64cc6c5318de15c60997fa41c5.html#a268abf64cc6c5318de15c60997fa41c5",
"d9/d1a/namespaceautoresearch_1_1simd__check.html#a224ffe156a7c265e48104eb3fb585a31",
"d9/d2a/classfl_1_1_potentiometer_aeaa757e1ecc617fcc4b558788e9e9054.html#aeaa757e1ecc617fcc4b558788e9e9054",
"d9/d34/structfl_1_1_flow_field_f_p_state_ae5fd727593479959c6d932ec5711fb9a.html#ae5fd727593479959c6d932ec5711fb9a",
"d9/d56/classfl_1_1asio_1_1http_1_1_server.html#a6e96013f5165ad04e1bd1bbf6e3289b1",
"d9/d66/classfl_1_1u4x12_aca2d285e90a1f46b66806e8c6d827e7e.html#aca2d285e90a1f46b66806e8c6d827e7e",
"d9/d77/classfl_1_1audio_1_1detector_1_1_note_abdf2b6bf2519dbbe12ba82a9f56f92ff.html#abdf2b6bf2519dbbe12ba82a9f56f92ff",
"d9/d9f/classfl_1_1s16x16_a37b89738ab6899fb9264f72ec8790b2a.html#a37b89738ab6899fb9264f72ec8790b2a",
"d9/db0/classfl_1_1_moving_average_a0010bf7c9a1b36161702643570a33ecc.html#a0010bf7c9a1b36161702643570a33ecc",
"d9/dd3/structfl_1_1numeric__limits_3_01char_01_4_aa9468d45bd22c1f31d7ddeda7986b592.html#aa9468d45bd22c1f31d7ddeda7986b592",
"d9/df4/classfl_1_1_x_y_path_a8e2a715e0c99e31cf949d81e0cfd10ae.html#a8e2a715e0c99e31cf949d81e0cfd10ae",
"da/d09/classfl_1_1_jpeg_decoder_1_1_impl.html",
"da/d3b/structfl_1_1make__unsigned.html",
"da/d43/classfl_1_1s12x4_ad8cbae5b7dd6dcb933a45496caf9238a.html#ad8cbae5b7dd6dcb933a45496caf9238a",
"da/d51/fl_2channels_2spi_2__build_8cpp_8hpp.html",
"da/d78/classfl_1_1asio_1_1http_1_1_response_a5e1ace4ff6bde5eb6bc9dd3d318ab6de.html#a5e1ace4ff6bde5eb6bc9dd3d318ab6de",
"da/d94/attack__decay__filter__impl_8h.html",
"da/da2/structfl_1_1detail_1_1_audio_logger_info_abbc54b230427f859f59d5872d9263805.html#abbc54b230427f859f59d5872d9263805",
"da/dc6/structfl_1_1priority__queue__stable_1_1_stable_element_ae18724f1665e1319d0dc717aa01e7a83.html#ae18724f1665e1319d0dc717aa01e7a83",
"da/dd6/structfl_1_1_c_r_g_b_a835a108043d3d709b55c34c39f66d3ef.html#a835a108043d3d709b55c34c39f66d3ef",
"da/dd6/structfl_1_1_c_r_g_b_add5b03a29164cfe4d6e6f36185244a9b.html#add5b03a29164cfe4d6e6f36185244a9ba842750742412d0fef1b56766607f515f",
"da/dde/classfl_1_1audio_1_1_signal_conditioner.html#ac6989be2efcab724f7f9b1e8bdc4b3cd",
"da/df9/classfl_1_1_stb_vorbis_decoder_a2470287dceaaf6db6296464db0f25b84.html#a2470287dceaaf6db6296464db0f25b84",
"db/d13/classfl_1_1memory__resource_a8cf7276a4cc29b7568c61c6b87d3e67f.html#a8cf7276a4cc29b7568c61c6b87d3e67f",
"db/d42/struct_key_a7f137c239dd54c73259f0df8731b5164.html#a7f137c239dd54c73259f0df8731b5164",
"db/d52/classfl_1_1number_af7e1389b8b362bb791604b523a761133.html#af7e1389b8b362bb791604b523a761133a3fbfeb013c3072123992eb293901841d",
"db/d58/classfl_1_1_perlin_particle_punch_af74aa00702e5075d35d7442374867ceb.html#af74aa00702e5075d35d7442374867ceb",
"db/d64/classfl_1_1s8x8.html#db/d1f/structfl_1_1s8x8_1_1_raw_tag",
"db/d7b/classfl_1_1_bilateral_filter_a3e75e2ce5a1746376fb05c1c06976c8c.html#a3e75e2ce5a1746376fb05c1c06976c8c",
"db/d89/classfl_1_1audio_1_1detector_1_1_pitch_a9cbc880acabb6c436a639c338350e2ce.html#a9cbc880acabb6c436a639c338350e2ce",
"db/da2/classfl_1_1_engine_events_1_1_listener_aebdd0563f4eb0ca7bd586d8d0d0ecacc.html#aebdd0563f4eb0ca7bd586d8d0d0ecacc",
"db/dbf/namespacefl_1_1audio.html#af0d7a2dd1531838bda417f463cf5ba60",
"db/deb/classmood__ring_1_1_flow_layer.html",
"db/dff/classfl_1_1s8x24_a8f93e5a41f847c2506f36d591191b6e3.html#a8f93e5a41f847c2506f36d591191b6e3",
"dc/d16/structfl_1_1_glyph_bitmap_a408d1c31d2cc51f1bdc0cb94d6bcab2e.html#a408d1c31d2cc51f1bdc0cb94d6bcab2e",
"dc/d2e/structfl_1_1callable__traits_3_01_r_07_c_1_1_5_08_07_args_8_8_8_08_4_ad0645cf0babf714277ad13e037964d00.html#ad0645cf0babf714277ad13e037964d00",
"dc/d41/namespacefl_1_1third__party_1_1truetype.html#dd/d7f/structfl_1_1third__party_1_1truetype_1_1stbtt____buf",
"dc/d4b/classfl_1_1istream__real_a8ca5d5b7033dc1286a7d7c842561ca1f.html#a8ca5d5b7033dc1286a7d7c842561ca1f",
"dc/d56/classfl_1_1unordered__map_ac375cd22f12c0721596e234103059111.html#ac375cd22f12c0721596e234103059111",
"dc/d6a/caleido2_8h_source.html",
"dc/d82/classfl_1_1_corkscrew_a96fb24562189ce902a49b06929ce7450.html#a96fb24562189ce902a49b06929ce7450",
"dc/d9c/structfl_1_1test_1_1detail_1_1_suite_scope.html",
"dc/db7/examples_2_luminova_2luminova_8h_a51ece01deb8ffb2d71d6487c4171e369.html#a51ece01deb8ffb2d71d6487c4171e369",
"dc/ddb/classfl_1_1_remote_aacc8520b7bee2c8fbc6ad058bcccd207.html#aacc8520b7bee2c8fbc6ad058bcccd207",
"dc/dec/structfl_1_1json__value_ac31ae1f8a76506152f81161b428e083b.html#ac31ae1f8a76506152f81161b428e083b",
"dc/dfe/classfl_1_1deque_ad8ae06bf7ce83b99b205af40b4ab6ea7.html#ad8ae06bf7ce83b99b205af40b4ab6ea7",
"dd/d0f/_apa102_8ino_a4c4ae9a4146ce8d6a5debc90300d9abd.html#a4c4ae9a4146ce8d6a5debc90300d9abd",
"dd/d28/classfl_1_1_red_black_tree_a2cd9d1202f7e5139b3f2e724b05be968.html#a2cd9d1202f7e5139b3f2e724b05be968",
"dd/d45/structfl_1_1_clockless_chipset_a6e380f08ec576660766049bb6cde3c85.html#a6e380f08ec576660766049bb6cde3c85",
"dd/d62/classfl_1_1_u_i_number_field_impl_ae1892c15c51d135a6c00954944cd8bf2.html#ae1892c15c51d135a6c00954944cd8bf2",
"dd/d7f/classfl_1_1_rings___f_p.html",
"dd/db8/classfl_1_1deque_1_1iterator_ac48864d93478826247715a3f0d55a92b.html#ac48864d93478826247715a3f0d55a92b",
"dd/dc7/group___h_s_v2_r_g_b_ga67db85b3beb8d49cb29c9889ea3f6ee4.html#ga67db85b3beb8d49cb29c9889ea3f6ee4",
"dd/dde/classfl_1_1video_1_1_video_impl_a0df776f1efbf24ab86caa13c96825292.html#a0df776f1efbf24ab86caa13c96825292",
"dd/df8/classfl_1_1_module___experiment10___f_p_a3a2e99466c065d14cabb5a074f9d1642.html#a3a2e99466c065d14cabb5a074f9d1642",
"de/d0c/mapping_8h_ae6e61eea9dcaba94419f40c467e4753d.html#ae6e61eea9dcaba94419f40c467e4753d",
"de/d29/structfl_1_1_potentiometer_1_1_listener_afdb1ff7d8868565797ddb8c8c991dc83.html#afdb1ff7d8868565797ddb8c8c991dc83",
"de/d40/classfl_1_1json_a8bc9791c502a7347529738376f61cb5d.html#a8bc9791c502a7347529738376f61cb5d",
"de/d52/structfl_1_1_nanos_range_a2d80b91f26646d21b3263ba7243902d4.html#a2d80b91f26646d21b3263ba7243902d4",
"de/d66/classfl_1_1basic__string_a78be9395642863b0c08c9bed61f3d47c.html#a78be9395642863b0c08c9bed61f3d47c",
"de/d67/classfl_1_1_serial_port_afd568b343586725745bafc10d4b82e55.html#afd568b343586725745bafc10d4b82e55",
"de/d82/classfl_1_1third__party_1_1_t_jpg___decoder_ac5f9658ecf4c5d19005fed7d49525d0e.html#ac5f9658ecf4c5d19005fed7d49525d0e",
"de/db7/xymap_8cpp_8hpp_source.html",
"de/dd4/classfl_1_1_map_red_black_tree_a709ada7fd95f912a9554f73689902387.html#a709ada7fd95f912a9554f73689902387",
"de/def/classfl_1_1istream_a71da7702200bad2661687e0290da41b9.html#a71da7702200bad2661687e0290da41b9",
"df/d03/classfl_1_1audio_1_1detector_1_1_downbeat_abac677e836607bfcba6230336aed4364.html#abac677e836607bfcba6230336aed4364",
"df/d1a/classfl_1_1audio_1_1detector_1_1_vibe_abd0626e0228a381ce8c82174a08ac263.html#abd0626e0228a381ce8c82174a08ac263",
"df/d28/classfl_1_1_jpeg_ad9a274c171b05a759dbe686b51c0180d.html#ad9a274c171b05a759dbe686b51c0180d",
"df/d4b/classfl_1_1test_1_1_test_context_a992ec1ba91e6f9339ee76eb8c5f1680b.html#a992ec1ba91e6f9339ee76eb8c5f1680b",
"df/d61/structfl_1_1int__conversion__visitor_3_01i64_01_4_aafe748f43d5f5cf5f5b62118ed154348.html#aafe748f43d5f5cf5f5b62118ed154348",
"df/d88/structfl_1_1is__integral_3_01char_01_4.html",
"df/d9e/namespacefl_1_1detail_a2c874578912a1e385624f7f2d03580f7.html#a2c874578912a1e385624f7f2d03580f7",
"df/da3/structfl_1_1response__aware__signature_3_01_r_07_5_08_07_response_send_01_6_00_01_args_8_8_8_08_4_a1e840bd20380e7d0f22503743f7e1727.html#a1e840bd20380e7d0f22503743f7e1727",
"df/dc7/structfl_1_1_draw_context_a1a5746286f28de2e4f3c3e0f91126e31.html#a1a5746286f28de2e4f3c3e0f91126e31",
"df/df1/classfl_1_1audio_1_1detector_1_1_mood_analyzer_a836b3612b2bc55f5ae0d92d4fcb9fe6d.html#a836b3612b2bc55f5ae0d92d4fcb9fe6d",
"dir_80f7443b4793499ecb90496bd7f8a795.html",
"functions_vars_w.html",
"namespacemembers_func_g.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
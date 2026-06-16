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
    [ "RGB8 Performance Guide", "d8/d7d/_c_r_g_b_performance_guide.html", [
      [ "Inline (Single-Pixel) Patterns", "d8/d7d/_c_r_g_b_performance_guide.html#inline_patterns", null ],
      [ "Bulk Operation Patterns", "d8/d7d/_c_r_g_b_performance_guide.html#bulk_patterns", null ],
      [ "Performance Characteristics", "d8/d7d/_c_r_g_b_performance_guide.html#performance_characteristics", null ]
    ] ],
    [ "fl/remote Architecture", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html", [
      [ "Overview", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md325", null ],
      [ "Layer Architecture", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md326", null ],
      [ "Layers", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md327", [
        [ "Transport Layer (fl/remote/transport/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md328", [
          [ "Serial Transport (transport/serial/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md329", null ],
          [ "HTTP Streaming Transport (transport/http/)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md330", null ]
        ] ],
        [ "Composition Layer (Factory Functions in transport/serial.h)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md331", null ],
        [ "Application Layer (fl/remote/remote.h)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md332", null ]
      ] ],
      [ "Design Rationale", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md333", [
        [ "Why Separate Transport and Application Logic?", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md334", null ],
        [ "Zero-Copy Optimization", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md335", null ],
        [ "Example: Using HTTP Streaming Transport", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md336", null ],
        [ "Example: Custom JSONL Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md337", null ]
      ] ],
      [ "File Organization", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md338", null ],
      [ "Protocol Formats", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md339", [
        [ "Serial Transport Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md340", null ],
        [ "HTTP Streaming Protocol", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md341", [
          [ "SYNC Mode (Immediate Response)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md342", null ],
          [ "ASYNC Mode (ACK + Result)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md343", null ],
          [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md344", null ]
        ] ]
      ] ],
      [ "RPC Modes and Response Handling", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md345", [
        [ "SYNC Mode (Default)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md346", null ],
        [ "ASYNC Mode", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md347", null ],
        [ "ASYNC_STREAM Mode", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md348", null ],
        [ "ResponseSend API", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md349", null ]
      ] ],
      [ "Migration Notes", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md350", [
        [ "From Old API (fastled3)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md351", null ],
        [ "From Serial to HTTP Transport", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md352", null ]
      ] ],
      [ "Sequence Diagrams", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md353", [
        [ "SYNC Mode (Immediate Response)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md354", null ],
        [ "ASYNC Mode (ACK + Result)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md355", null ],
        [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md356", null ]
      ] ],
      [ "Component Diagram", "df/d89/md_fl_2remote_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md357", null ]
    ] ],
    [ "HTTP Streaming RPC Protocol Specification", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html", [
      [ "Overview", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md447", null ],
      [ "Protocol Version", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md448", null ],
      [ "HTTP Request Format (Client → Server)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md450", [
        [ "Headers", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md451", null ],
        [ "JSON-RPC Request Format", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md452", null ]
      ] ],
      [ "HTTP Response Format (Server → Client)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md454", [
        [ "Headers", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md455", null ],
        [ "JSON-RPC Response Format", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md456", [
          [ "Success Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md457", null ],
          [ "Error Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md458", null ]
        ] ]
      ] ],
      [ "RPC Modes", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md460", [
        [ "SYNC Mode (Immediate Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md461", null ],
        [ "ASYNC Mode (ACK + Later Result)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md463", null ],
        [ "ASYNC_STREAM Mode (ACK + Updates + Final)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md465", null ]
      ] ],
      [ "Heartbeat Protocol", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md467", [
        [ "Ping (Request)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md468", null ],
        [ "Pong (Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md469", null ],
        [ "Heartbeat Timing", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md470", null ]
      ] ],
      [ "Connection Lifecycle", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md472", [
        [ "Initial Connection", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md473", null ],
        [ "Normal Operation", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md474", null ],
        [ "Reconnection", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md475", null ]
      ] ],
      [ "Error Handling", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md477", [
        [ "Connection Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md478", null ],
        [ "HTTP Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md479", null ],
        [ "JSON-RPC Errors", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md480", null ]
      ] ],
      [ "Security Considerations", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md482", [
        [ "Transport Security", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md483", null ],
        [ "Authentication", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md484", null ],
        [ "Rate Limiting", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md485", null ]
      ] ],
      [ "Implementation Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md487", [
        [ "Client Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md488", null ],
        [ "Server Requirements", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md489", null ]
      ] ],
      [ "Example Implementations", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md491", [
        [ "SYNC Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md492", null ],
        [ "ASYNC Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md494", null ],
        [ "ASYNC_STREAM Request/Response", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md496", null ]
      ] ],
      [ "Protocol Extensions", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md498", [
        [ "Batch Requests (Optional)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md499", null ],
        [ "Notifications (No Response)", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md500", null ]
      ] ],
      [ "Compliance Checklist", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md502", [
        [ "JSON-RPC 2.0 Compliance", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md503", null ],
        [ "HTTP/1.1 Compliance", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md504", null ],
        [ "Custom Features", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md505", null ]
      ] ],
      [ "References", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md507", null ],
      [ "Changelog", "d0/de6/md_fl_2stl_2asio_2http_2_p_r_o_t_o_c_o_l.html#autotoc_md509", null ]
    ] ],
    [ "FastLED ↔ Asio Compatibility Layer", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html", [
      [ "Type Mapping", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md441", null ],
      [ "Operation Mapping", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md442", null ],
      [ "Completion Handler Signatures", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md443", null ],
      [ "Error Handling", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md444", null ],
      [ "What's NOT Ported (By Design)", "df/d1f/md_fl_2stl_2asio_2_r_e_a_d_m_e___a_s_i_o___c_o_m_p_a_t.html#autotoc_md445", null ]
    ] ],
    [ "FastLED Source Tree (<tt>src/</tt>)", "d3/dcc/md__r_e_a_d_m_e.html", [
      [ "Table of Contents", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md593", null ],
      [ "Overview and Quick Start", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md595", [
        [ "What lives in src/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md596", null ],
        [ "Include policy", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md597", null ]
      ] ],
      [ "Directory Map (7 major areas)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md599", [
        [ "Public headers and glue", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md600", null ],
        [ "Core foundation: fl/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md601", null ],
        [ "Effects and graphics: fx/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md602", null ],
        [ "Platforms and HAL: platforms/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md603", null ],
        [ "Sensors and input: fl/sensors/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md604", null ],
        [ "Fonts: fl/font/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md605", null ],
        [ "Third‑party and shims: third_party/", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md606", null ]
      ] ],
      [ "Quick Usage Examples", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md608", [
        [ "Classic strip setup", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md609", null ],
        [ "2D matrix with fl::Leds + XYMap", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md610", null ],
        [ "Resampling pipeline (downscale/upscale)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md611", null ],
        [ "JSON UI (WASM)", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md612", null ]
      ] ],
      [ "Deep Dives by Area", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md614", [
        [ "Public API surface", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md615", null ],
        [ "Core foundation cross‑reference", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md616", null ],
        [ "FX engine building blocks", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md617", null ],
        [ "Platform layer and stubs", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md618", null ],
        [ "WASM specifics", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md619", null ],
        [ "Testing and compile‑time gates", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md620", null ]
      ] ],
      [ "Guidance for New Users", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md622", null ],
      [ "Guidance for C++ Developers", "d3/dcc/md__r_e_a_d_m_e.html#autotoc_md623", null ]
    ] ],
    [ "ARM Assembly Optimization for libhelix-mp3", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html", [
      [ "Automatic Selective Compilation", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md653", [
        [ "✅ Platforms Where Assembly IS Used", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md654", null ],
        [ "❌ Platforms Where Assembly is SKIPPED", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md655", null ]
      ] ],
      [ "How It Works", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md656", null ],
      [ "Performance", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md657", null ],
      [ "Technical Details", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md658", [
        [ "Assembly File", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md659", null ],
        [ "C++ Fallback (polyphase.cpp)", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md660", null ],
        [ "Function Naming", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md661", null ],
        [ "Removed Files", "d2/df4/md_third__party_2libhelix__mp3_2_r_e_a_d_m_e___a_r_m___a_s_s_e_m_b_l_y.html#autotoc_md662", null ]
      ] ]
    ] ],
    [ "TODO", "d1/d5a/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2examples_2_t_o_d_o.html", null ],
    [ "Platform Porting Guide", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html", [
      [ "Fast porting for a new board on existing hardware", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1013", [
        [ "Setting up the basic files/folders", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1014", null ],
        [ "Porting fastpin.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1015", null ],
        [ "Porting fastspi.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1016", null ],
        [ "Porting clockless.h", "dd/d9e/md__2home_2runner_2work_2_fast_l_e_d_2_fast_l_e_d_2_p_o_r_t_i_n_g.html#autotoc_md1017", null ]
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
"d0/d08/classfl_1_1span_ad107cb4f423b787fbafa1004c0ca463c.html#ad107cb4f423b787fbafa1004c0ca463c",
"d0/d26/structfl_1_1is__floating__point_3_01const_01_t_01_4.html",
"d0/d4b/classfl_1_1_video.html",
"d0/d5d/structfl_1_1detail_1_1_json_to_bool_visitor.html",
"d0/d67/structfl_1_1_driver_failure_info.html",
"d0/d89/group___color_enums_ga251e9e8dc2c7b981786b71706522b2a9.html#gga251e9e8dc2c7b981786b71706522b2a9aa9dab8ea30eb2a41c48956480b7014d1",
"d0/d9d/classfl_1_1_time_ramp_af2ed0a7927456db7d0be670f5692a28a.html#af2ed0a7927456db7d0be670f5692a28a",
"d0/dc9/kiss__fft_8h.html#ac1e17add2ae6b815da29d7d67b03fa70",
"d0/ddb/classfl_1_1_red_black_tree_1_1const__iterator_a57a3a529707ee97d01715b59fdd7987d.html#a57a3a529707ee97d01715b59fdd7987d",
"d0/ddf/classfl_1_1audio_1_1_frequency_bin_mapper.html#aace994bde397af17f8ad01b16cbf684a",
"d0/dee/struct_pixel_controller_abfc21e7a2692504f7331156ea9989fd4.html#abfc21e7a2692504f7331156ea9989fd4",
"d1/d0e/classfl_1_1_canvas_ac68111fd50a26816f4b3c2ed16a2a46d.html#ac68111fd50a26816f4b3c2ed16a2a46d",
"d1/d27/classfl_1_1sstream_a80752c20ba9bb0fc64fd538b93c007ae.html#a80752c20ba9bb0fc64fd538b93c007ae",
"d1/d3d/_smart_matrix_sketch_8h_source.html",
"d1/d5c/classfl_1_1_x_y_raster_u8_sparse_a3bead5d4ca0516b6a8d5cc0472e9d28e.html#a3bead5d4ca0516b6a8d5cc0472e9d28e",
"d1/d69/scope__exit_8h_source.html",
"d1/d85/class_ripple_ace5a8cbdae3ff509be19386ce33c530e.html#ace5a8cbdae3ff509be19386ce33c530e",
"d1/d86/namespacefl_1_1third__party.html#ab5fc367bafb6a0f45a68391fbd7e0a87",
"d1/d86/namespacefl_1_1third__party_a2a0dfcebbdd7789efe9951d5925c7683.html#a2a0dfcebbdd7789efe9951d5925c7683",
"d1/d86/namespacefl_1_1third__party_ab32dae62203e078b4a140581d601c340.html#ab32dae62203e078b4a140581d601c340",
"d1/d93/classfl_1_1fixed__point__base_a90a8801110fafa2dd0b6ed21c9d4048b.html#a90a8801110fafa2dd0b6ed21c9d4048b",
"d1/dc6/classfl_1_1_u_i_dropdown_a0eed3145de02fda94d09d4964282873b.html#a0eed3145de02fda94d09d4964282873b",
"d1/dda/cq__kernel_8h_a356fef204f9e57567a3248b4fa3ed5aa.html#a356fef204f9e57567a3248b4fa3ed5aa",
"d1/de8/classfl_1_1ostream_a2794c4356dbccb9fbfe272b7022947c0.html#a2794c4356dbccb9fbfe272b7022947c0",
"d2/d0d/group___random.html",
"d2/d24/classfl_1_1vector_a6710323c876d2b170a27ae41bab1a3d8.html#a6710323c876d2b170a27ae41bab1a3d8",
"d2/d29/structfl_1_1_i_channel_driver_1_1_capabilities.html",
"d2/d4d/classfl_1_1_u_i_audio_acd6fb9cbc2acf58fa62f217da8f74fbc.html#acd6fb9cbc2acf58fa62f217da8f74fbc",
"d2/d59/namespacefl_1_1third__party_1_1vorbis.html#acc6123682436fa9dd573edbb0f24c424",
"d2/d65/classfl_1_1audio_1_1detector_1_1_musical_beat.html#a1941dcc52c7af8936dd2be6f98c90a87",
"d2/d6b/classfl_1_1_channel_manager_ab7a49c773268b687f1594f5e669f4e6c.html#ab7a49c773268b687f1594f5e669f4e6c",
"d2/d8c/structfl_1_1_hash_3_01vec2_3_01_t_01_4_01_4_a04f35dd9f3585f14e09e7ab410d09761.html#a04f35dd9f3585f14e09e7ab410d09761",
"d2/dac/classfl_1_1_fast_l_e_d_adapter_ab294fd37ca544ad7da8d995bdf5b29f9.html#ab294fd37ca544ad7da8d995bdf5b29f9",
"d2/db2/classfl_1_1string_af3143b481b75b236436ae01e333c697e.html#af3143b481b75b236436ae01e333c697e",
"d2/dbc/classfl_1_1_x_y_path_renderer_a0b17bd1cc6178b72ecb7604354a778e2.html#a0b17bd1cc6178b72ecb7604354a778e2",
"d2/de9/classfl_1_1fstream_a35775aef9f0b12abb753044940dc542b.html#a35775aef9f0b12abb753044940dc542b",
"d3/d03/classfl_1_1_multi_set_tree_1_1_const_reverse_iterator_wrapper_a0ef2696211f61b8739c54dde1ca90af9.html#a0ef2696211f61b8739c54dde1ca90af9",
"d3/d25/classfl_1_1_digital_pin_impl_a8a4517a0b4a1cd30f8864c6b11abeb95.html#a8a4517a0b4a1cd30f8864c6b11abeb95",
"d3/d48/classfl_1_1u0x32_ae90ac09493201f5ac9d87c36af20b2e7.html#ae90ac09493201f5ac9d87c36af20b2e7",
"d3/d69/structfl_1_1_multi_map_tree_1_1_pair_compare_with_id.html",
"d3/d6b/classfl_1_1audio_1_1_processor_a8c054981ff8501c22575828203ceeeda.html#a8c054981ff8501c22575828203ceeeda",
"d3/d73/classfl_1_1_fixed_vector_a169763e2c993f0628d09b1eeca91faa9.html#a169763e2c993f0628d09b1eeca91faa9",
"d3/d7e/classfl_1_1detail_1_1_spectral_variance_impl_a33a4785cfec66ac5418ed2b07e7061af.html#a33a4785cfec66ac5418ed2b07e7061af",
"d3/da6/classfl_1_1audio_1_1detector_1_1_transient_a3ed8119ad8414e6775d9ab53e4d7c2be.html#a3ed8119ad8414e6775d9ab53e4d7c2be",
"d3/daf/splat_8h_source.html",
"d3/dcd/structfl_1_1anonymous__namespace_02gradient_8cpp_8hpp_03_1_1_visitor_a5299ad9dab45acad95523131b2d8085e.html#a5299ad9dab45acad95523131b2d8085e",
"d3/de0/namespacefl_1_1anonymous__namespace_02rgbww_8cpp_8hpp_03_a4c35bdb38798909c3ce2c28896ae8348.html#a4c35bdb38798909c3ce2c28896ae8348",
"d4/d06/curr_8h_afede82fc6e8ef5f15a79fc75e937b5d9.html#afede82fc6e8ef5f15a79fc75e937b5d9",
"d4/d1c/classfl_1_1_flow_field_a13ac4c5f56afc7a4f981194385f60756.html#a13ac4c5f56afc7a4f981194385f60756",
"d4/d36/namespacefl.html#a357fee9cbf844e378ee10026bb94a20c",
"d4/d36/namespacefl.html#abd4144072696c59b82e97d7a395835c0",
"d4/d36/namespacefl_a021864abc9f3c84188076856f02bbb36.html#a021864abc9f3c84188076856f02bbb36",
"d4/d36/namespacefl_a293efd41430fafa91668716112cd2b09.html#a293efd41430fafa91668716112cd2b09",
"d4/d36/namespacefl_a4ab5702bb3b99d547de43de5ced26ca2.html#a4ab5702bb3b99d547de43de5ced26ca2",
"d4/d36/namespacefl_a769a0468668430335fcc5260ebb89043.html#a769a0468668430335fcc5260ebb89043",
"d4/d36/namespacefl_a9cbf1f2a5558ce8f1ba9c1b1eb411db7.html#a9cbf1f2a5558ce8f1ba9c1b1eb411db7",
"d4/d36/namespacefl_ac7fef97b0cb4d567cdf7d0886089c8d9.html#ac7fef97b0cb4d567cdf7d0886089c8d9",
"d4/d36/namespacefl_aef30cbd1dac648887ffa5dc1086b0164.html#aef30cbd1dac648887ffa5dc1086b0164a7e1d7a172afeb2743e03365a92cfd7e1",
"d4/d49/structfl_1_1_copy_to_output_iterator_visitor_a8731f78600652e5aab3d026881d040ac.html#a8731f78600652e5aab3d026881d040ac",
"d4/d60/classfl_1_1_fx_engine_a6e8578dffb32b7feb7801af837afb3bd.html#a6e8578dffb32b7feb7801af837afb3bd",
"d4/d72/_flow_field_8ino_a4fc01d736fe50cf5b977f755b675f11d.html#a4fc01d736fe50cf5b977f755b675f11d",
"d4/dad/structfl_1_1contains__type_3_01_t_01_4.html",
"d4/dc8/classanimartrix__ring_1_1_sound_orchestrator_aae4bd2e6d021a47353af7b1ea59d4180.html#aae4bd2e6d021a47353af7b1ea59d4180",
"d4/dd1/classfl_1_1third__party_1_1_software_mpeg1_decoder.html#ae8bcaf1db6f4fa4da42f0101a6a4cc8b",
"d4/df2/classfl_1_1_luminova_a56e00591e93399001a84e2e5380b6ca6.html#a56e00591e93399001a84e2e5380b6ca6",
"d5/d0f/classfl_1_1test_1_1_x_m_l_reporter_af135d467c349008f9343eab8beb94a24.html#af135d467c349008f9343eab8beb94a24",
"d5/d29/classfl_1_1_complex___kaleido_a3aeac35a401611947cd94a36d6ac09eb.html#a3aeac35a401611947cd94a36d6ac09eb",
"d5/d56/classfl_1_1unordered__set_1_1const__iterator_ad42d1668622ca0cea7535e26150beb2a.html#ad42d1668622ca0cea7535e26150beb2a",
"d5/d65/classfl_1_1s4x12_ac20adb5a2c523b6ed3726efba8fa5f3b.html#ac20adb5a2c523b6ed3726efba8fa5f3b",
"d5/d79/classfl_1_1_scale_up_a908785a5a2e8b598f1c1a0045d058b92.html#a908785a5a2e8b598f1c1a0045d058b92",
"d5/d8c/md_fl_2audio_2_t_e_s_t_i_n_g.html#autotoc_md55",
"d5/da0/classfl_1_1_fx2d_to1d_a97ed364bc718dd154e14122443ba5c5e.html#a97ed364bc718dd154e14122443ba5c5e",
"d5/dc2/structfl_1_1audio_1_1detector_1_1_buildup_a4f8f8007cbb848b5292d77e566d4d8b2.html#a4f8f8007cbb848b5292d77e566d4d8b2",
"d5/df4/classfl_1_1_channel_data_a57b023952ec7f4c794645b04cdfed798.html#a57b023952ec7f4c794645b04cdfed798",
"d6/d10/structfl_1_1audio_1_1fft_1_1_impl_1_1_result_ad26009f8e94f277aa482cac3c26bacab.html#ad26009f8e94f277aa482cac3c26bacab",
"d6/d2d/classfl_1_1string__n_afda388850c48e2900b924ac2684fefbf.html#afda388850c48e2900b924ac2684fefbf",
"d6/d31/classfl_1_1audio_1_1detector_1_1_vocal_ac2378cf315b3c607a0005f1047444830.html#ac2378cf315b3c607a0005f1047444830",
"d6/d4f/classfl_1_1_multi_map_tree_aaa0e1e52b4730bf448abe12acd93cbad.html#aaa0e1e52b4730bf448abe12acd93cbad",
"d6/d64/classfl_1_1_multi_set_tree_a7d05edf7947de0fc482cf687cc7cdcf8.html#a7d05edf7947de0fc482cf687cc7cdcf8",
"d6/d7c/structfl_1_1numeric__limits_a34eeb6affaa5380f3918ca88e89b3797.html#a34eeb6affaa5380f3918ca88e89b3797",
"d6/d7e/fltest_8h_a2d960a78b08d79c2beeb38ab6cb0b81a.html#a2d960a78b08d79c2beeb38ab6cb0b81a",
"d6/d88/classfl_1_1_type_conversion_result_aaad8902d13cb0b184284875cf4bfba9b.html#aaad8902d13cb0b184284875cf4bfba9b",
"d6/d9d/classfl_1_1_audio_batch_ab98b6d383de7a3294f4867a31e1cbc46.html#ab98b6d383de7a3294f4867a31e1cbc46",
"d6/dd2/classfl_1_1audio_1_1fft_1_1_context_a5f3c2c6e3d3bdfb7bc13ffde8562e272.html#a5f3c2c6e3d3bdfb7bc13ffde8562e272",
"d6/ddb/structfl_1_1audio_1_1detector_1_1_chord_af10bd46aa4c6073fabff7ecbf238ea40.html#af10bd46aa4c6073fabff7ecbf238ea40",
"d6/df8/classfl_1_1audio_1_1detector_1_1_tempo_analyzer_a3e32829988e7e96e82c1ae8bde50316a.html#a3e32829988e7e96e82c1ae8bde50316a",
"d7/d04/classfl_1_1task_1_1_i_task_impl_ada79c01b049462b6f79b2ed32230f403.html#ada79c01b049462b6f79b2ed32230f403",
"d7/d21/classfl_1_1audio_1_1detector_1_1_percussion_ae5d4122706a6dea3e1f6dc638caa90e9.html#ae5d4122706a6dea3e1f6dc638caa90e9",
"d7/d35/_fx_sd_card_8ino_af04e6ec3c92f294d177b2ffd4885ee71.html#af04e6ec3c92f294d177b2ffd4885ee71",
"d7/d4d/classfl_1_1detail_1_1_scaled_pixel_iterator_r_g_b16_ad9d5d58bb3c5b010ef8a0f0fde26580f.html#ad9d5d58bb3c5b010ef8a0f0fde26580f",
"d7/d6a/structfl_1_1_x_y_draw_gradient_af5260c3591427f3b1bebef53ee940445.html#af5260c3591427f3b1bebef53ee940445",
"d7/d70/classfl_1_1spi_1_1_multi_lane_device_ab88d95033af8f217808ba3b05dc823eb.html#ab88d95033af8f217808ba3b05dc823eb",
"d7/d8f/struct_draw_raster_to_wave_simulator_a38007d07674f8fec9685627c66ca4d1c.html#a38007d07674f8fec9685627c66ca4d1c",
"d7/db1/structfl_1_1_t_i_m_i_n_g___w_s2811__400_k_h_z.html",
"d7/dd9/classfl_1_1task_1_1_coroutine.html",
"d7/df2/structfl_1_1audio_1_1detector_1_1_vocal_detector_diagnostics_a2ec56fb2a0b98835cce656c76bc6d7c0.html#a2ec56fb2a0b98835cce656c76bc6d7c0",
"d8/d08/class_timer_a5f16e8da27d2a5a5242dead46de05d97.html#a5f16e8da27d2a5a5242dead46de05d97",
"d8/d38/structfl_1_1third__party_1_1_gif_bitmap_a0e1d1c88c9e7e157841d7bc75fe9cab4.html#a0e1d1c88c9e7e157841d7bc75fe9cab4",
"d8/d62/classfl_1_1_time_clamped_transition_a1c66cba1e4d80f8b265f3fda76001511.html#a1c66cba1e4d80f8b265f3fda76001511",
"d8/d7f/classfl_1_1_module___experiment4___f_p.html",
"d8/da0/classfl_1_1_c_l_e_d_controller_ab8e1ee460f25af890c1d40479e4c5b09.html#ab8e1ee460f25af890c1d40479e4c5b09",
"d8/dca/classfl_1_1allocator__inlined_a72180eba0a300cba505e5b9f7d4b3ebf.html#a72180eba0a300cba505e5b9f7d4b3ebf",
"d8/dd0/midi___defs_8h_ae5f8dc293002c1090c3551ba40f6aee8.html#ae5f8dc293002c1090c3551ba40f6aee8",
"d8/de5/structfl_1_1detail_1_1resolve__bus_3_01_bus_1_1_a_u_t_o_00_01_chipset_01_4_ade592986c26dd607ce57eae7ec729bd7.html#ade592986c26dd607ce57eae7ec729bd7",
"d8/dea/classfl_1_1audio_1_1_reactive_af7c2a94a6f71ca952b22556cafc2955b.html#af7c2a94a6f71ca952b22556cafc2955b",
"d8/df9/classfl_1_1list_ace1388cbddbc57d89219f1d6216fb4e3.html#ace1388cbddbc57d89219f1d6216fb4e3",
"d9/d0a/classfl_1_1spi_1_1_device_a4b4903b281bf5fdb3ddb6ec923b53d71.html#a4b4903b281bf5fdb3ddb6ec923b53d71",
"d9/d17/classfl_1_1audio_1_1detector_1_1_drop_detector_aeaa28ff90b01cf8ac272bf861effb3aa.html#aeaa28ff90b01cf8ac272bf861effb3aa",
"d9/d2a/classfl_1_1_potentiometer_a7205b0db178d7e92c89c46f10ef0f0e2.html#a7205b0db178d7e92c89c46f10ef0f0e2",
"d9/d34/structfl_1_1_flow_field_f_p_state_a325c5924a356f8905713e44e211df1bb.html#a325c5924a356f8905713e44e211df1bb",
"d9/d52/classfl_1_1audio_1_1_sample_impl_aa6435ea533544e94a8991861bd07b8c3.html#aa6435ea533544e94a8991861bd07b8c3",
"d9/d66/classfl_1_1u4x12_a98f3a875120274d5bb4aa5911063357e.html#a98f3a875120274d5bb4aa5911063357e",
"d9/d77/classfl_1_1audio_1_1detector_1_1_note_abc3566b7ccf94afbff8f408bd6a09977.html#abc3566b7ccf94afbff8f408bd6a09977",
"d9/d9f/classfl_1_1s16x16_a6b2a5ecef2102dd6787e635a8607b151.html#a6b2a5ecef2102dd6787e635a8607b151",
"d9/db5/classfl_1_1_big___caleido___f_p_abdfc57695a1c9f4da5c8a3166c7437e2.html#abdfc57695a1c9f4da5c8a3166c7437e2",
"d9/dea/time_8cpp_8hpp_a49606be7356624568932ec81c0d429f4.html#a49606be7356624568932ec81c0d429f4",
"d9/dfb/classfl_1_1audio_1_1detector_1_1_key_detector_a9e226028528732cc43e288a47ab06235.html#a9e226028528732cc43e288a47ab06235",
"da/d1e/structfl_1_1gfx_1_1blur__detail_1_1pixel__ops_3_01_c_r_g_b_01_4_aa32edb0024226b933e1c275381502101.html#aa32edb0024226b933e1c275381502101",
"da/d3c/namespacefl_1_1fl.html#d0/d9d/structfl_1_1fl_1_1remove__extent",
"da/d43/classfl_1_1s12x4_ad95562b9dda482d099422cd456051fd8.html#ad95562b9dda482d099422cd456051fd8",
"da/d52/stl_2asio_2http_2server_8cpp_8hpp.html",
"da/d78/classfl_1_1asio_1_1http_1_1_response_a6844fcc0edb41a50dad5d21ee745ad1a.html#a6844fcc0edb41a50dad5d21ee745ad1a",
"da/d97/u8x8_8cpp_8hpp.html",
"da/dae/_teensy_massive_parallel_8ino.html",
"da/dc7/advanced_8h_ab656ca94f0f14a07bd099c276fa3359c.html#ab656ca94f0f14a07bd099c276fa3359c",
"da/dd6/structfl_1_1_c_r_g_b_add5b03a29164cfe4d6e6f36185244a9b.html#add5b03a29164cfe4d6e6f36185244a9ba1c7e39b5b63960b7d2c2bffe5bfa14ec",
"da/dd6/structfl_1_1_c_r_g_b_add5b03a29164cfe4d6e6f36185244a9b.html#add5b03a29164cfe4d6e6f36185244a9babd5e5eea32337549f5ed51ed2316690a",
"da/de3/group___color_fills_ga99d02e2309e2242f1d987d399aaa387a.html#ga99d02e2309e2242f1d987d399aaa387a",
"da/df9/classfl_1_1detail_1_1_scaled_pixel_iterator_r_g_b_w_w_aff60f7b1d6e204b252f91f45aa29f140.html#aff60f7b1d6e204b252f91f45aa29f140",
"db/d22/classfl_1_1_response_send_a5fec154aac60a134f0bc42c9d07ab8d0.html#a5fec154aac60a134f0bc42c9d07ab8d0",
"db/d4e/classfl_1_1net_1_1http_1_1_http_stream_transport.html#ae6d8ee9186de9b7f2b3395a695206ab5",
"db/d58/classfl_1_1_perlin_particle_punch.html#ae9b8132dce5686d3dc56cdf5722e7553",
"db/d5d/classfl_1_1audio_1_1detector_1_1_buildup_detector_a87ed72b7c36c92c5cae666438b6ff680.html#a87ed72b7c36c92c5cae666438b6ff680",
"db/d67/structfl_1_1swap__impl_3_01_t_00_01false_01_4_a873b507eb8e2bd4e318c4f198ae32801.html#a873b507eb8e2bd4e318c4f198ae32801",
"db/d89/classfl_1_1audio_1_1detector_1_1_pitch_a31ddf84f663582e1c5411575ea031c92.html#a31ddf84f663582e1c5411575ea031c92",
"db/d98/perlin__q16_8h_source.html",
"db/dbb/inlined__data_8cpp_source.html",
"db/dd7/classfl_1_1_noise_bias1_d_ab47cc09cf4c28b30a26be88f4f24dbfd.html#ab47cc09cf4c28b30a26be88f4f24dbfd",
"db/dff/classfl_1_1s8x24_a3ea6cbfbc43c1aff068011cc8a5ab9cf.html#a3ea6cbfbc43c1aff068011cc8a5ab9cf",
"dc/d22/structfl_1_1_multi_run_config.html",
"dc/d41/namespacefl_1_1third__party_1_1truetype.html#a9479cf6fb1c67bb93a9868b3de4f9819",
"dc/d41/structfl_1_1fl_1_1is__member__function__pointer.html",
"dc/d56/classfl_1_1unordered__map_a2fa5a90d894b9333c70d404cbd79be29.html#a2fa5a90d894b9333c70d404cbd79be29",
"dc/d62/optional_8h.html",
"dc/d7c/structfl_1_1vec3_ac35a4fea338e5bcaa9af5760623f03de.html#ac35a4fea338e5bcaa9af5760623f03de",
"dc/d8e/structfl_1_1detail_1_1_json_to_type_a95c296afbad272cd567858021e9e5156.html#a95c296afbad272cd567858021e9e5156",
"dc/db1/random_8cpp_8hpp.html",
"dc/ddb/classfl_1_1_remote_a84f3e07d1ad9abea93773c4fbeb8dd71.html#a84f3e07d1ad9abea93773c4fbeb8dd71",
"dc/dec/structfl_1_1json__value_aa64f1b86a5b4078db3e4ce7892cde858.html#aa64f1b86a5b4078db3e4ce7892cde858",
"dc/dfe/classfl_1_1deque_aceba92645803e389082929a1d9ccd169.html#aceba92645803e389082929a1d9ccd169",
"dd/d0f/_apa102_8ino_source.html",
"dd/d28/classfl_1_1_red_black_tree_a4566c51a366d57fcd85936fc5d11eda8.html#a4566c51a366d57fcd85936fc5d11eda8afb0136b923af8c04b31a9d1b5e989acf",
"dd/d45/structfl_1_1_clockless_chipset_a9cac72d6d9a62651ea249d3628551f2a.html#a9cac72d6d9a62651ea249d3628551f2a",
"dd/d64/namespacefl_1_1detail_1_1anonymous__namespace_02allocator_8cpp_8hpp_03.html",
"dd/d80/classfl_1_1detail_1_1_scaled_pixel_iterator_r_g_b_a1642cd82b14c5aa33b932123cfadd499.html#a1642cd82b14c5aa33b932123cfadd499",
"dd/dbb/class_c_fast_l_e_d_a03f0559bdcb421d199f6eb8faa0002f9.html#a03f0559bdcb421d199f6eb8faa0002f9",
"dd/dca/namespace_codec_processor.html",
"dd/dde/classfl_1_1video_1_1_video_impl_abb5bb3f079577aff2031d9f9141e7edc.html#abb5bb3f079577aff2031d9f9141e7edc",
"de/d09/namespacefl_1_1anonymous__namespace_02chasing__spirals_8cpp_8hpp_03.html#a4b4f36a6d72507ce56d0362c2d7450d4",
"de/d10/classfl_1_1_rectangular_draw_buffer_a8a7ee487ac870855ded20e857acbfbbf.html#a8a7ee487ac870855ded20e857acbfbbfa332f6d620a308da9cfc601c35b6d20b9",
"de/d40/classfl_1_1json_a05686d9138b900b8d8b7d54a7d7dd9c7.html#a05686d9138b900b8d8b7d54a7d7dd9c7",
"de/d40/classfl_1_1json_afb972511a01213af38b8073dab14bc1c.html#afb972511a01213af38b8073dab14bc1c",
"de/d66/classfl_1_1basic__string_a171b40cdbd9ef2efc823c828da828d23.html#a171b40cdbd9ef2efc823c828da828d23",
"de/d66/classfl_1_1basic__string_ac999e017d7d871f23454c99d5819b81d.html#ac999e017d7d871f23454c99d5819b81d",
"de/d72/class_c_every_n_millis_dynamic.html",
"de/da7/classfl_1_1anonymous__namespace_02json_8cpp_8hpp_03_1_1_json_builder_a63e8ea6a560a55e0ee57fc6fb2346e11.html#a63e8ea6a560a55e0ee57fc6fb2346e11",
"de/dc9/_animartrix_8ino-example.html",
"de/dde/namespacefl_1_1asset__detail.html#a9144cfb1175d4295da7fd6f97afe131b",
"de/dfa/classfl_1_1_chasing___spirals___float_ac8384a1a75d5c44f32bd758563f7a16b.html#ac8384a1a75d5c44f32bd758563f7a16b",
"df/d11/classfl_1_1_pride2015_a774c173d32e2db00e3247668d3d3d6cb.html#a774c173d32e2db00e3247668d3d3d6cb",
"df/d1d/classfl_1_1_phyllotaxis_path_abce77e4ea5900344289ca09d2225b394.html#abce77e4ea5900344289ca09d2225b394",
"df/d37/classfl_1_1_file_system_a9bbd51efdf7a56d2880ec1a13d540f97.html#a9bbd51efdf7a56d2880ec1a13d540f97",
"df/d53/classfl_1_1sstream__noop_a2cffcd18f4c944dbd4176fc5c81d4fce.html#a2cffcd18f4c944dbd4176fc5c81d4fce",
"df/d68/structfl_1_1detail_1_1_spi_logger_info.html",
"df/d91/classfl_1_1_priority_queue_ab150772233344f283bb79cfa0359a8c8.html#ab150772233344f283bb79cfa0359a8c8",
"df/d9e/namespacefl_1_1detail_a044429d234076342c0a79d99a8d4f71c.html#a044429d234076342c0a79d99a8d4f71c",
"df/dab/classfl_1_1_null_file_handle_accbb05e675a5ae6763c2441dbd576a67.html#accbb05e675a5ae6763c2441dbd576a67",
"df/dcd/classfl_1_1unique__ptr_3_01_t_0f_0e_00_01_deleter_01_4_aad9db604b59795770702fa7e0a9abef6.html#aad9db604b59795770702fa7e0a9abef6",
"df/df5/structfl_1_1_channel_config_a2ec2197512f5b13739e043b4c5c75c58.html#a2ec2197512f5b13739e043b4c5c75c58",
"dir_db6352ed6231c09262436175d9525ae9.html",
"globals_vars_k.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
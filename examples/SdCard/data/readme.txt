data/ directory will appear automatically in emscripten web builds.

  * The .dat file represents uncompressed RGB video data.
  * the the screenmap.csv represents the coordinates of each pixel index a screen (see wasm build)
    * right now this is in csv format while FastLED only supports json format, but this will be fixed.
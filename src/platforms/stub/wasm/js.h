#pragma once


// Functions called on the C++ side that will land in JavaScript.
void jsOnFrame();  // Called when a frame is done.

// Sets the canvas size. This assumes one strip per row. This is
// method is pretty inflexible and is likely to change in the future.
void jsSetCanvasSize(int width, int height);

#pragma once

/// @file gfx.h
/// @brief 2D graphics primitives with antialiased drawing operations
///
/// Provides antialiased drawing functions for lines, circles, rings, and thick lines.
/// Template API accepts any arithmetic coordinate type (float, s16x16, int, etc).
/// Can draw to Leds* or custom DrawCanvas targets.

// IWYU pragma: begin_keep
#include "gfx/canvas.h"
#include "gfx/primitives.h"
// IWYU pragma: end_keep

// Types and functions are available via fl::gfx namespace
// Users should use: fl::gfx::drawLine(), fl::gfx::DrawCanvas, etc.

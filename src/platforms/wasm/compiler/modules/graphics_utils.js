/* eslint-disable no-console */
/* eslint-disable guard-for-in */
/* eslint-disable no-restricted-syntax */

/**
 * Determines if the LED layout represents a dense grid
 * @param {Object} frameData - The frame data containing screen mapping information
 * @returns {boolean} - True if the layout is a dense grid (pixel density close to 1)
 */
export function isDenseGrid(frameData) {
  const { screenMap } = frameData;

  // Check if all pixel densities are undefined
  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    const strip = screenMap.strips[stripId];
    allPixelDensitiesUndefined = allPixelDensitiesUndefined && (strip.diameter === undefined);
    if (!allPixelDensitiesUndefined) {
      break;
    }
  }

  if (!allPixelDensitiesUndefined) {
    return false;
  }

  // Calculate total pixels and screen area
  let totalPixels = 0;
  for (const strip of frameData) {
    if (strip.strip_id in screenMap.strips) {
      const stripMap = screenMap.strips[strip.strip_id];
      const len = Math.min(stripMap.map.x.length, stripMap.map.y.length);
      totalPixels += len;
    }
  }

  const width = 1 + (screenMap.absMax[0] - screenMap.absMin[0]);
  const height = 1 + (screenMap.absMax[1] - screenMap.absMin[1]);
  const screenArea = width * height;
  const pixelDensity = totalPixels / screenArea;

  // Return true if density is close to 1 (indicating a grid)
  return pixelDensity > 0.9 && pixelDensity < 1.1;
}

/**
 * Creates position calculator functions for mapping LED coordinates to screen space
 * @param {Object} frameData - The frame data containing screen mapping information
 * @param {number} screenWidth - The width of the screen in pixels
 * @param {number} screenHeight - The height of the screen in pixels
 * @returns {Object} - Object containing calcXPosition and calcYPosition functions
 */
export function makePositionCalculators(frameData, screenWidth, screenHeight) {
  const { screenMap } = frameData;
  const width = screenMap.absMax[0] - screenMap.absMin[0];
  const height = screenMap.absMax[1] - screenMap.absMin[1];

  return {
    calcXPosition: (x) => {
      return (((x - screenMap.absMin[0]) / width) * screenWidth) - (screenWidth / 2);
    },
    calcYPosition: (y) => {
      const negY = (((y - screenMap.absMin[1]) / height) * screenHeight) - (screenHeight / 2);
      return negY; // Remove negative sign to fix Y-axis orientation
    }
  };
}

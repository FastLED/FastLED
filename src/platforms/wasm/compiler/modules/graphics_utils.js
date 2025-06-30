/* eslint-disable no-console */
/* eslint-disable guard-for-in */
/* eslint-disable no-restricted-syntax */

/**
 * @fileoverview Graphics utility functions for FastLED
 * Provides screen mapping and coordinate calculation functions
 */

/**
 * @typedef {Object} StripData
 * @property {number} id
 * @property {number[]} leds
 * @property {Object} map
 * @property {number[]} map.x
 * @property {number[]} map.y
 * @property {number} [diameter]
 */

/**
 * @typedef {Object} ScreenMapData
 * @property {{ [key: string]: StripData }} strips
 * @property {number[]} [min]
 * @property {number[]} [max]
 */

/**
 * @typedef {Object} FrameData
 * @property {ScreenMapData} screenMap
 */

/**
 * Determines if the LED layout represents a dense grid
 * @param {FrameData} frameData - The frame data containing screen mapping information
 * @returns {boolean} - True if the layout is a dense grid (pixel density close to 1)
 */
export function isDenseGrid(frameData) {
  const { screenMap } = frameData;

  // Check if all pixel densities are undefined
  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
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
 * @param {FrameData} frameData - The frame data containing screen mapping information
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
    },
  };
}

/**
 * Check if all pixel densities are undefined in the screen map
 * @param {FrameData} frameData - Frame data containing screen map
 * @returns {boolean} True if all pixel densities are undefined
 */
function allPixelDensitiesUndefined(frameData) {
  const { screenMap } = frameData;
  if (!screenMap || !screenMap.strips) {
    return true;
  }
  
  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
    const strip = screenMap.strips[stripId];
    allPixelDensitiesUndefined = allPixelDensitiesUndefined && (strip.diameter === undefined);
    if (!allPixelDensitiesUndefined) {
      break;
    }
  }
  return allPixelDensitiesUndefined;
}

/**
 * Check if screen map is a dense screen map (has valid position data)
 * @param {FrameData} frameData - Frame data containing screen map
 * @returns {boolean} True if screen map is dense
 */
function isDenseScreenMap(frameData) {
  if (!frameData || typeof frameData !== 'object') {
    return false;
  }
  
  for (const strip of frameData) {
    if (strip && strip.map && strip.map.x && strip.map.y) {
      return true;
    }
  }
  return false;
}

/**
 * Create screen bounds calculation functions
 * @param {FrameData} frameData - Frame data containing screen map
 * @returns {Object} Object containing coordinate calculation functions
 */
function createScreenBoundsCalculation(frameData) {
  const { screenMap } = frameData;
  if (!screenMap || !screenMap.strips) {
    throw new Error('Invalid frameData: missing screenMap or strips');
  }

  return {
    /**
     * Calculate X position  
     * @param {number} x - X coordinate
     * @returns {number} Calculated X position
     */
    calcXPosition: (x) => {
      return x; // Placeholder implementation
    },
    /**
     * Calculate Y position
     * @param {number} y - Y coordinate  
     * @returns {number} Calculated Y position
     */
    calcYPosition: (y) => {
      return y; // Placeholder implementation
    }
  };
}

export { allPixelDensitiesUndefined, isDenseScreenMap, createScreenBoundsCalculation };

/// <reference path="../types.d.ts" />

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
 * @property {number[]} absMax - Maximum coordinates array
 * @property {number[]} absMin - Minimum coordinates array
 * @property {{ [key: string]: any }} strips - Strip configuration data
 */

/**
 * @typedef {Array & {screenMap?: ScreenMapData}} FrameData
 */

/**
 * Determines if the LED layout represents a dense grid
 * @param {FrameData | (Array & {screenMap?: ScreenMapData})} frameData - The frame data containing screen mapping information
 * @returns {boolean} - True if the layout is a dense grid (pixel density close to 1)
 */
export function isDenseGrid(frameData) {
  // Defensive programming: ensure frameData and screenMap are properly structured
  if (!frameData || typeof frameData !== 'object') {
    console.warn('isDenseGrid: frameData is not a valid object');
    return false;
  }

  const { screenMap } = frameData;

  // Defensive programming: ensure screenMap exists and has expected structure
  if (!screenMap || typeof screenMap !== 'object') {
    console.warn('isDenseGrid: screenMap is not a valid object');
    return false;
  }

  if (!screenMap.strips || typeof screenMap.strips !== 'object') {
    console.warn('isDenseGrid: screenMap.strips is not a valid object');
    return false;
  }

  // Check if all pixel densities are undefined
  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
    const strip = screenMap.strips[stripId];
    if (!strip || typeof strip !== 'object') {
      console.warn(`isDenseGrid: Invalid strip data for strip ${stripId}`);
      continue;
    }
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

  // Defensive programming: ensure frameData is iterable (array-like)
  if (!Array.isArray(frameData)) {
    console.warn('isDenseGrid: frameData is not an array, cannot iterate strips');
    return false;
  }

  for (const strip of frameData) {
    if (!strip || typeof strip !== 'object' || typeof strip.strip_id === 'undefined') {
      console.warn('isDenseGrid: Invalid strip object or missing strip_id');
      continue;
    }

    // Check if this strip exists in screenMap.strips
    const stripIdStr = String(strip.strip_id);
    const stripIdNum = strip.strip_id;

    // Handle both string and numeric keys for compatibility
    let stripMap = null;
    if (stripIdStr in screenMap.strips) {
      stripMap = screenMap.strips[stripIdStr];
    } else if (stripIdNum in screenMap.strips) {
      stripMap = screenMap.strips[stripIdNum];
    }

    if (stripMap && stripMap.map && stripMap.map.x && stripMap.map.y) {
      const len = Math.min(stripMap.map.x.length, stripMap.map.y.length);
      totalPixels += len;
    }
  }

  // Defensive programming: ensure absMax and absMin exist
  if (!screenMap.absMax || !screenMap.absMin
      || !Array.isArray(screenMap.absMax) || !Array.isArray(screenMap.absMin)
      || screenMap.absMax.length < 2 || screenMap.absMin.length < 2) {
    console.warn('isDenseGrid: screenMap missing absMax/absMin arrays');
    return false;
  }

  const width = 1 + (screenMap.absMax[0] - screenMap.absMin[0]);
  const height = 1 + (screenMap.absMax[1] - screenMap.absMin[1]);
  const screenArea = width * height;

  // Avoid division by zero
  if (screenArea <= 0) {
    console.warn('isDenseGrid: Invalid screen area calculation');
    return false;
  }

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
  // Defensive programming: ensure frameData and screenMap exist
  if (!frameData || typeof frameData !== 'object') {
    console.warn('makePositionCalculators: frameData is not a valid object, using default calculations');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  const { screenMap } = frameData;
  if (!screenMap || typeof screenMap !== 'object') {
    console.warn('makePositionCalculators: screenMap is not a valid object, using default calculations');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  // Defensive programming: ensure absMax and absMin exist
  if (!screenMap.absMax || !screenMap.absMin
      || !Array.isArray(screenMap.absMax) || !Array.isArray(screenMap.absMin)
      || screenMap.absMax.length < 2 || screenMap.absMin.length < 2) {
    console.warn('makePositionCalculators: screenMap missing absMax/absMin arrays, using default calculations');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  const width = screenMap.absMax[0] - screenMap.absMin[0];
  const height = screenMap.absMax[1] - screenMap.absMin[1];

  // Avoid division by zero
  if (width <= 0 || height <= 0) {
    console.warn('makePositionCalculators: Invalid width/height calculated, using default calculations');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  return {
    calcXPosition: (x) => (((x - screenMap.absMin[0]) / width) * screenWidth) - (screenWidth / 2),
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
  // Defensive programming: ensure frameData exists and has screenMap
  if (!frameData || typeof frameData !== 'object') {
    console.warn('allPixelDensitiesUndefined: frameData is not a valid object');
    return true;
  }

  const { screenMap } = frameData;
  if (!screenMap || !screenMap.strips || typeof screenMap.strips !== 'object') {
    console.warn('allPixelDensitiesUndefined: screenMap or screenMap.strips not found');
    return true;
  }

  let allPixelDensitiesUndefined = true;
  for (const stripId in screenMap.strips) {
    if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
    const strip = screenMap.strips[stripId];
    if (!strip || typeof strip !== 'object') {
      console.warn(`allPixelDensitiesUndefined: Invalid strip data for strip ${stripId}`);
      continue;
    }
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
  // Defensive programming: ensure frameData and screenMap exist
  if (!frameData || typeof frameData !== 'object') {
    console.warn('createScreenBoundsCalculation: frameData is not a valid object, returning default functions');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  const { screenMap } = frameData;
  if (!screenMap || !screenMap.strips || typeof screenMap.strips !== 'object') {
    console.warn('createScreenBoundsCalculation: Invalid frameData - missing screenMap or strips, returning default functions');
    return {
      calcXPosition: (x) => x,
      calcYPosition: (y) => y,
    };
  }

  return {
    /**
     * Calculate X position
     * @param {number} x - X coordinate
     * @returns {number} Calculated X position
     */
    calcXPosition: (x) => x, // Placeholder implementation
    /**
     * Calculate Y position
     * @param {number} y - Y coordinate
     * @returns {number} Calculated Y position
     */
    calcYPosition: (y) => y // Placeholder implementation
    ,
  };
}

export { allPixelDensitiesUndefined, createScreenBoundsCalculation, isDenseScreenMap };

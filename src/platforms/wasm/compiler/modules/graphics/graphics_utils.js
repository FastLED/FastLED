/// <reference path="../../types.d.ts" />

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
 * @param {ScreenMapData} screenMap - The screen mapping data
 * @returns {boolean} - True if the layout is a dense grid (pixel density close to 1)
 */
export function isDenseGrid(screenMap) {
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

  // Calculate total pixels by iterating screenMap.strips directly
  let totalPixels = 0;

  for (const stripId in screenMap.strips) {
    if (!Object.prototype.hasOwnProperty.call(screenMap.strips, stripId)) continue;
    const stripMap = screenMap.strips[stripId];

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

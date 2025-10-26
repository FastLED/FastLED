/**
 * FastLED Debug Logger
 *
 * Centralized logging utility for debugging the Pure JavaScript Architecture.
 * All log calls use FASTLED_DEBUG_LOG() so they can be easily searched and replaced.
 */

let FASTLED_DEBUG_ENABLED = false;
const FASTLED_DEBUG_PREFIX = '[FASTLED_DEBUG]';

/**
 * Main debug logging function - can be easily search/replaced
 * @param {string} location - Where the log is coming from
 * @param {string} message - Log message
 * @param {...*} args - Additional arguments
 */
function FASTLED_DEBUG_LOG(location, message, ...args) {
  if (!FASTLED_DEBUG_ENABLED) return;

  const timestamp = new Date().toISOString().split('T')[1].slice(0, -1);
  const prefix = `${FASTLED_DEBUG_PREFIX} [${timestamp}] [${location}]`;

  if (args.length === 0) {
    console.log(`${prefix} ${message}`);
  } else {
    console.log(`${prefix} ${message}`, ...args);
  }
}

/**
 * Error logging function
 */
function FASTLED_DEBUG_ERROR(location, message, error) {
  if (!FASTLED_DEBUG_ENABLED) return;

  const timestamp = new Date().toISOString().split('T')[1].slice(0, -1);
  const prefix = `${FASTLED_DEBUG_PREFIX} [${timestamp}] [${location}] ERROR:`;
  console.error(`${prefix} ${message}`, error);
}

/**
 * Function entry/exit tracing
 */
function FASTLED_DEBUG_TRACE(location, functionName, action = 'ENTER', data = null) {
  if (!FASTLED_DEBUG_ENABLED) return;

  const timestamp = new Date().toISOString().split('T')[1].slice(0, -1);
  const prefix = `${FASTLED_DEBUG_PREFIX} [${timestamp}] [${location}] TRACE:`;

  if (data) {
    console.log(`${prefix} ${action} ${functionName}`, data);
  } else {
    console.log(`${prefix} ${action} ${functionName}`);
  }
}

/**
 * Enable/disable debug logging
 */
function setFastLEDDebugEnabled(enabled) {
  FASTLED_DEBUG_ENABLED = enabled;
  FASTLED_DEBUG_LOG('DEBUG_LOGGER', `Debug logging ${enabled ? 'ENABLED' : 'DISABLED'}`);
}

// Export functions
window.FASTLED_DEBUG_LOG = FASTLED_DEBUG_LOG;
window.FASTLED_DEBUG_ERROR = FASTLED_DEBUG_ERROR;
window.FASTLED_DEBUG_TRACE = FASTLED_DEBUG_TRACE;
window.setFastLEDDebugEnabled = setFastLEDDebugEnabled;

export {
  FASTLED_DEBUG_LOG, FASTLED_DEBUG_ERROR, FASTLED_DEBUG_TRACE, setFastLEDDebugEnabled,
};

// Initial startup log
FASTLED_DEBUG_LOG('DEBUG_LOGGER', 'FastLED Debug Logger initialized and ready');

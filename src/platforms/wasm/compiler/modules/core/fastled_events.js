/**
 * FastLED Pure JavaScript Event System
 *
 * This module implements an event-driven architecture that replaces callback chains
 * with clean event patterns. It provides a centralized event system for FastLED
 * components to communicate without tight coupling.
 *
 * Key Features:
 * - Event-driven architecture replacing callback chains
 * - Centralized event management system
 * - Type-safe event interfaces
 * - Performance monitoring and debugging
 * - Clean separation of concerns
 *
 * @module FastLEDEvents
 */

// Import debug logging
import { FASTLED_DEBUG_LOG, FASTLED_DEBUG_TRACE } from './fastled_debug_logger.js';

/**
 * FastLED Event System Class
 * Extends EventTarget to provide custom FastLED-specific events
 */
class FastLEDEventSystem extends EventTarget {
  constructor() {
    FASTLED_DEBUG_TRACE('EVENTS', 'FastLEDEventSystem constructor', 'ENTER');
    super();
    this.eventStats = {
      frameCount: 0,
      stripAdded: 0,
      stripUpdated: 0,
      uiUpdated: 0,
      errors: 0,
    };
    FASTLED_DEBUG_LOG('EVENTS', 'Event stats initialized', this.eventStats);

    this.setupEventListeners();
    console.log('FastLED Event System initialized');
    FASTLED_DEBUG_LOG('EVENTS', 'FastLED Event System initialized');
    FASTLED_DEBUG_TRACE('EVENTS', 'FastLEDEventSystem constructor', 'EXIT');
  }

  /**
     * Set up default event listeners for FastLED events
     * Note: Circular event listeners removed - only worker events are used for thread communication
     */
  setupEventListeners() {
    // Intentionally empty - worker events are registered directly by consumers
    // This prevents circular dependencies and unnecessary event overhead
  }

  /**
     * Emit a strip added event
     * @param {number} stripId - Strip identifier
     * @param {number} numLeds - Number of LEDs in strip
     */
  emitStripAdded(stripId, numLeds) {
    this.dispatchEvent(new CustomEvent('strip_added', {
      detail: { stripId, numLeds, timestamp: Date.now() },
    }));
  }

  /**
     * Emit a strip update event
     * @param {Object} stripData - Strip update data
     */
  emitStripUpdate(stripData) {
    this.dispatchEvent(new CustomEvent('strip_update', {
      detail: { stripData, timestamp: Date.now() },
    }));
  }

  /**
     * Emit a frame ready event
     * @param {Array} frameData - Frame data with pixel information
     */
  emitFrameReady(frameData) {
    this.dispatchEvent(new CustomEvent('frame_ready', {
      detail: { frameData, timestamp: Date.now() },
    }));
  }

  /**
     * Emit a UI update event
     * @param {Object} uiData - UI element data
     */
  emitUiUpdate(uiData) {
    this.dispatchEvent(new CustomEvent('ui_update', {
      detail: { uiData, timestamp: Date.now() },
    }));
  }

  /**
     * Generic emit method for custom events
     * @param {string} eventName - Name of the event
     * @param {Object} [eventData] - Event data
     */
  emit(eventName, eventData = {}) {
    this.dispatchEvent(new CustomEvent(eventName, {
      detail: { ...eventData, timestamp: Date.now() },
    }));
  }

  /**
   * Convenience method for addEventListener with detail unwrapping
   * Provides a simpler API similar to Node.js EventEmitter
   * @param {string} eventName - Name of the event to listen for
   * @param {Function} callback - Callback function that receives event detail
   */
  on(eventName, callback) {
    this.addEventListener(eventName, (event) => {
      // Unwrap the detail object for convenience
      callback(event.detail);
    });
  }

  /**
     * Emit an error event
     * @param {string} errorType - Type of error
     * @param {string} errorMessage - Error message
     * @param {Object} [errorData] - Additional error data
     */
  emitError(errorType, errorMessage, errorData = null) {
    this.dispatchEvent(new CustomEvent('error', {
      detail: {
        errorType, errorMessage, errorData, timestamp: Date.now(),
      },
    }));
  }

  /**
     * Emit a performance warning event
     * @param {string} metric - Performance metric name
     * @param {number} value - Current value
     * @param {number} threshold - Warning threshold
     */
  emitPerformanceWarning(metric, value, threshold) {
    this.dispatchEvent(new CustomEvent('performance_warning', {
      detail: {
        metric, value, threshold, timestamp: Date.now(),
      },
    }));
  }

  /**
     * Get event statistics
     * @returns {Object} Event statistics object
     */
  getEventStats() {
    return {
      ...this.eventStats,
      uptime: Date.now() - this.startTime,
    };
  }

  /**
     * Reset event statistics
     */
  resetEventStats() {
    this.eventStats = {
      frameCount: 0,
      stripAdded: 0,
      stripUpdated: 0,
      uiUpdated: 0,
      errors: 0,
    };
    this.startTime = Date.now();
    console.log('FastLED event statistics reset');
  }

  /**
     * Add a custom event listener with error handling
     * @param {string} eventType - Type of event to listen for
     * @param {Function} handler - Event handler function
     * @param {Object} [options] - Event listener options
     */
  addSafeEventListener(eventType, handler, options = {}) {
    const safeHandler = (event) => {
      try {
        handler(event);
      } catch (error) {
        console.error(`Error in ${eventType} event handler:`, error);
        this.emitError(`${eventType}_handler`, error.message, event.detail);
      }
    };

    this.addEventListener(eventType, safeHandler, options);
    console.log(`Safe event listener added for ${eventType}`);
  }

  /**
     * Monitor performance metrics and emit warnings
     * @param {string} metric - Metric name
     * @param {number} value - Current value
     * @param {number} threshold - Warning threshold
     */
  monitorPerformance(metric, value, threshold) {
    if (value > threshold) {
      this.emitPerformanceWarning(metric, value, threshold);
    }
  }

  /**
     * Enable or disable debug logging for events
     * @param {boolean} enabled - Whether to enable debug logging
     */
  setDebugMode(enabled) {
    this.debugMode = enabled;

    if (enabled) {
      // Add debug listeners for all events
      ['strip_added', 'strip_update', 'frame_ready', 'ui_update', 'error', 'performance_warning'].forEach((eventType) => {
        this.addEventListener(eventType, (event) => {
          console.debug(`[FastLED Event Debug] ${eventType}:`, event.detail);
        });
      });
      console.log('FastLED event debug mode enabled');
    } else {
      console.log('FastLED event debug mode disabled');
    }
  }
}

/**
 * Performance Monitor Class
 * Monitors FastLED performance and emits warnings via the event system
 */
class FastLEDPerformanceMonitor {
  constructor(eventSystem) {
    this.eventSystem = eventSystem;
    this.metrics = {
      frameTime: { values: [], maxHistory: 60 },
      memoryUsage: { values: [], maxHistory: 10 },
      eventRate: { values: [], maxHistory: 30 },
    };
    this.thresholds = {
      frameTime: 32, // 32ms = ~30fps
      memoryUsage: 100 * 1024 * 1024, // 100MB
      eventRate: 1000, // 1000 events per second
    };
    this.startTime = Date.now();
  }

  /**
     * Record a frame time measurement
     * @param {number} frameTime - Frame time in milliseconds
     */
  recordFrameTime(frameTime) {
    this.recordMetric('frameTime', frameTime);
  }

  /**
     * Record memory usage measurement
     * @param {number} memoryUsage - Memory usage in bytes
     */
  recordMemoryUsage(memoryUsage) {
    this.recordMetric('memoryUsage', memoryUsage);
  }

  /**
     * Record event rate measurement
     * @param {number} eventRate - Events per second
     */
  recordEventRate(eventRate) {
    this.recordMetric('eventRate', eventRate);
  }

  /**
     * Record a metric value and check against thresholds
     * @param {string} metricName - Name of the metric
     * @param {number} value - Metric value
     */
  recordMetric(metricName, value) {
    const metric = this.metrics[metricName];
    if (!metric) return;

    metric.values.push(value);
    if (metric.values.length > metric.maxHistory) {
      metric.values.shift();
    }

    // Check threshold
    const threshold = this.thresholds[metricName];
    if (value > threshold) {
      this.eventSystem.monitorPerformance(metricName, value, threshold);
    }
  }

  /**
     * Get average value for a metric
     * @param {string} metricName - Name of the metric
     * @returns {number} Average value
     */
  getAverageMetric(metricName) {
    const metric = this.metrics[metricName];
    if (!metric || metric.values.length === 0) return 0;

    return metric.values.reduce((a, b) => a + b, 0) / metric.values.length;
  }

  /**
     * Get performance summary
     * @returns {Object} Performance summary
     */
  getPerformanceSummary() {
    return {
      uptime: Date.now() - this.startTime,
      averageFrameTime: this.getAverageMetric('frameTime'),
      averageMemoryUsage: this.getAverageMetric('memoryUsage'),
      averageEventRate: this.getAverageMetric('eventRate'),
      eventStats: this.eventSystem.getEventStats(),
    };
  }
}

// Create global event system instance
const fastLEDEvents = new FastLEDEventSystem();
const fastLEDPerformanceMonitor = new FastLEDPerformanceMonitor(fastLEDEvents);

// Mark start time
fastLEDEvents.startTime = Date.now();

// Expose globally for access from other modules
window.fastLEDEvents = fastLEDEvents;
window.fastLEDPerformanceMonitor = fastLEDPerformanceMonitor;

// Export for ES modules
export {
  FastLEDEventSystem, FastLEDPerformanceMonitor, fastLEDEvents, fastLEDPerformanceMonitor,
};

console.log('FastLED Event System and Performance Monitor initialized and exposed globally');

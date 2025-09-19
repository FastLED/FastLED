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
     */
  setupEventListeners() {
    // Strip events
    this.addEventListener('strip_added', (event) => {
      this.eventStats.stripAdded++;
      const { stripId, numLeds } = event.detail;
      if (globalThis.FastLED_onStripAdded) {
        globalThis.FastLED_onStripAdded(stripId, numLeds).catch((error) => {
          console.error('Error in strip_added event handler:', error);
          this.emitError('strip_added_handler', error.message, { stripId, numLeds });
        });
      }
    });

    // Strip update events
    this.addEventListener('strip_update', (event) => {
      this.eventStats.stripUpdated++;
      const { stripData } = event.detail;
      if (globalThis.FastLED_onStripUpdate) {
        globalThis.FastLED_onStripUpdate(stripData).catch((error) => {
          console.error('Error in strip_update event handler:', error);
          this.emitError('strip_update_handler', error.message, stripData);
        });
      }
    });

    // Frame events
    this.addEventListener('frame_ready', (event) => {
      this.eventStats.frameCount++;
      const { frameData } = event.detail;
      if (globalThis.FastLED_onFrame) {
        globalThis.FastLED_onFrame(frameData).catch((error) => {
          console.error('Error in frame_ready event handler:', error);
          this.emitError('frame_handler', error.message, { frameDataLength: frameData.length });
        });
      }
    });

    // UI events
    this.addEventListener('ui_update', (event) => {
      this.eventStats.uiUpdated++;
      const { uiData } = event.detail;
      if (globalThis.FastLED_onUiElementsAdded) {
        globalThis.FastLED_onUiElementsAdded(uiData).catch((error) => {
          console.error('Error in ui_update event handler:', error);
          this.emitError('ui_update_handler', error.message, uiData);
        });
      }
    });

    // Error events
    this.addEventListener('error', (event) => {
      this.eventStats.errors++;
      const { errorType, errorMessage, errorData } = event.detail;
      console.error(`FastLED Event Error [${errorType}]: ${errorMessage}`, errorData);

      // Call global error handler if available
      if (globalThis.FastLED_onError) {
        globalThis.FastLED_onError(errorType, errorMessage, errorData).catch((error) => {
          console.error('Error in error event handler:', error);
          // Don't emit another error to avoid loops
        });
      }
    });

    // Performance monitoring events
    this.addEventListener('performance_warning', (event) => {
      const { metric, value, threshold } = event.detail;
      console.warn(`FastLED Performance Warning: ${metric} = ${value} (threshold: ${threshold})`);
    });

    console.log('FastLED event listeners set up successfully');
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

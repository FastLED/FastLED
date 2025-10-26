/**
 * FastLED Resize Coordinator
 *
 * Coordinated resize event handling with debouncing and performance optimization.
 * Prevents race conditions and ensures smooth resize operations.
 *
 * @module ResizeCoordinator
 */

/* eslint-disable no-console */

/**
 * @typedef {Object} ResizeEvent
 * @property {number} width - New width
 * @property {number} height - New height
 * @property {number} timestamp - Event timestamp
 * @property {string} source - Event source identifier
 */

/**
 * @typedef {Object} ResizeMetrics
 * @property {number} totalResizes - Total resize events handled
 * @property {number} throttledResizes - Resizes that were throttled
 * @property {number} lastResizeTime - Timestamp of last resize
 * @property {number} averageResizeInterval - Average time between resizes
 */

/**
 * Coordinates resize events across components
 */
export class ResizeCoordinator {
    /**
     * Default configuration
     * @type {Object}
     */
    static DEFAULT_CONFIG = {
        debounceDelay: 150,        // Milliseconds to wait before processing resize
        throttleDelay: 50,         // Minimum time between resize events
        maxPendingResizes: 3,      // Maximum pending resize events
        enableMetrics: true,       // Track resize metrics
        enableLogging: false       // Enable debug logging
    };

    /**
     * @param {Object} [config] - Optional configuration overrides
     */
    constructor(config = {}) {
        /** @type {Object} */
        this.config = { ...ResizeCoordinator.DEFAULT_CONFIG, ...config };

        /** @type {Map<string, Function>} */
        this.handlers = new Map();

        /** @type {number|NodeJS.Timeout|null} */
        this.debounceTimer = null;

        /** @type {number|NodeJS.Timeout|null} */
        this.throttleTimer = null;

        /** @type {ResizeEvent|null} */
        this.pendingResize = null;

        /** @type {ResizeEvent|null} */
        this.lastResize = null;

        /** @type {boolean} */
        this.isProcessing = false;

        /** @type {Set<string>} */
        this.activeComponents = new Set();

        /** @type {ResizeMetrics} */
        this.metrics = {
            totalResizes: 0,
            throttledResizes: 0,
            lastResizeTime: 0,
            averageResizeInterval: 0
        };

        /** @type {Array<number>} */
        this.resizeIntervals = [];

        /** @type {boolean} */
        this.isPaused = false;

        this.setupWindowListener();
    }

    /**
     * Sets up global window resize listener
     * @private
     */
    setupWindowListener() {
        if (typeof window !== 'undefined') {
            const handleResize = () => {
                if (!this.isPaused) {
                    this.handleResize(
                        window.innerWidth,
                        window.innerHeight,
                        'window'
                    );
                }
            };

            window.addEventListener('resize', handleResize);

            // Store reference for cleanup
            this._windowResizeHandler = handleResize;
        }
    }

    /**
     * Handles resize event with debouncing
     * @param {number} width - New width
     * @param {number} height - New height
     * @param {string} [source='unknown'] - Event source
     */
    handleResize(width, height, source = 'unknown') {
        if (this.isPaused) {
            return;
        }

        const now = Date.now();
        const resizeEvent = {
            width,
            height,
            timestamp: now,
            source
        };

        // Update metrics
        if (this.config.enableMetrics) {
            this.updateMetrics(now);
        }

        // Check throttling
        if (this.isThrottled()) {
            this.metrics.throttledResizes++;
            this.pendingResize = resizeEvent;
            return;
        }

        // Clear existing debounce timer
        if (this.debounceTimer) {
            clearTimeout(this.debounceTimer);
        }

        // Store pending resize
        this.pendingResize = resizeEvent;

        // Start debounce timer
        this.debounceTimer = setTimeout(() => {
            this.processResize();
        }, this.config.debounceDelay);

        if (this.config.enableLogging) {
            console.log('ResizeCoordinator: Resize event received', resizeEvent);
        }
    }

    /**
     * Checks if resize events are throttled
     * @returns {boolean}
     * @private
     */
    isThrottled() {
        if (!this.lastResize) {
            return false;
        }

        const timeSinceLastResize = Date.now() - this.lastResize.timestamp;
        return timeSinceLastResize < this.config.throttleDelay;
    }

    /**
     * Processes pending resize event
     * @private
     */
    async processResize() {
        if (!this.pendingResize || this.isProcessing) {
            return;
        }

        this.isProcessing = true;
        const resizeEvent = this.pendingResize;
        this.pendingResize = null;
        this.lastResize = resizeEvent;

        if (this.config.enableLogging) {
            console.log('ResizeCoordinator: Processing resize', resizeEvent);
        }

        // Notify all handlers
        const promises = [];
        for (const [componentId, handler] of this.handlers) {
            if (!this.activeComponents.has(componentId)) {
                continue;
            }

            try {
                const result = handler(resizeEvent);
                if (result instanceof Promise) {
                    promises.push(
                        result.catch(error => {
                            console.error(
                                `ResizeCoordinator: Handler error for ${componentId}`,
                                error
                            );
                        })
                    );
                }
            } catch (error) {
                console.error(
                    `ResizeCoordinator: Handler error for ${componentId}`,
                    error
                );
            }
        }

        // Wait for all async handlers
        if (promises.length > 0) {
            await Promise.all(promises);
        }

        this.isProcessing = false;

        // Process any pending resize that came in during processing
        if (this.pendingResize) {
            setTimeout(() => this.processResize(), 0);
        }
    }

    /**
     * Updates resize metrics
     * @param {number} timestamp - Current timestamp
     * @private
     */
    updateMetrics(timestamp) {
        this.metrics.totalResizes++;

        if (this.metrics.lastResizeTime > 0) {
            const interval = timestamp - this.metrics.lastResizeTime;
            this.resizeIntervals.push(interval);

            // Keep only last 100 intervals
            if (this.resizeIntervals.length > 100) {
                this.resizeIntervals.shift();
            }

            // Calculate average
            const sum = this.resizeIntervals.reduce((a, b) => a + b, 0);
            this.metrics.averageResizeInterval = Math.round(
                sum / this.resizeIntervals.length
            );
        }

        this.metrics.lastResizeTime = timestamp;
    }

    /**
     * Registers a resize handler for a component
     * @param {string} componentId - Component identifier
     * @param {Function} handler - Handler function
     * @param {boolean} [autoActivate=true] - Auto-activate component
     */
    registerHandler(componentId, handler, autoActivate = true) {
        this.handlers.set(componentId, handler);

        if (autoActivate) {
            this.activateComponent(componentId);
        }

        if (this.config.enableLogging) {
            console.log(`ResizeCoordinator: Registered handler for ${componentId}`);
        }
    }

    /**
     * Unregisters a resize handler
     * @param {string} componentId - Component identifier
     */
    unregisterHandler(componentId) {
        this.handlers.delete(componentId);
        this.activeComponents.delete(componentId);

        if (this.config.enableLogging) {
            console.log(`ResizeCoordinator: Unregistered handler for ${componentId}`);
        }
    }

    /**
     * Activates a component for resize events
     * @param {string} componentId - Component identifier
     */
    activateComponent(componentId) {
        if (this.handlers.has(componentId)) {
            this.activeComponents.add(componentId);
        }
    }

    /**
     * Deactivates a component from resize events
     * @param {string} componentId - Component identifier
     */
    deactivateComponent(componentId) {
        this.activeComponents.delete(componentId);
    }

    /**
     * Forces an immediate resize event
     * @param {number} [width] - Width (defaults to window width)
     * @param {number} [height] - Height (defaults to window height)
     */
    forceResize(width, height) {
        const w = width ?? (typeof window !== 'undefined' ? window.innerWidth : 0);
        const h = height ?? (typeof window !== 'undefined' ? window.innerHeight : 0);

        // Clear any pending resize
        if (this.debounceTimer) {
            clearTimeout(this.debounceTimer);
            this.debounceTimer = null;
        }

        // Process immediately
        this.pendingResize = {
            width: w,
            height: h,
            timestamp: Date.now(),
            source: 'forced'
        };

        this.processResize();
    }

    /**
     * Pauses resize handling
     */
    pause() {
        this.isPaused = true;

        if (this.debounceTimer) {
            clearTimeout(this.debounceTimer);
            this.debounceTimer = null;
        }

        if (this.config.enableLogging) {
            console.log('ResizeCoordinator: Paused');
        }
    }

    /**
     * Resumes resize handling
     */
    resume() {
        this.isPaused = false;

        if (this.config.enableLogging) {
            console.log('ResizeCoordinator: Resumed');
        }
    }

    /**
     * Gets current metrics
     * @returns {ResizeMetrics}
     */
    getMetrics() {
        return { ...this.metrics };
    }

    /**
     * Resets metrics
     */
    resetMetrics() {
        this.metrics = {
            totalResizes: 0,
            throttledResizes: 0,
            lastResizeTime: 0,
            averageResizeInterval: 0
        };
        this.resizeIntervals = [];
    }

    /**
     * Updates configuration
     * @param {Object} config - Configuration updates
     */
    updateConfig(config) {
        this.config = { ...this.config, ...config };
    }

    /**
     * Cleanup and destroy
     */
    destroy() {
        // Clear timers
        if (this.debounceTimer) {
            clearTimeout(this.debounceTimer);
        }
        if (this.throttleTimer) {
            clearTimeout(this.throttleTimer);
        }

        // Remove window listener
        if (typeof window !== 'undefined' && this._windowResizeHandler) {
            window.removeEventListener('resize', this._windowResizeHandler);
        }

        // Clear handlers
        this.handlers.clear();
        this.activeComponents.clear();

        if (this.config.enableLogging) {
            console.log('ResizeCoordinator: Destroyed');
        }
    }
}
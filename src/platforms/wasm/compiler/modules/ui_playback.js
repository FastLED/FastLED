/**
 * FastLED UI Playback Module
 *
 * Replays recorded UI events to recreate UI element interactions.
 * Works with recordings from UIRecorder to simulate user interactions.
 *
 * Features:
 * - Event-by-event playback with timing
 * - Speed control (1x, 2x, 0.5x, etc.)
 * - Pause/resume functionality
 * - Skip to timestamp
 * - Element state validation
 * - Playback progress tracking
 *
 * @module UIPlayback
 */

/* eslint-disable no-console */

/**
 * @typedef {Object} PlaybackOptions
 * @property {number} [speed=1.0] - Playback speed multiplier
 * @property {boolean} [validateElements=true] - Validate elements exist before applying changes
 * @property {boolean} [debugMode=false] - Enable debug logging
 * @property {number} [maxEvents=10000] - Maximum events to process
 */

/**
 * @typedef {Object} PlaybackStatus
 * @property {boolean} isPlaying - Whether playback is active
 * @property {boolean} isPaused - Whether playback is paused
 * @property {number} currentTimestamp - Current playback timestamp
 * @property {number} totalDuration - Total recording duration
 * @property {number} currentEventIndex - Current event index
 * @property {number} totalEvents - Total number of events
 * @property {number} progress - Playback progress (0-1)
 * @property {number} speed - Current playback speed
 */

/**
 * Handles playback of recorded UI events
 */
export class UIPlayback {
    /**
     * @param {Object} uiManager - Reference to JsonUiManager instance
     * @param {PlaybackOptions} [options] - Playback configuration
     */
    constructor(uiManager, options = {}) {
        /** @type {Object} */
        this.uiManager = uiManager;

        /** @type {PlaybackOptions} */
        this.options = {
            speed: 1.0,
            validateElements: true,
            debugMode: false,
            maxEvents: 10000,
            ...options
        };

        /** @type {Object|null} */
        this.recording = null;

        /** @type {boolean} */
        this.isPlaying = false;

        /** @type {boolean} */
        this.isPaused = false;

        /** @type {number} */
        this.currentEventIndex = 0;

        /** @type {number} */
        this.currentTimestamp = 0;

        /** @type {number} */
        this.playbackStartTime = 0;

        /** @type {number} */
        this.pausedDuration = 0;

        /** @type {number|NodeJS.Timeout|null} */
        this.playbackTimer = null;

        /** @type {Set<Function>} */
        this.eventListeners = new Set();

        /** @type {Map<string, Object>} */
        this.elementStates = new Map();

        this.log('UIPlayback initialized');
    }

    /**
     * Loads a recording for playback
     * @param {Object} recording - Recording data from UIRecorder
     * @returns {boolean} Success status
     */
    loadRecording(recording) {
        try {
            if (!this.validateRecording(recording)) {
                throw new Error('Invalid recording format');
            }

            this.recording = recording;
            this.currentEventIndex = 0;
            this.currentTimestamp = 0;
            this.elementStates.clear();

            this.log(`Loaded recording: ${recording.events.length} events, ${recording.recording.metadata.totalDuration}ms`);
            this.notifyListeners('recordingLoaded', { recording });

            return true;
        } catch (error) {
            console.error('Failed to load recording:', error);
            return false;
        }
    }

    /**
     * Starts playback of the loaded recording
     * @returns {boolean} Success status
     */
    startPlayback() {
        if (!this.recording) {
            console.error('No recording loaded');
            return false;
        }

        if (this.isPlaying) {
            this.log('Playback already in progress');
            return true;
        }

        this.isPlaying = true;
        this.isPaused = false;
        this.playbackStartTime = Date.now();
        this.pausedDuration = 0;

        // Reset to beginning if we were at the end
        if (this.currentEventIndex >= this.recording.events.length) {
            this.currentEventIndex = 0;
            this.currentTimestamp = 0;
        }

        this.scheduleNextEvent();

        this.log(`Playback started from event ${this.currentEventIndex} at speed ${this.options.speed}x`);
        this.notifyListeners('playbackStarted', this.getStatus());

        return true;
    }

    /**
     * Pauses playback
     */
    pausePlayback() {
        if (!this.isPlaying || this.isPaused) {
            return;
        }

        this.isPaused = true;
        this.pausedDuration += Date.now() - this.playbackStartTime;

        if (this.playbackTimer) {
            clearTimeout(this.playbackTimer);
            this.playbackTimer = null;
        }

        this.log('Playback paused');
        this.notifyListeners('playbackPaused', this.getStatus());
    }

    /**
     * Resumes paused playback
     */
    resumePlayback() {
        if (!this.isPlaying || !this.isPaused) {
            return;
        }

        this.isPaused = false;
        this.playbackStartTime = Date.now();

        this.scheduleNextEvent();

        this.log('Playback resumed');
        this.notifyListeners('playbackResumed', this.getStatus());
    }

    /**
     * Stops playback
     */
    stopPlayback() {
        if (!this.isPlaying) {
            return;
        }

        this.isPlaying = false;
        this.isPaused = false;
        this.currentEventIndex = 0;
        this.currentTimestamp = 0;
        this.pausedDuration = 0;

        if (this.playbackTimer) {
            clearTimeout(this.playbackTimer);
            this.playbackTimer = null;
        }

        this.log('Playback stopped');
        this.notifyListeners('playbackStopped', this.getStatus());
    }

    /**
     * Seeks to a specific timestamp
     * @param {number} timestamp - Target timestamp in milliseconds
     */
    seekToTimestamp(timestamp) {
        if (!this.recording) {
            return;
        }

        const events = this.recording.events;
        let targetIndex = 0;

        // Find the event index for the target timestamp
        for (let i = 0; i < events.length; i++) {
            if (events[i].timestamp <= timestamp) {
                targetIndex = i;
            } else {
                break;
            }
        }

        // Replay all events up to the target timestamp
        this.replayToIndex(targetIndex);
        this.currentTimestamp = timestamp;

        this.log(`Seeked to timestamp ${timestamp}ms (event ${targetIndex})`);
        this.notifyListeners('playbackSeeked', this.getStatus());

        // Resume playback if we were playing
        if (this.isPlaying && !this.isPaused) {
            this.scheduleNextEvent();
        }
    }

    /**
     * Sets playback speed
     * @param {number} speed - Speed multiplier (1.0 = normal speed)
     */
    setPlaybackSpeed(speed) {
        const MIN_SPEED = 0.1;
        const MAX_SPEED = 10.0;
        this.options.speed = Math.max(MIN_SPEED, Math.min(MAX_SPEED, speed));

        this.log(`Playback speed set to ${this.options.speed}x`);
        this.notifyListeners('speedChanged', { speed: this.options.speed });

        // Reschedule next event if playing
        if (this.isPlaying && !this.isPaused) {
            if (this.playbackTimer) {
                clearTimeout(this.playbackTimer);
            }
            this.scheduleNextEvent();
        }
    }

    /**
     * Schedules the next event for playback
     * @private
     */
    scheduleNextEvent() {
        if (!this.recording || !this.isPlaying || this.isPaused) {
            return;
        }

        const events = this.recording.events;

        if (this.currentEventIndex >= events.length) {
            // Playback complete
            this.isPlaying = false;
            this.log('Playback completed');
            this.notifyListeners('playbackCompleted', this.getStatus());
            return;
        }

        const nextEvent = events[this.currentEventIndex];
        const elapsed = Date.now() - this.playbackStartTime + this.pausedDuration;
        const targetTime = (nextEvent.timestamp - this.currentTimestamp) / this.options.speed;
        const delay = Math.max(0, targetTime - elapsed);

        this.playbackTimer = setTimeout(() => {
            this.executeEvent(nextEvent);
            this.currentEventIndex++;
            this.currentTimestamp = nextEvent.timestamp;
            this.scheduleNextEvent();
        }, delay);
    }

    /**
     * Executes a single UI event
     * @param {Object} event - UI event to execute
     * @private
     */
    executeEvent(event) {
        this.log(`Executing ${event.type} event for ${event.elementId}`);

        switch (event.type) {
            case 'add':
                this.executeAddEvent(event);
                break;
            case 'update':
                this.executeUpdateEvent(event);
                break;
            case 'remove':
                this.executeRemoveEvent(event);
                break;
            default:
                console.warn(`Unknown event type: ${event.type}`);
        }

        this.notifyListeners('eventExecuted', { event, status: this.getStatus() });
    }

    /**
     * Executes an element addition event
     * @param {Object} event - Add event
     * @private
     */
    executeAddEvent(event) {
        const { elementId, data } = event;

        if (data.elementConfig) {
            // Store the element configuration for reference
            this.elementStates.set(elementId, {
                config: data.elementConfig,
                addedAt: event.timestamp
            });

            this.log(`Element ${elementId} marked as added (${data.elementType})`);
        }
    }

    /**
     * Executes an element update event
     * @param {Object} event - Update event
     * @private
     */
    executeUpdateEvent(event) {
        const { elementId, data } = event;

        if (this.options.validateElements && this.uiManager.uiElements) {
            const element = this.uiManager.uiElements[elementId];
            if (!element) {
                this.log(`Element ${elementId} not found for update`);
                return;
            }

            // Apply the update to the actual UI element
            this.applyElementUpdate(element, data.value);
        }

        // Update our state tracking
        const elementState = this.elementStates.get(elementId) || {};
        elementState.lastValue = data.value;
        elementState.lastUpdate = event.timestamp;
        this.elementStates.set(elementId, elementState);

        this.log(`Updated ${elementId} = ${data.value}`);
    }

    /**
     * Executes an element removal event
     * @param {Object} event - Remove event
     * @private
     */
    executeRemoveEvent(event) {
        const { elementId } = event;

        // Remove from our state tracking
        this.elementStates.delete(elementId);

        this.log(`Element ${elementId} marked as removed`);
    }

    /**
     * Applies an update to a UI element
     * @param {HTMLElement} element - DOM element to update
     * @param {*} value - New value
     * @private
     */
    applyElementUpdate(element, value) {
        const inputElement = /** @type {HTMLInputElement} */ (element);
        const selectElement = /** @type {HTMLSelectElement} */ (element);

        if (inputElement.type === 'checkbox') {
            inputElement.checked = Boolean(value);
        } else if (inputElement.type === 'range') {
            inputElement.value = value;
            // Update display value if present
            const valueDisplay = element.parentElement?.querySelector('.slider-value');
            if (valueDisplay) {
                valueDisplay.textContent = value;
            }
        } else if (inputElement.type === 'number') {
            inputElement.value = value;
        } else if (element.tagName === 'SELECT') {
            selectElement.selectedIndex = value;
        } else if (inputElement.type === 'submit') {
            element.setAttribute('data-pressed', value ? 'true' : 'false');
            element.classList.toggle('active', Boolean(value));
        } else {
            inputElement.value = value;
        }

        // Trigger change event to notify other systems
        element.dispatchEvent(new Event('change', { bubbles: true }));
    }

    /**
     * Replays all events up to a specific index
     * @param {number} targetIndex - Target event index
     * @private
     */
    replayToIndex(targetIndex) {
        if (!this.recording) {
            return;
        }

        // Clear current state
        this.elementStates.clear();

        // Replay events from beginning to target
        for (let i = 0; i <= targetIndex && i < this.recording.events.length; i++) {
            const event = this.recording.events[i];
            this.executeEvent(event);
        }

        this.currentEventIndex = targetIndex + 1;
    }

    /**
     * Gets current playback status
     * @returns {PlaybackStatus}
     */
    getStatus() {
        const totalDuration = this.recording?.recording.metadata.totalDuration || 0;
        const totalEvents = this.recording?.events.length || 0;

        return {
            isPlaying: this.isPlaying,
            isPaused: this.isPaused,
            currentTimestamp: this.currentTimestamp,
            totalDuration,
            currentEventIndex: this.currentEventIndex,
            totalEvents,
            progress: totalDuration > 0 ? this.currentTimestamp / totalDuration : 0,
            speed: this.options.speed
        };
    }

    /**
     * Validates recording format
     * @param {Object} recording - Recording to validate
     * @returns {boolean} Valid status
     * @private
     */
    validateRecording(recording) {
        return recording
            && recording.recording
            && recording.recording.version
            && Array.isArray(recording.events);
    }

    /**
     * Adds event listener
     * @param {Function} listener - Event listener function
     */
    addEventListener(listener) {
        this.eventListeners.add(listener);
    }

    /**
     * Removes event listener
     * @param {Function} listener - Event listener function
     */
    removeEventListener(listener) {
        this.eventListeners.delete(listener);
    }

    /**
     * Notifies all listeners of playback events
     * @param {string} eventType - Type of event
     * @param {Object} data - Event data
     * @private
     */
    notifyListeners(eventType, data) {
        for (const listener of this.eventListeners) {
            try {
                listener({ type: eventType, ...data });
            } catch (error) {
                console.error('UIPlayback listener error:', error);
            }
        }
    }

    /**
     * Logs debug messages if debug mode is enabled
     * @param {string} message - Message to log
     * @private
     */
    log(message) {
        if (this.options.debugMode) {
            console.log(`[UIPlayback] ${message}`);
        }
    }

    /**
     * Enables debug mode
     */
    enableDebugMode() {
        this.options.debugMode = true;
        this.log('Debug mode enabled');
    }

    /**
     * Disables debug mode
     */
    disableDebugMode() {
        this.options.debugMode = false;
    }

    /**
     * Destroys the playback instance
     */
    destroy() {
        this.stopPlayback();
        this.eventListeners.clear();
        this.elementStates.clear();
        this.recording = null;

        this.log('UIPlayback destroyed');
    }
}

// Global playback controls
if (typeof window !== 'undefined') {
    window.UIPlayback = UIPlayback;

    // Global playback functions
    window.loadUIRecordingForPlayback = function(recording) {
        if (!window.uiPlayback && window.uiManager) {
            window.uiPlayback = new UIPlayback(window.uiManager, { debugMode: true });
        }
        if (window.uiPlayback) {
            return window.uiPlayback.loadRecording(recording);
        }
        return false;
    };

    window.startUIPlayback = function() {
        if (window.uiPlayback) {
            return window.uiPlayback.startPlayback();
        }
        return false;
    };

    window.pauseUIPlayback = function() {
        if (window.uiPlayback) {
            window.uiPlayback.pausePlayback();
        }
    };

    window.resumeUIPlayback = function() {
        if (window.uiPlayback) {
            window.uiPlayback.resumePlayback();
        }
    };

    window.stopUIPlayback = function() {
        if (window.uiPlayback) {
            window.uiPlayback.stopPlayback();
        }
    };

    window.setUIPlaybackSpeed = function(speed) {
        if (window.uiPlayback) {
            window.uiPlayback.setPlaybackSpeed(speed);
        }
    };

    window.getUIPlaybackStatus = function() {
        if (window.uiPlayback) {
            return window.uiPlayback.getStatus();
        }
        return { isPlaying: false, progress: 0 };
    };
}
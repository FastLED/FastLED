/**
 * FastLED UI Recorder Module
 *
 * Records UI element changes (add, update, remove) as JSON events for playback and analysis.
 * Integrates with JsonUiManager to capture all UI state modifications.
 *
 * Features:
 * - Records element addition, updates, and removal
 * - JSON event stream with timestamps
 * - Playback functionality for recorded sessions
 * - Export/import recording data
 * - Automatic cleanup and memory management
 *
 * @module UIRecorder
 */

/* eslint-disable no-console */

/**
 * @typedef {'add'|'update'|'remove'} UIEventType
 */

/**
 * @typedef {Object} UIEvent
 * @property {number} timestamp - Milliseconds from recording start
 * @property {UIEventType} type - Type of UI event
 * @property {string} elementId - ID of the UI element
 * @property {Object} data - Event-specific data
 * @property {string} [data.elementType] - Type of element (for add events)
 * @property {*} [data.value] - Element value (for update events)
 * @property {*} [data.previousValue] - Previous value (for update events)
 * @property {Object} [data.elementConfig] - Full element configuration (for add events)
 * @property {Object} [data.elementSnapshot] - Element snapshot (for remove events)
 */

/**
 * @typedef {Object} UIRecording
 * @property {Object} recording - Recording metadata
 * @property {string} recording.version - Recording format version
 * @property {string} recording.startTime - ISO8601 start timestamp
 * @property {string} [recording.endTime] - ISO8601 end timestamp
 * @property {Object} recording.metadata - Additional metadata
 * @property {string} recording.metadata.recordingId - Unique recording ID
 * @property {string} recording.metadata.layoutMode - Layout mode when recorded
 * @property {number} [recording.metadata.totalDuration] - Total duration in ms
 * @property {UIEvent[]} events - Array of UI events
 */

/**
 * Records UI element changes for playback and analysis
 */
export class UIRecorder {
    /** Constants for ID generation */
    static ID_GENERATION = {
        BASE36_RADIX: 36,              // Base36 encoding (0-9, a-z)
        RANDOM_STRING_START: 2,        // Start position for random string
        RANDOM_STRING_LENGTH: 9        // Length of random string part
    };
    /**
     * @param {Object} [options] - Configuration options
     * @param {boolean} [options.autoStart=false] - Start recording immediately
     * @param {number} [options.maxEvents=10000] - Maximum events to store
     * @param {boolean} [options.debugMode=false] - Enable debug logging
     */
    constructor(options = {}) {
        /** @type {boolean} */
        this.isRecording = false;

        /** @type {number|null} */
        this.recordingStartTime = null;

        /** @type {UIEvent[]} */
        this.events = [];

        /** @type {string|null} */
        this.recordingId = null;

        /** @type {Object} */
        this.options = {
            autoStart: false,
            maxEvents: 10000,
            debugMode: false,
            ...options
        };

        /** @type {Object} */
        this.recordingMetadata = {
            version: "1.0",
            layoutMode: "unknown"
        };

        /** @type {Map<string, Object>} */
        this.elementSnapshots = new Map();

        /** @type {Set<Function>} */
        this.eventListeners = new Set();

        if (this.options.autoStart) {
            this.startRecording();
        }

        this.log('UIRecorder initialized');
    }

    /**
     * Starts recording UI events
     * @param {Object} [metadata] - Optional recording metadata
     * @returns {string} Recording ID
     */
    startRecording(metadata = {}) {
        if (this.isRecording) {
            this.log('Recording already in progress');
            return this.recordingId;
        }

        this.recordingId = this.generateRecordingId();
        this.recordingStartTime = Date.now();
        this.isRecording = true;
        this.events = [];
        this.elementSnapshots.clear();

        // Capture current layout mode if available
        if (window.uiManager && window.uiManager.getLayoutInfo) {
            const layoutInfo = window.uiManager.getLayoutInfo();
            this.recordingMetadata.layoutMode = layoutInfo?.mode || 'unknown';
        }

        // Merge additional metadata
        Object.assign(this.recordingMetadata, metadata);

        this.log(`Recording started: ${this.recordingId}`);
        this.notifyListeners('recordingStarted', { recordingId: this.recordingId });

        return this.recordingId;
    }

    /**
     * Stops recording UI events
     * @returns {UIRecording|null} Complete recording data
     */
    stopRecording() {
        if (!this.isRecording) {
            this.log('No recording in progress');
            return null;
        }

        const endTime = Date.now();
        const totalDuration = endTime - this.recordingStartTime;

        const recording = {
            recording: {
                version: this.recordingMetadata.version,
                startTime: new Date(this.recordingStartTime).toISOString(),
                endTime: new Date(endTime).toISOString(),
                metadata: {
                    recordingId: this.recordingId,
                    layoutMode: this.recordingMetadata.layoutMode,
                    totalDuration,
                    eventCount: this.events.length
                }
            },
            events: [...this.events]
        };

        this.isRecording = false;
        this.recordingStartTime = null;

        this.log(`Recording stopped: ${this.recordingId}, ${this.events.length} events, ${totalDuration}ms`);
        this.notifyListeners('recordingStopped', { recording });

        return recording;
    }

    /**
     * Records addition of a UI element
     * @param {string} elementId - Element ID
     * @param {Object} elementConfig - Element configuration
     */
    recordElementAdd(elementId, elementConfig) {
        if (!this.isRecording) return;

        const timestamp = this.getRelativeTimestamp();
        const event = {
            timestamp,
            type: /** @type {UIEventType} */ ('add'),
            elementId,
            data: {
                elementType: elementConfig.type,
                elementConfig: { ...elementConfig }
            }
        };

        this.addEvent(event);
        this.elementSnapshots.set(elementId, { ...elementConfig });

        this.log(`Recorded ADD: ${elementId} (${elementConfig.type})`);
    }

    /**
     * Records update of a UI element
     * @param {string} elementId - Element ID
     * @param {*} newValue - New element value
     * @param {*} [previousValue] - Previous element value
     */
    recordElementUpdate(elementId, newValue, previousValue) {
        if (!this.isRecording) return;

        const timestamp = this.getRelativeTimestamp();
        const event = {
            timestamp,
            type: /** @type {UIEventType} */ ('update'),
            elementId,
            data: {
                value: newValue,
                previousValue
            }
        };

        this.addEvent(event);

        this.log(`Recorded UPDATE: ${elementId} = ${newValue} (was ${previousValue})`);
    }

    /**
     * Records removal of a UI element
     * @param {string} elementId - Element ID
     */
    recordElementRemove(elementId) {
        if (!this.isRecording) return;

        const timestamp = this.getRelativeTimestamp();
        const snapshot = this.elementSnapshots.get(elementId);

        const event = {
            timestamp,
            type: /** @type {UIEventType} */ ('remove'),
            elementId,
            data: {
                elementSnapshot: snapshot ? { ...snapshot } : null
            }
        };

        this.addEvent(event);
        this.elementSnapshots.delete(elementId);

        this.log(`Recorded REMOVE: ${elementId}`);
    }

    /**
     * Adds an event to the recording
     * @param {UIEvent} event - Event to add
     * @private
     */
    addEvent(event) {
        this.events.push(event);

        // Enforce maximum events limit
        if (this.events.length > this.options.maxEvents) {
            const removed = this.events.splice(0, this.events.length - this.options.maxEvents);
            this.log(`Removed ${removed.length} old events (max limit: ${this.options.maxEvents})`);
        }

        this.notifyListeners('eventRecorded', { event });
    }

    /**
     * Gets the current recording status
     * @returns {Object} Recording status
     */
    getStatus() {
        return {
            isRecording: this.isRecording,
            recordingId: this.recordingId,
            eventCount: this.events.length,
            recordingDuration: this.isRecording ? this.getRelativeTimestamp() : 0,
            elementCount: this.elementSnapshots.size
        };
    }

    /**
     * Exports the current recording as JSON
     * @returns {string|null} JSON string or null if no recording
     */
    exportRecording() {
        if (!this.isRecording && this.events.length === 0) {
            return null;
        }

        let recording;
        if (this.isRecording) {
            // Export current recording state
            recording = {
                recording: {
                    version: this.recordingMetadata.version,
                    startTime: new Date(this.recordingStartTime).toISOString(),
                    metadata: {
                        recordingId: this.recordingId,
                        layoutMode: this.recordingMetadata.layoutMode,
                        eventCount: this.events.length,
                        status: 'in_progress'
                    }
                },
                events: [...this.events]
            };
        } else {
            // Use last stopped recording
            recording = this.stopRecording();
        }

        return JSON.stringify(recording, null, 2);
    }

    /**
     * Imports a recording from JSON
     * @param {string} jsonString - JSON recording data
     * @returns {boolean} Success status
     */
    importRecording(jsonString) {
        try {
            const recording = JSON.parse(jsonString);

            if (!this.validateRecording(recording)) {
                throw new Error('Invalid recording format');
            }

            // Stop current recording if active
            if (this.isRecording) {
                this.stopRecording();
            }

            // Load the imported recording
            this.events = recording.events || [];
            this.recordingMetadata = { ...recording.recording.metadata };

            // Rebuild element snapshots from add events
            this.rebuildElementSnapshots();

            this.log(`Imported recording: ${this.events.length} events`);
            this.notifyListeners('recordingImported', { recording });

            return true;
        } catch (error) {
            console.error('Failed to import recording:', error);
            return false;
        }
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
     * Rebuilds element snapshots from add events
     * @private
     */
    rebuildElementSnapshots() {
        this.elementSnapshots.clear();

        for (const event of this.events) {
            if (event.type === 'add' && event.data.elementConfig) {
                this.elementSnapshots.set(event.elementId, event.data.elementConfig);
            } else if (event.type === 'remove') {
                this.elementSnapshots.delete(event.elementId);
            }
        }
    }

    /**
     * Clears all recorded events
     */
    clearRecording() {
        this.events = [];
        this.elementSnapshots.clear();

        if (this.isRecording) {
            this.recordingStartTime = Date.now();
        }

        this.log('Recording cleared');
        this.notifyListeners('recordingCleared');
    }

    /**
     * Gets relative timestamp from recording start
     * @returns {number} Milliseconds since recording started
     * @private
     */
    getRelativeTimestamp() {
        return this.recordingStartTime ? Date.now() - this.recordingStartTime : 0;
    }

    /**
     * Generates a unique recording ID
     * @returns {string} Recording ID
     * @private
     */
    generateRecordingId() {
        return `ui_recording_${Date.now()}_${Math.random().toString(UIRecorder.ID_GENERATION.BASE36_RADIX).substr(UIRecorder.ID_GENERATION.RANDOM_STRING_START, UIRecorder.ID_GENERATION.RANDOM_STRING_LENGTH)}`;
    }

    /**
     * Adds event listener for recording events
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
     * Notifies all listeners of recording events
     * @param {string} eventType - Type of event
     * @param {Object} data - Event data
     * @private
     */
    notifyListeners(eventType, data) {
        for (const listener of this.eventListeners) {
            try {
                listener({ type: eventType, ...data });
            } catch (error) {
                console.error('UIRecorder listener error:', error);
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
            console.log(`[UIRecorder] ${message}`);
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
     * Destroys the recorder and cleans up resources
     */
    destroy() {
        if (this.isRecording) {
            this.stopRecording();
        }

        this.events = [];
        this.elementSnapshots.clear();
        this.eventListeners.clear();

        this.log('UIRecorder destroyed');
    }
}

// Global instance for easy access
if (typeof window !== 'undefined') {
    window.UIRecorder = UIRecorder;

    // Global control functions
    window.startUIRecording = function(metadata) {
        if (!window.uiRecorder) {
            window.uiRecorder = new UIRecorder({ debugMode: true });
        }
        return window.uiRecorder.startRecording(metadata);
    };

    window.stopUIRecording = function() {
        if (window.uiRecorder) {
            return window.uiRecorder.stopRecording();
        }
        return null;
    };

    window.getUIRecordingStatus = function() {
        if (window.uiRecorder) {
            return window.uiRecorder.getStatus();
        }
        return { isRecording: false, eventCount: 0 };
    };

    window.exportUIRecording = function() {
        if (window.uiRecorder) {
            return window.uiRecorder.exportRecording();
        }
        return null;
    };

    window.clearUIRecording = function() {
        if (window.uiRecorder) {
            window.uiRecorder.clearRecording();
        }
    };
}
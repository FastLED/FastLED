/**
 * FastLED Layout State Manager
 *
 * Unified state management for layout calculations.
 * Centralized layout state storage with atomic updates and change events.
 *
 * @module LayoutStateManager
 */

/* eslint-disable no-console */

/**
 * @typedef {'mobile'|'tablet'|'desktop'|'ultrawide'} LayoutMode
 */

/**
 * @typedef {Object} LayoutState
 * @property {LayoutMode} mode - Current layout mode
 * @property {number} viewportWidth - Total viewport width
 * @property {number} availableWidth - Width available for content
 * @property {number} canvasSize - Current canvas size
 * @property {number} uiColumns - Number of UI columns
 * @property {number} uiColumnWidth - Width of each UI column
 * @property {number} uiTotalWidth - Total width of all UI columns
 * @property {boolean} canExpand - Whether canvas can expand
 * @property {boolean} container2Visible - Whether second container is visible
 * @property {number} totalGroups - Total number of UI groups
 * @property {number} totalElements - Total number of UI elements
 * @property {Object.<string, Object>} containers - Container-specific state
 */

/**
 * @typedef {Object} StateChangeEvent
 * @property {LayoutState} state - Current state
 * @property {LayoutState} previousState - Previous state
 * @property {Array<string>} changedFields - List of changed field names
 */

/**
 * @typedef {Object} LayoutBreakpoints
 * @property {{max: number}} mobile
 * @property {{min: number, max: number}} tablet
 * @property {{min: number, max: number}} desktop
 * @property {{min: number}} ultrawide
 */

/**
 * Manages unified layout state with atomic updates
 */
export class LayoutStateManager {
    /**
     * Default layout configuration
     * @type {Object}
     */
    static DEFAULT_CONFIG = {
        minCanvasSize: 320,
        maxCanvasSize: 800,
        minUIColumnWidth: 280,
        maxUIColumnWidth: 400,
        preferredUIColumnWidth: 320,
        horizontalGap: 40,
        verticalGap: 20,
        containerPadding: 40,
        maxUIColumns: 3,
        canvasExpansionRatio: 0.6,
        minContentRatio: 0.4
    };

    /**
     * Layout breakpoints
     * @type {LayoutBreakpoints}
     */
    static BREAKPOINTS = {
        mobile: { max: 768 },
        tablet: { min: 769, max: 1199 },
        desktop: { min: 1200, max: 1599 },
        ultrawide: { min: 1600 }
    };

    /**
     * @param {Object} [config] - Optional configuration overrides
     */
    constructor(config = {}) {
        /** @type {Object} */
        this.config = { ...LayoutStateManager.DEFAULT_CONFIG, ...config };

        /** @type {LayoutBreakpoints} */
        this.breakpoints = LayoutStateManager.BREAKPOINTS;

        /** @type {LayoutState} */
        this.state = this.createInitialState();

        /** @type {Set<Function>} */
        this.listeners = new Set();

        /** @type {boolean} */
        this.debugMode = false;
    }

    /**
     * Creates initial state
     * @returns {LayoutState}
     * @private
     */
    createInitialState() {
        return {
            mode: 'desktop',
            viewportWidth: 0,
            availableWidth: 0,
            canvasSize: 0,
            uiColumns: 1,
            uiColumnWidth: 0,
            uiTotalWidth: 0,
            canExpand: false,
            container2Visible: false,
            totalGroups: 0,
            totalElements: 0,
            containers: {
                'ui-controls': {
                    visible: true,
                    columns: 1,
                    width: 0
                },
                'ui-controls-2': {
                    visible: false,
                    columns: 0,
                    width: 0
                }
            }
        };
    }

    /**
     * Detects layout mode based on viewport width
     * @param {number} width - Viewport width
     * @returns {LayoutMode}
     */
    detectLayoutMode(width) {
        if (width <= this.breakpoints.mobile.max) {
            return 'mobile';
        } else if (width >= this.breakpoints.tablet.min && width <= this.breakpoints.tablet.max) {
            return 'tablet';
        } else if (width >= this.breakpoints.desktop.min && width <= this.breakpoints.desktop.max) {
            return 'desktop';
        } else {
            return 'ultrawide';
        }
    }

    /**
     * Calculates layout based on viewport dimensions
     * @param {number} viewportWidth - Current viewport width
     * @returns {LayoutState} Calculated layout state
     */
    calculateLayout(viewportWidth) {
        const mode = this.detectLayoutMode(viewportWidth);
        const availableWidth = viewportWidth - (2 * this.config.containerPadding);

        let layout = {
            mode,
            viewportWidth,
            availableWidth,
            canvasSize: 0,
            uiColumns: 0,
            uiColumnWidth: 0,
            uiTotalWidth: 0,
            canExpand: false,
            container2Visible: false,
            totalGroups: this.state.totalGroups,
            totalElements: this.state.totalElements,
            containers: {}
        };

        switch (mode) {
            case 'mobile':
                layout = this.calculateMobileLayout(layout, availableWidth);
                break;
            case 'tablet':
                layout = this.calculateTabletLayout(layout, availableWidth);
                break;
            case 'desktop':
                layout = this.calculateDesktopLayout(layout, availableWidth);
                break;
            case 'ultrawide':
                layout = this.calculateUltrawideLayout(layout, availableWidth);
                break;
        }

        return layout;
    }

    /**
     * Calculates mobile layout (single column)
     * @param {LayoutState} layout - Layout state to update
     * @param {number} availableWidth - Available width
     * @returns {LayoutState}
     * @private
     */
    calculateMobileLayout(layout, availableWidth) {
        layout.canvasSize = Math.min(availableWidth, this.config.maxCanvasSize);
        layout.uiColumns = 1;
        layout.uiColumnWidth = availableWidth;
        layout.uiTotalWidth = availableWidth;
        layout.canExpand = false;
        layout.container2Visible = false;

        layout.containers = {
            'ui-controls': {
                visible: true,
                columns: 1,
                width: availableWidth
            },
            'ui-controls-2': {
                visible: false,
                columns: 0,
                width: 0
            }
        };

        return layout;
    }

    /**
     * Calculates tablet layout (2 columns)
     * @param {LayoutState} layout - Layout state to update
     * @param {number} availableWidth - Available width
     * @returns {LayoutState}
     * @private
     */
    calculateTabletLayout(layout, availableWidth) {
        const baseCanvasSize = Math.min(this.config.maxCanvasSize, availableWidth * 0.5);
        const remainingSpace = availableWidth - baseCanvasSize - this.config.horizontalGap;

        layout.canvasSize = baseCanvasSize;
        layout.uiColumns = 1;
        layout.uiColumnWidth = Math.max(this.config.minUIColumnWidth, remainingSpace);
        layout.uiTotalWidth = layout.uiColumnWidth;
        layout.canExpand = false;
        layout.container2Visible = false;

        layout.containers = {
            'ui-controls': {
                visible: true,
                columns: 1,
                width: layout.uiColumnWidth
            },
            'ui-controls-2': {
                visible: false,
                columns: 0,
                width: 0
            }
        };

        return layout;
    }

    /**
     * Calculates desktop layout (2 columns with expansion)
     * @param {LayoutState} layout - Layout state to update
     * @param {number} availableWidth - Available width
     * @returns {LayoutState}
     * @private
     */
    calculateDesktopLayout(layout, availableWidth) {
        const baseCanvasSize = Math.min(this.config.maxCanvasSize, availableWidth * this.config.canvasExpansionRatio);
        const remainingSpace = availableWidth - baseCanvasSize - this.config.horizontalGap;

        // Calculate UI columns that can fit
        const possibleColumns = Math.floor(remainingSpace / this.config.preferredUIColumnWidth);
        const actualColumns = Math.min(Math.max(1, possibleColumns), 2);

        const uiColumnWidth = remainingSpace / actualColumns;
        const canExpand = uiColumnWidth > this.config.maxUIColumnWidth;

        if (canExpand) {
            const extraSpace = (uiColumnWidth - this.config.maxUIColumnWidth) * actualColumns;
            layout.canvasSize = Math.min(
                this.config.maxCanvasSize,
                baseCanvasSize + (extraSpace * this.config.canvasExpansionRatio)
            );
            layout.uiColumnWidth = this.config.maxUIColumnWidth;
        } else {
            layout.canvasSize = baseCanvasSize;
            layout.uiColumnWidth = uiColumnWidth;
        }

        layout.uiColumns = actualColumns;
        layout.uiTotalWidth = layout.uiColumnWidth * actualColumns;
        layout.canExpand = canExpand;
        layout.container2Visible = false;

        layout.containers = {
            'ui-controls': {
                visible: true,
                columns: actualColumns,
                width: layout.uiTotalWidth
            },
            'ui-controls-2': {
                visible: false,
                columns: 0,
                width: 0
            }
        };

        return layout;
    }

    /**
     * Calculates ultrawide layout (3 columns)
     * @param {LayoutState} layout - Layout state to update
     * @param {number} availableWidth - Available width
     * @returns {LayoutState}
     * @private
     */
    calculateUltrawideLayout(layout, availableWidth) {
        const baseCanvasSize = this.config.maxCanvasSize;
        const remainingSpace = availableWidth - baseCanvasSize - (2 * this.config.horizontalGap);

        // Calculate columns for both sides
        const sideSpace = remainingSpace / 2;
        const columnsPerSide = Math.floor(sideSpace / this.config.preferredUIColumnWidth);
        const actualColumnsPerSide = Math.min(Math.max(1, columnsPerSide), Math.floor(this.config.maxUIColumns / 2));

        const uiColumnWidth = Math.min(
            this.config.maxUIColumnWidth,
            sideSpace / actualColumnsPerSide
        );

        layout.canvasSize = baseCanvasSize;
        layout.uiColumns = actualColumnsPerSide * 2;
        layout.uiColumnWidth = uiColumnWidth;
        layout.uiTotalWidth = uiColumnWidth * layout.uiColumns;
        layout.canExpand = false;
        layout.container2Visible = true;

        layout.containers = {
            'ui-controls': {
                visible: true,
                columns: actualColumnsPerSide,
                width: uiColumnWidth * actualColumnsPerSide
            },
            'ui-controls-2': {
                visible: true,
                columns: actualColumnsPerSide,
                width: uiColumnWidth * actualColumnsPerSide
            }
        };

        return layout;
    }

    /**
     * Updates state atomically
     * @param {Partial<LayoutState>} updates - State updates to apply
     * @returns {boolean} Whether state changed
     */
    updateState(updates) {
        const previousState = { ...this.state };
        const changedFields = [];

        for (const [key, value] of Object.entries(updates)) {
            if (JSON.stringify(this.state[key]) !== JSON.stringify(value)) {
                changedFields.push(key);
                this.state[key] = value;
            }
        }

        if (changedFields.length > 0) {
            this.notifyListeners({
                state: { ...this.state },
                previousState,
                changedFields
            });

            if (this.debugMode) {
                console.log('LayoutStateManager: State updated', {
                    changedFields,
                    state: this.state
                });
            }

            return true;
        }

        return false;
    }

    /**
     * Updates viewport and recalculates layout
     * @param {number} viewportWidth - New viewport width
     * @returns {boolean} Whether layout changed
     */
    updateViewport(viewportWidth) {
        const newLayout = this.calculateLayout(viewportWidth);
        return this.updateState(newLayout);
    }

    /**
     * Updates content metrics
     * @param {number} totalGroups - Total UI groups
     * @param {number} totalElements - Total UI elements
     * @returns {boolean} Whether metrics changed
     */
    updateContentMetrics(totalGroups, totalElements) {
        return this.updateState({
            totalGroups,
            totalElements
        });
    }

    /**
     * Gets current state
     * @returns {LayoutState}
     */
    getState() {
        return { ...this.state };
    }

    /**
     * Gets specific container state
     * @param {string} containerId - Container ID
     * @returns {Object|null}
     */
    getContainerState(containerId) {
        return this.state.containers[containerId] || null;
    }

    /**
     * Adds state change listener
     * @param {Function} listener - Listener function
     */
    addStateChangeListener(listener) {
        this.listeners.add(listener);
    }

    /**
     * Removes state change listener
     * @param {Function} listener - Listener function
     */
    removeStateChangeListener(listener) {
        this.listeners.delete(listener);
    }

    /**
     * Notifies all listeners of state change
     * @param {StateChangeEvent} event - Change event
     * @private
     */
    notifyListeners(event) {
        for (const listener of this.listeners) {
            try {
                listener(event);
            } catch (error) {
                console.error('LayoutStateManager: Listener error', error);
            }
        }
    }

    /**
     * Enables debug mode
     */
    enableDebugMode() {
        this.debugMode = true;
        console.log('LayoutStateManager: Debug mode enabled');
    }

    /**
     * Disables debug mode
     */
    disableDebugMode() {
        this.debugMode = false;
    }

    /**
     * Resets state to initial values
     */
    reset() {
        const previousState = { ...this.state };
        this.state = this.createInitialState();

        this.notifyListeners({
            state: { ...this.state },
            previousState,
            changedFields: Object.keys(this.state)
        });
    }
}
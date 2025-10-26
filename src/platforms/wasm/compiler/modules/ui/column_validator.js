/**
 * FastLED Column Validator
 *
 * Layout optimization and validation for column-based layouts.
 * Analyzes layout efficiency and provides optimization recommendations.
 *
 * @module ColumnValidator
 */

/* eslint-disable no-console */

/**
 * @typedef {Object} ValidationResult
 * @property {boolean} isValid - Whether layout is valid
 * @property {number} efficiency - Efficiency score (0-100)
 * @property {Array<string>} warnings - Layout warnings
 * @property {Array<string>} errors - Layout errors
 * @property {Object} metrics - Layout metrics
 * @property {Array<string>} recommendations - Optimization recommendations
 */

/**
 * @typedef {Object} LayoutMetrics
 * @property {number} spaceUtilization - Percentage of space used
 * @property {number} columnBalance - Balance score between columns
 * @property {number} contentDensity - Content per column density
 * @property {number} aspectRatio - Layout aspect ratio
 * @property {number} whitespaceRatio - Ratio of whitespace to content
 */

/**
 * @typedef {Object} OptimizationSuggestion
 * @property {string} type - Suggestion type
 * @property {string} description - Human-readable description
 * @property {Object} params - Suggested parameters
 * @property {number} impact - Expected improvement (0-100)
 */

/**
 * Validates and optimizes column layouts
 */
export class ColumnValidator {
    /**
     * Default validation thresholds
     * @type {Object}
     */
    static DEFAULT_THRESHOLDS = {
        minSpaceUtilization: 0.6,      // Minimum 60% space usage
        maxSpaceUtilization: 0.95,     // Maximum 95% space usage
        minColumnWidth: 280,           // Minimum column width in pixels
        maxColumnWidth: 600,           // Maximum column width in pixels
        minContentPerColumn: 3,        // Minimum items per column
        maxContentPerColumn: 20,       // Maximum items per column
        maxAspectRatio: 3,             // Maximum width:height ratio
        minAspectRatio: 0.33,          // Minimum width:height ratio
        maxWhitespaceRatio: 0.4,       // Maximum 40% whitespace
        columnBalanceTolerance: 0.2    // 20% tolerance for column balance
    };

    /** Layout dimension constants */
    static LAYOUT_DIMENSIONS = {
        DEFAULT_VIEWPORT_HEIGHT: 800,  // Default viewport height fallback
        DEFAULT_UI_HEIGHT: 600,        // Default UI container height
        MIN_VIEWPORT_WIDTH: 320,       // Minimum supported viewport width
        MIN_CANVAS_SIZE: 200,          // Minimum canvas size warning threshold
        MAX_CANVAS_SIZE: 1200,         // Maximum canvas size warning threshold
        ELEMENT_SIZE_ESTIMATE: 100,    // Estimated pixel size per UI element
        MIN_COLUMN_WIDTH_THRESHOLD: 300, // Threshold for column width recommendations
        LARGE_COLUMN_WIDTH_THRESHOLD: 500, // Threshold for too-wide columns
        HIGH_CONTENT_DENSITY: 15,      // High content density threshold
        LOW_CONTENT_DENSITY: 5,        // Low content density threshold
        WIDE_LAYOUT_THRESHOLD: 1400    // Threshold for wide layout optimizations
    };

    /** Optimization scoring constants */
    static SCORING_CONSTANTS = {
        ERROR_PENALTY: 20,             // Points deducted per error
        WARNING_PENALTY: 5,            // Points deducted per warning
        OPTIMAL_SPACE_MIN: 0.7,        // Optimal space utilization minimum
        OPTIMAL_SPACE_MAX: 0.9,        // Optimal space utilization maximum
        OPTIMAL_ASPECT_MIN: 1.3,       // Optimal aspect ratio minimum
        OPTIMAL_ASPECT_MAX: 1.7,       // Optimal aspect ratio maximum
        SPACE_WEIGHT: 0.4,             // Weight for space utilization in score
        BALANCE_WEIGHT: 0.3,           // Weight for column balance in score
        ASPECT_WEIGHT: 0.3,            // Weight for aspect ratio in score
        LOW_SPACE_THRESHOLD: 0.6,      // Low space utilization threshold
        HIGH_WHITESPACE_THRESHOLD: 0.5, // High whitespace ratio threshold
        LOW_BALANCE_THRESHOLD: 0.7,    // Low column balance threshold
        CANVAS_SCALE_FACTOR: 1.2,      // Canvas size optimization scaling
        CANVAS_WIDTH_RATIO: 0.6        // Canvas to available width ratio
    };

    /**
     * @param {Object} [thresholds] - Optional threshold overrides
     */
    constructor(thresholds = {}) {
        /** @type {Object} */
        this.thresholds = { ...ColumnValidator.DEFAULT_THRESHOLDS, ...thresholds };

        /** @type {boolean} */
        this.debugMode = false;

        /** @type {Map<string, ValidationResult>} */
        this.validationCache = new Map();

        /** @type {number} */
        this.cacheMaxSize = 100;
    }

    /**
     * Validates a layout configuration
     * @param {Object} layout - Layout configuration to validate
     * @returns {ValidationResult}
     */
    validate(layout) {
        const cacheKey = this.generateCacheKey(layout);

        // Check cache
        if (this.validationCache.has(cacheKey)) {
            if (this.debugMode) {
                console.log('ColumnValidator: Using cached validation');
            }
            return this.validationCache.get(cacheKey);
        }

        const result = {
            isValid: true,
            efficiency: 100,
            warnings: [],
            errors: [],
            metrics: {},
            recommendations: []
        };

        // Calculate metrics
        result.metrics = this.calculateMetrics(layout);

        // Perform validations
        this.validateDimensions(layout, result);
        this.validateColumns(layout, result);
        this.validateContent(layout, result);
        this.validateSpaceUtilization(layout, result);
        this.validateAspectRatio(layout, result);

        // Calculate efficiency score
        result.efficiency = this.calculateEfficiency(result);

        // Generate recommendations
        result.recommendations = this.generateRecommendations(layout, result);

        // Cache result
        this.cacheValidation(cacheKey, result);

        if (this.debugMode) {
            console.log('ColumnValidator: Validation result', result);
        }

        return result;
    }

    /**
     * Calculates layout metrics
     * @param {Object} layout - Layout configuration
     * @returns {LayoutMetrics}
     * @private
     */
    calculateMetrics(layout) {
        const totalSpace = layout.viewportWidth * (layout.viewportHeight || ColumnValidator.LAYOUT_DIMENSIONS.DEFAULT_VIEWPORT_HEIGHT);
        const usedSpace = (layout.canvasSize * (layout.canvasHeight || layout.canvasSize)) +
                         (layout.uiTotalWidth * (layout.uiHeight || ColumnValidator.LAYOUT_DIMENSIONS.DEFAULT_UI_HEIGHT));

        const spaceUtilization = Math.min(1, usedSpace / totalSpace);

        // Calculate column balance
        let columnBalance = 1;
        if (layout.containers) {
            const columnCounts = Object.values(layout.containers)
                .filter(c => c.visible)
                .map(c => c.itemCount || 0);

            if (columnCounts.length > 1) {
                const avg = columnCounts.reduce((a, b) => a + b, 0) / columnCounts.length;
                const variance = columnCounts.reduce((sum, count) => {
                    return sum + Math.pow(count - avg, 2);
                }, 0) / columnCounts.length;
                columnBalance = Math.max(0, 1 - (Math.sqrt(variance) / avg));
            }
        }

        // Calculate content density
        const totalColumns = layout.uiColumns || 1;
        const totalElements = layout.totalElements || 0;
        const contentDensity = totalElements / totalColumns;

        // Calculate aspect ratio
        const layoutWidth = layout.canvasSize + layout.uiTotalWidth;
        const layoutHeight = Math.max(
            layout.canvasHeight || layout.canvasSize,
            layout.uiHeight || ColumnValidator.LAYOUT_DIMENSIONS.DEFAULT_UI_HEIGHT
        );
        const aspectRatio = layoutWidth / layoutHeight;

        // Calculate whitespace ratio
        const contentArea = totalElements * ColumnValidator.LAYOUT_DIMENSIONS.ELEMENT_SIZE_ESTIMATE;
        const totalArea = layout.uiTotalWidth * (layout.uiHeight || ColumnValidator.LAYOUT_DIMENSIONS.DEFAULT_UI_HEIGHT);
        const whitespaceRatio = Math.max(0, 1 - (contentArea / totalArea));

        return {
            spaceUtilization,
            columnBalance,
            contentDensity,
            aspectRatio,
            whitespaceRatio
        };
    }

    /**
     * Validates layout dimensions
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Result object to update
     * @private
     */
    validateDimensions(layout, result) {
        // Check viewport width
        if (layout.viewportWidth < ColumnValidator.LAYOUT_DIMENSIONS.MIN_VIEWPORT_WIDTH) {
            result.errors.push(`Viewport width is too small (< ${ColumnValidator.LAYOUT_DIMENSIONS.MIN_VIEWPORT_WIDTH}px)`);
            result.isValid = false;
        }

        // Check canvas size
        if (layout.canvasSize < ColumnValidator.LAYOUT_DIMENSIONS.MIN_CANVAS_SIZE) {
            result.warnings.push(`Canvas size is very small (< ${ColumnValidator.LAYOUT_DIMENSIONS.MIN_CANVAS_SIZE}px)`);
        }

        if (layout.canvasSize > ColumnValidator.LAYOUT_DIMENSIONS.MAX_CANVAS_SIZE) {
            result.warnings.push(`Canvas size is very large (> ${ColumnValidator.LAYOUT_DIMENSIONS.MAX_CANVAS_SIZE}px)`);
        }
    }

    /**
     * Validates column configuration
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Result object to update
     * @private
     */
    validateColumns(layout, result) {
        const columnWidth = layout.uiColumnWidth;

        if (columnWidth && columnWidth < this.thresholds.minColumnWidth) {
            result.warnings.push(
                `Column width (${columnWidth}px) is below minimum (${this.thresholds.minColumnWidth}px)`
            );
        }

        if (columnWidth && columnWidth > this.thresholds.maxColumnWidth) {
            result.warnings.push(
                `Column width (${columnWidth}px) exceeds maximum (${this.thresholds.maxColumnWidth}px)`
            );
        }

        // Check column count
        if (layout.uiColumns > 4) {
            result.warnings.push('Too many UI columns (> 4) may affect usability');
        }
    }

    /**
     * Validates content distribution
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Result object to update
     * @private
     */
    validateContent(layout, result) {
        const contentPerColumn = result.metrics.contentDensity;

        if (contentPerColumn < this.thresholds.minContentPerColumn) {
            result.warnings.push(
                `Low content density (${contentPerColumn.toFixed(1)} items/column)`
            );
        }

        if (contentPerColumn > this.thresholds.maxContentPerColumn) {
            result.warnings.push(
                `High content density (${contentPerColumn.toFixed(1)} items/column)`
            );
        }

        // Check column balance
        if (result.metrics.columnBalance < (1 - this.thresholds.columnBalanceTolerance)) {
            result.warnings.push('Unbalanced content distribution between columns');
        }
    }

    /**
     * Validates space utilization
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Result object to update
     * @private
     */
    validateSpaceUtilization(layout, result) {
        const utilization = result.metrics.spaceUtilization;

        if (utilization < this.thresholds.minSpaceUtilization) {
            result.warnings.push(
                `Low space utilization (${(utilization * 100).toFixed(1)}%)`
            );
        }

        if (utilization > this.thresholds.maxSpaceUtilization) {
            result.warnings.push(
                `Very high space utilization (${(utilization * 100).toFixed(1)}%)`
            );
        }

        // Check whitespace
        if (result.metrics.whitespaceRatio > this.thresholds.maxWhitespaceRatio) {
            result.warnings.push(
                `High whitespace ratio (${(result.metrics.whitespaceRatio * 100).toFixed(1)}%)`
            );
        }
    }

    /**
     * Validates aspect ratio
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Result object to update
     * @private
     */
    validateAspectRatio(layout, result) {
        const ratio = result.metrics.aspectRatio;

        if (ratio > this.thresholds.maxAspectRatio) {
            result.warnings.push(
                `Layout is too wide (aspect ratio: ${ratio.toFixed(2)})`
            );
        }

        if (ratio < this.thresholds.minAspectRatio) {
            result.warnings.push(
                `Layout is too tall (aspect ratio: ${ratio.toFixed(2)})`
            );
        }
    }

    /**
     * Calculates overall efficiency score
     * @param {ValidationResult} result - Validation result
     * @returns {number} Efficiency score (0-100)
     * @private
     */
    calculateEfficiency(result) {
        let score = 100;

        // Deduct for errors
        score -= result.errors.length * ColumnValidator.SCORING_CONSTANTS.ERROR_PENALTY;

        // Deduct for warnings
        score -= result.warnings.length * ColumnValidator.SCORING_CONSTANTS.WARNING_PENALTY;

        // Factor in metrics
        const metrics = result.metrics;

        // Space utilization factor (optimal: 0.7-0.9)
        const spaceScore = metrics.spaceUtilization < ColumnValidator.SCORING_CONSTANTS.OPTIMAL_SPACE_MIN
            ? metrics.spaceUtilization / ColumnValidator.SCORING_CONSTANTS.OPTIMAL_SPACE_MIN
            : metrics.spaceUtilization > ColumnValidator.SCORING_CONSTANTS.OPTIMAL_SPACE_MAX
                ? (1 - metrics.spaceUtilization) / (1 - ColumnValidator.SCORING_CONSTANTS.OPTIMAL_SPACE_MAX)
                : 1;

        // Column balance factor
        const balanceScore = metrics.columnBalance;

        // Aspect ratio factor (optimal: 1.3-1.7)
        const aspectScore = metrics.aspectRatio < ColumnValidator.SCORING_CONSTANTS.OPTIMAL_ASPECT_MIN
            ? metrics.aspectRatio / ColumnValidator.SCORING_CONSTANTS.OPTIMAL_ASPECT_MIN
            : metrics.aspectRatio > ColumnValidator.SCORING_CONSTANTS.OPTIMAL_ASPECT_MAX
                ? Math.max(0, 2 - (metrics.aspectRatio / ColumnValidator.SCORING_CONSTANTS.OPTIMAL_ASPECT_MAX))
                : 1;

        // Combine factors
        const metricsScore = (spaceScore * ColumnValidator.SCORING_CONSTANTS.SPACE_WEIGHT +
                             balanceScore * ColumnValidator.SCORING_CONSTANTS.BALANCE_WEIGHT +
                             aspectScore * ColumnValidator.SCORING_CONSTANTS.ASPECT_WEIGHT) * 100;
        score = Math.min(score, metricsScore);

        return Math.max(0, Math.round(score));
    }

    /**
     * Generates optimization recommendations
     * @param {Object} layout - Layout configuration
     * @param {ValidationResult} result - Validation result
     * @returns {Array<string>} Recommendations
     * @private
     */
    generateRecommendations(layout, result) {
        const recommendations = [];
        const metrics = result.metrics;

        // Space utilization recommendations
        if (metrics.spaceUtilization < ColumnValidator.SCORING_CONSTANTS.LOW_SPACE_THRESHOLD) {
            recommendations.push('Consider reducing container padding or increasing content size');
        }

        if (metrics.spaceUtilization > ColumnValidator.SCORING_CONSTANTS.OPTIMAL_SPACE_MAX) {
            recommendations.push('Add more spacing between elements for better readability');
        }

        // Column recommendations
        if (layout.uiColumnWidth < ColumnValidator.LAYOUT_DIMENSIONS.MIN_COLUMN_WIDTH_THRESHOLD && layout.uiColumns > 1) {
            recommendations.push('Consider reducing the number of columns for wider column width');
        }

        if (layout.uiColumnWidth > ColumnValidator.LAYOUT_DIMENSIONS.LARGE_COLUMN_WIDTH_THRESHOLD) {
            recommendations.push('Consider adding more columns to better utilize horizontal space');
        }

        // Content density recommendations
        if (metrics.contentDensity > ColumnValidator.LAYOUT_DIMENSIONS.HIGH_CONTENT_DENSITY) {
            recommendations.push('Consider pagination or virtual scrolling for better performance');
        }

        if (metrics.contentDensity < ColumnValidator.LAYOUT_DIMENSIONS.LOW_CONTENT_DENSITY && layout.uiColumns > 2) {
            recommendations.push('Consider reducing columns to consolidate content');
        }

        // Balance recommendations
        if (metrics.columnBalance < ColumnValidator.SCORING_CONSTANTS.LOW_BALANCE_THRESHOLD) {
            recommendations.push('Redistribute content more evenly between columns');
        }

        // Whitespace recommendations
        if (metrics.whitespaceRatio > ColumnValidator.SCORING_CONSTANTS.HIGH_WHITESPACE_THRESHOLD) {
            recommendations.push('Reduce vertical spacing or add more content');
        }

        return recommendations;
    }

    /**
     * Optimizes a layout configuration
     * @param {Object} layout - Current layout
     * @returns {Object} Optimized layout parameters
     */
    optimize(layout) {
        const validation = this.validate(layout);
        const optimized = { ...layout };

        // Optimize column width
        if (layout.uiColumnWidth < this.thresholds.minColumnWidth) {
            const newColumns = Math.max(1, layout.uiColumns - 1);
            optimized.uiColumns = newColumns;
            optimized.uiColumnWidth = layout.uiTotalWidth / newColumns;
        }

        // Optimize canvas size
        if (validation.metrics.spaceUtilization < ColumnValidator.SCORING_CONSTANTS.LOW_SPACE_THRESHOLD) {
            optimized.canvasSize = Math.min(
                layout.canvasSize * ColumnValidator.SCORING_CONSTANTS.CANVAS_SCALE_FACTOR,
                layout.availableWidth * ColumnValidator.SCORING_CONSTANTS.CANVAS_WIDTH_RATIO
            );
        }

        // Optimize container visibility
        if (layout.uiColumns === 1 && layout.availableWidth > ColumnValidator.LAYOUT_DIMENSIONS.WIDE_LAYOUT_THRESHOLD) {
            optimized.container2Visible = true;
            optimized.uiColumns = 2;
        }

        if (this.debugMode) {
            console.log('ColumnValidator: Optimized layout', {
                original: layout,
                optimized,
                changes: this.diffLayouts(layout, optimized)
            });
        }

        return optimized;
    }

    /**
     * Generates cache key for layout
     * @param {Object} layout - Layout configuration
     * @returns {string} Cache key
     * @private
     */
    generateCacheKey(layout) {
        const key = [
            layout.mode,
            layout.viewportWidth,
            layout.canvasSize,
            layout.uiColumns,
            layout.uiColumnWidth,
            layout.totalElements
        ].join('-');
        return key;
    }

    /**
     * Caches validation result
     * @param {string} key - Cache key
     * @param {ValidationResult} result - Validation result
     * @private
     */
    cacheValidation(key, result) {
        // Limit cache size
        if (this.validationCache.size >= this.cacheMaxSize) {
            const firstKey = this.validationCache.keys().next().value;
            this.validationCache.delete(firstKey);
        }

        this.validationCache.set(key, result);
    }

    /**
     * Compares two layouts and returns differences
     * @param {Object} layout1 - First layout
     * @param {Object} layout2 - Second layout
     * @returns {Object} Differences
     * @private
     */
    diffLayouts(layout1, layout2) {
        const diff = {};

        for (const key in layout2) {
            if (layout1[key] !== layout2[key]) {
                diff[key] = {
                    from: layout1[key],
                    to: layout2[key]
                };
            }
        }

        return diff;
    }

    /**
     * Clears validation cache
     */
    clearCache() {
        this.validationCache.clear();
    }

    /**
     * Enables debug mode
     */
    enableDebugMode() {
        this.debugMode = true;
        console.log('ColumnValidator: Debug mode enabled');
    }

    /**
     * Disables debug mode
     */
    disableDebugMode() {
        this.debugMode = false;
    }

    /**
     * Updates validation thresholds
     * @param {Object} thresholds - New thresholds
     */
    updateThresholds(thresholds) {
        this.thresholds = { ...this.thresholds, ...thresholds };
        this.clearCache(); // Clear cache as thresholds changed
    }
}
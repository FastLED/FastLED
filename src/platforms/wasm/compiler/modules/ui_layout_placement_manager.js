/**
 * FastLED UI Layout Placement Manager
 *
 * Advanced responsive layout management system for FastLED WebAssembly applications.
 * Intelligently manages UI layout with dynamic column allocation and responsive behavior.
 *
 * Key features:
 * - Calculates optimal number of UI columns based on available space
 * - Allows main content area to expand when space permits
 * - Maintains responsive behavior across different screen sizes
 * - Ensures main content always has priority for space allocation
 * - Handles breakpoint transitions smoothly
 * - Provides layout optimization callbacks for UI elements
 * - Manages container styling and positioning
 *
 * Supported layouts:
 * - Mobile: Single column, stacked layout
 * - Tablet: Side-by-side with single UI column
 * - Desktop: Multi-column UI with expandable canvas
 * - Ultrawide: Maximum columns and aggressive expansion
 *
 * @module UILayoutPlacementManager
 */

/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */

/**
 * Advanced UI Layout Placement Manager
 *
 * Intelligently manages UI layout with dynamic column allocation:
 * - Calculates optimal number of UI columns based on available space
 * - Allows main content area to expand when space permits
 * - Maintains responsive behavior across different screen sizes
 * - Ensures main content always has priority for space allocation
 */
export class UILayoutPlacementManager {
  /**
   * Creates a new UILayoutPlacementManager instance
   * Sets up responsive breakpoints and initializes layout calculations
   */
  constructor() {
    /**
     * Media queries for different layout breakpoints
     * @type {Object<string, MediaQueryList>}
     */
    this.breakpoints = {
      /** @type {MediaQueryList} Mobile devices (max 767px) */
      mobile: globalThis.matchMedia('(max-width: 767px)'),
      /** @type {MediaQueryList} Tablet devices (768px-1199px) */
      tablet: globalThis.matchMedia('(min-width: 768px) and (max-width: 1199px)'),
      /** @type {MediaQueryList} Desktop devices (1200px+) */
      desktop: globalThis.matchMedia('(min-width: 1200px)'),
      /** @type {MediaQueryList} Ultrawide displays (1600px+) */
      ultrawide: globalThis.matchMedia('(min-width: 1600px)'),
    };

    /**
     * Layout configuration parameters
     * @type {Object}
     */
    this.config = {
      // Minimum dimensions
      /** @type {number} Minimum canvas size in pixels */
      minCanvasSize: 320,
      /** @type {number} Maximum canvas size in pixels */
      maxCanvasSize: 800,
      /** @type {number} Minimum UI column width in pixels */
      minUIColumnWidth: 280,
      /** @type {number} Maximum UI column width in pixels */
      maxUIColumnWidth: 400,

      // Spacing
      /** @type {number} Horizontal gap between elements */
      horizontalGap: 40,
      /** @type {number} Vertical gap between elements */
      verticalGap: 20,
      /** @type {number} Container padding */
      containerPadding: 40,

      // UI column settings
      /** @type {number} Maximum number of UI columns */
      maxUIColumns: 3,
      /** @type {number} Preferred UI column width */
      preferredUIColumnWidth: 320,

      // Canvas expansion rules
      /** @type {number} Canvas should take up to 60% of available width */
      canvasExpansionRatio: 0.6,
      /** @type {number} Content area should have at least 40% of width */
      minContentRatio: 0.4,
    };

    /** @type {string} Current layout mode */
    this.currentLayout = this.detectLayout();

    /** @type {Object} Current layout calculation data */
    this.layoutData = this.calculateLayoutData();

    // Bind event handlers
    this.handleLayoutChange = this.handleLayoutChange.bind(this);
    this.handleResize = this.handleResize.bind(this);

    // Listen for breakpoint changes
    Object.values(this.breakpoints).forEach((mq) => {
      mq.addEventListener('change', this.handleLayoutChange);
    });

    // Listen for window resize for fine-grained adjustments
    globalThis.addEventListener('resize', this.handleResize);

    // Apply initial layout
    this.applyLayout();
  }

  /**
   * Detect current layout mode based on screen size
   * @returns {string} Layout mode ('mobile', 'tablet', 'desktop', or 'ultrawide')
   */
  detectLayout() {
    if (this.breakpoints.mobile.matches) return 'mobile';
    if (this.breakpoints.tablet.matches) return 'tablet';
    if (this.breakpoints.ultrawide.matches) return 'ultrawide';
    return 'desktop';
  }

  /**
   * Calculate optimal layout dimensions and column allocation
   * @returns {Object} Layout data with dimensions and configuration
   */
  calculateLayoutData() {
    const viewportWidth = globalThis.innerWidth;
    const availableWidth = viewportWidth - (this.config.containerPadding * 2);

    let layoutData = {
      /** @type {number} Current viewport width */
      viewportWidth,
      /** @type {number} Available width after padding */
      availableWidth,
      /** @type {number} Calculated canvas size */
      canvasSize: this.config.minCanvasSize,
      /** @type {number} Number of UI columns */
      uiColumns: 1,
      /** @type {number} Width of each UI column */
      uiColumnWidth: this.config.preferredUIColumnWidth,
      /** @type {number} Total width of all UI columns */
      uiTotalWidth: this.config.preferredUIColumnWidth,
      /** @type {number} Total content area width */
      contentWidth: this.config.minCanvasSize,
      /** @type {string} Current layout mode */
      layoutMode: this.currentLayout,
      /** @type {boolean} Whether canvas can expand beyond minimum */
      canExpand: false,
    };

    // Calculate layout based on current mode
    switch (this.currentLayout) {
      case 'mobile':
        layoutData = this.calculateMobileLayout(layoutData);
        break;
      case 'tablet':
        layoutData = this.calculateTabletLayout(layoutData);
        break;
      case 'desktop':
        layoutData = this.calculateDesktopLayout(layoutData);
        break;
      case 'ultrawide':
        layoutData = this.calculateUltrawideLayout(layoutData);
        break;
    }

    return layoutData;
  }

  /**
   * Calculate mobile layout (stacked, single column)
   * @param {Object} layoutData - Base layout data to modify
   * @returns {Object} Updated layout data for mobile
   */
  calculateMobileLayout(layoutData) {
    layoutData.uiColumns = 1;
    layoutData.uiColumnWidth = Math.min(layoutData.availableWidth, this.config.maxUIColumnWidth);
    layoutData.uiTotalWidth = layoutData.uiColumnWidth;
    layoutData.canvasSize = Math.min(layoutData.availableWidth, this.config.minCanvasSize);
    layoutData.contentWidth = layoutData.canvasSize;
    layoutData.canExpand = false;
    return layoutData;
  }

  /**
   * Calculate tablet layout (side-by-side, single UI column)
   * @param {Object} layoutData - Base layout data to modify
   * @returns {Object} Updated layout data for tablet
   */
  calculateTabletLayout(layoutData) {
    const requiredWidth = this.config.minCanvasSize + this.config.minUIColumnWidth +
      this.config.horizontalGap;

    if (layoutData.availableWidth >= requiredWidth) {
      // Side-by-side layout
      const remainingWidth = layoutData.availableWidth - this.config.horizontalGap;
      const optimalCanvasSize = Math.min(
        remainingWidth * this.config.canvasExpansionRatio,
        this.config.maxCanvasSize,
      );

      layoutData.canvasSize = Math.max(optimalCanvasSize, this.config.minCanvasSize);
      layoutData.uiTotalWidth = remainingWidth - layoutData.canvasSize;
      layoutData.uiColumnWidth = Math.min(layoutData.uiTotalWidth, this.config.maxUIColumnWidth);
      layoutData.uiColumns = 1;
      layoutData.contentWidth = layoutData.canvasSize;
      layoutData.canExpand = layoutData.canvasSize > this.config.minCanvasSize;
    } else {
      // Fall back to stacked layout
      return this.calculateMobileLayout(layoutData);
    }

    return layoutData;
  }

  /**
   * Calculate desktop layout (multi-column UI possible)
   * @param {Object} layoutData - Base layout data to modify
   * @returns {Object} Updated layout data for desktop
   */
  calculateDesktopLayout(layoutData) {
    const baseRequiredWidth = this.config.minCanvasSize + this.config.minUIColumnWidth +
      this.config.horizontalGap;

    if (layoutData.availableWidth >= baseRequiredWidth) {
      // Calculate optimal canvas size
      const remainingWidth = layoutData.availableWidth - this.config.horizontalGap;
      const maxCanvasSize = Math.min(
        remainingWidth * this.config.canvasExpansionRatio,
        this.config.maxCanvasSize,
      );

      layoutData.canvasSize = Math.max(maxCanvasSize, this.config.minCanvasSize);
      const availableUIWidth = remainingWidth - layoutData.canvasSize;

      // Calculate optimal number of UI columns
      const optimalColumns = Math.min(
        Math.floor(availableUIWidth / this.config.minUIColumnWidth),
        this.config.maxUIColumns,
      );

      layoutData.uiColumns = Math.max(1, optimalColumns);
      layoutData.uiColumnWidth = Math.min(
        availableUIWidth / layoutData.uiColumns,
        this.config.maxUIColumnWidth,
      );
      layoutData.uiTotalWidth = layoutData.uiColumnWidth * layoutData.uiColumns;
      layoutData.contentWidth = layoutData.canvasSize;
      layoutData.canExpand = layoutData.canvasSize > this.config.minCanvasSize;
    } else {
      // Fall back to tablet layout
      return this.calculateTabletLayout(layoutData);
    }

    return layoutData;
  }

  /**
   * Calculate ultrawide layout (maximum columns and expansion)
   * @param {Object} layoutData - Base layout data to modify
   * @returns {Object} Updated layout data for ultrawide displays
   */
  calculateUltrawideLayout(layoutData) {
    const result = this.calculateDesktopLayout(layoutData);

    // On ultrawide screens, allow more aggressive expansion
    const remainingWidth = layoutData.availableWidth - this.config.horizontalGap;
    const maxExpandedCanvas = Math.min(
      remainingWidth * 0.7, // Allow canvas to take up to 70% on ultrawide
      this.config.maxCanvasSize,
    );

    if (maxExpandedCanvas > result.canvasSize) {
      const extraSpace = maxExpandedCanvas - result.canvasSize;
      const availableUIWidth = result.uiTotalWidth + extraSpace;

      // Recalculate UI columns with extra space
      const optimalColumns = Math.min(
        Math.floor(availableUIWidth / this.config.minUIColumnWidth),
        this.config.maxUIColumns,
      );

      result.canvasSize = maxExpandedCanvas;
      result.uiColumns = Math.max(result.uiColumns, optimalColumns);
      result.uiColumnWidth = Math.min(
        availableUIWidth / result.uiColumns,
        this.config.maxUIColumnWidth,
      );
      result.uiTotalWidth = result.uiColumnWidth * result.uiColumns;
      result.contentWidth = result.canvasSize;
      result.canExpand = true;
    }

    return result;
  }

  /**
   * Handle layout changes from media query breakpoints
   */
  handleLayoutChange() {
    const newLayout = this.detectLayout();
    if (newLayout !== this.currentLayout) {
      console.log(`Layout transition: ${this.currentLayout} → ${newLayout}`);
      this.currentLayout = newLayout;
      this.layoutData = this.calculateLayoutData();
      this.applyLayout();
    }
  }

  /**
   * Handle window resize for fine-grained adjustments
   */
  handleResize() {
    // Debounce resize events
    clearTimeout(this.resizeTimeout);
    this.resizeTimeout = setTimeout(() => {
      const newLayoutData = this.calculateLayoutData();

      // Only update if significant changes
      if (this.hasSignificantLayoutChange(newLayoutData)) {
        this.layoutData = newLayoutData;
        this.applyLayout();
      }
    }, 100);
  }

  /**
   * Check if layout change is significant enough to warrant update
   */
  hasSignificantLayoutChange(newLayoutData) {
    const threshold = 50; // pixels
    return (
      Math.abs(newLayoutData.canvasSize - this.layoutData.canvasSize) > threshold ||
      newLayoutData.uiColumns !== this.layoutData.uiColumns ||
      Math.abs(newLayoutData.uiColumnWidth - this.layoutData.uiColumnWidth) > threshold
    );
  }

  /**
   * Apply the calculated layout to DOM elements
   */
  applyLayout() {
    const mainContainer = document.getElementById('main-container');
    const topRow = document.getElementById('top-row');
    const canvasContainer = document.getElementById('canvas-container');
    const uiControls = document.getElementById('ui-controls');
    const canvas = document.getElementById('myCanvas');

    if (!mainContainer || !topRow || !canvasContainer || !uiControls) {
      console.warn('Layout containers not found, cannot apply layout');
      return;
    }

    // Remove existing layout classes
    [mainContainer, topRow, canvasContainer, uiControls].forEach((el) => {
      el.classList.remove('mobile-layout', 'tablet-layout', 'desktop-layout', 'ultrawide-layout');
    });

    // Apply new layout classes
    const layoutClass = `${this.currentLayout}-layout`;
    [mainContainer, topRow, canvasContainer, uiControls].forEach((el) => {
      el.classList.add(layoutClass);
    });

    // Apply layout-specific styles
    this.applyContainerStyles(mainContainer, topRow, canvasContainer, uiControls);

    // Apply canvas sizing
    if (canvas) {
      this.applyCanvasStyles(canvas);
    }

    // Apply UI column layout
    this.applyUIColumnLayout(uiControls);

    // Trigger custom event for other components
    globalThis.dispatchEvent(
      new CustomEvent('layoutChanged', {
        detail: {
          layout: this.currentLayout,
          data: this.layoutData,
        },
      }),
    );

    console.log(`Applied ${this.currentLayout} layout:`, this.layoutData);
  }

  /**
   * Apply styles to main container elements
   */
  applyContainerStyles(mainContainer, topRow, canvasContainer, uiControls) {
    const isStacked = this.currentLayout === 'mobile';

    // Main container
    mainContainer.style.maxWidth = `${
      Math.min(this.layoutData.availableWidth + this.config.containerPadding * 2, 2000)
    }px`;

    // Top row
    topRow.style.flexDirection = isStacked ? 'column' : 'row';
    topRow.style.alignItems = isStacked ? 'center' : 'flex-start';
    topRow.style.gap = isStacked
      ? `${this.config.verticalGap}px`
      : `${this.config.horizontalGap}px`;
    topRow.style.justifyContent = 'center';

    // Canvas container
    canvasContainer.style.order = '1';
    canvasContainer.style.flex = '0 0 auto';

    // UI controls
    uiControls.style.order = '2';
    uiControls.style.flex = '0 0 auto';
    uiControls.style.marginTop = isStacked ? `${this.config.verticalGap}px` : '0';
    uiControls.style.width = `${this.layoutData.uiTotalWidth}px`;
    uiControls.style.alignSelf = isStacked ? 'center' : 'flex-start';
    uiControls.style.height = 'fit-content';
  }

  /**
   * Apply canvas sizing and responsive behavior
   */
  applyCanvasStyles(canvas) {
    const { canvasSize } = this.layoutData;

    // Set canvas display size
    canvas.style.width = `${canvasSize}px`;
    canvas.style.height = `${canvasSize}px`;
    canvas.style.maxWidth = `${canvasSize}px`;
    canvas.style.maxHeight = `${canvasSize}px`;

    // Maintain pixel-perfect rendering
    canvas.style.imageRendering = 'pixelated';

    console.log(`Canvas sized to ${canvasSize}px (expanded: ${this.layoutData.canExpand})`);
  }

  /**
   * Apply multi-column layout to UI controls
   */
  applyUIColumnLayout(uiControls) {
    const { uiColumns, uiColumnWidth } = this.layoutData;

    if (uiColumns > 1) {
      // Multi-column layout
      uiControls.style.display = 'grid';
      uiControls.style.gridTemplateColumns = `repeat(${uiColumns}, ${uiColumnWidth}px)`;
      uiControls.style.gridGap = `${this.config.verticalGap}px ${this.config.horizontalGap / 2}px`;
      uiControls.style.alignItems = 'start';
      uiControls.style.justifyContent = 'center';

      // Add CSS custom properties for child elements
      uiControls.style.setProperty('--ui-columns', uiColumns);
      uiControls.style.setProperty('--ui-column-width', `${uiColumnWidth}px`);

      console.log(`UI arranged in ${uiColumns} columns of ${uiColumnWidth}px each`);
    } else {
      // Single column layout
      uiControls.style.display = 'flex';
      uiControls.style.flexDirection = 'column';
      uiControls.style.gridTemplateColumns = 'none';
      uiControls.style.gridGap = 'unset';
      uiControls.style.alignItems = 'center';
      uiControls.style.gap = `${this.config.verticalGap}px`;

      uiControls.style.removeProperty('--ui-columns');
      uiControls.style.removeProperty('--ui-column-width');
    }
  }

  /**
   * Get current layout information
   */
  getLayoutInfo() {
    return {
      mode: this.currentLayout,
      data: this.layoutData,
      isStacked: this.currentLayout === 'mobile',
      canExpand: this.layoutData.canExpand,
      uiColumns: this.layoutData.uiColumns,
    };
  }

  /**
   * Force a layout recalculation and update
   */
  forceLayoutUpdate() {
    this.layoutData = this.calculateLayoutData();
    this.applyLayout();
  }

  /**
   * Clean up event listeners
   */
  destroy() {
    Object.values(this.breakpoints).forEach((mq) => {
      mq.removeEventListener('change', this.handleLayoutChange);
    });
    globalThis.removeEventListener('resize', this.handleResize);
    clearTimeout(this.resizeTimeout);
  }
}

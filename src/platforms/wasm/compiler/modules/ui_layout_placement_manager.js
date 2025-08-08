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
 * @fileoverview UI Layout Placement Manager for FastLED
 * Handles responsive layout calculations and CSS application
 */

/**
 * @typedef {Object} LayoutConfig
 * @property {number} containerPadding
 * @property {number} minCanvasSize
 * @property {number} maxCanvasSize
 * @property {number} preferredUIColumnWidth
 * @property {number} maxUIColumnWidth
 * @property {number} minUIColumnWidth
 * @property {number} maxUIColumns
 * @property {number} canvasExpansionRatio
 * @property {number} horizontalGap
 * @property {number} verticalGap
 */

/**
 * @typedef {Object} LayoutResult
 * @property {number} viewportWidth
 * @property {number} availableWidth
 * @property {number} canvasSize
 * @property {number} uiColumns
 * @property {number} uiColumnWidth
 * @property {number} uiTotalWidth
 * @property {number} contentWidth
 * @property {string} layoutMode
 * @property {boolean} canExpand
 */

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
     * @type {LayoutConfig}
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

    /** @type {LayoutResult|null} */
    this.layoutData = this.calculateLayoutData();

    // Bind event handlers
    this.handleLayoutChange = this.handleLayoutChange.bind(this);
    this.handleResize = this.handleResize.bind(this);
    this.forceLayoutUpdate = this.forceLayoutUpdate.bind(this);

    // Listen for breakpoint changes
    Object.values(this.breakpoints).forEach((mq) => {
      mq.addEventListener('change', this.handleLayoutChange);
    });

    // Listen for window resize for fine-grained adjustments
    globalThis.addEventListener('resize', this.handleResize);

    // Add visibility change listener to re-apply layout when page becomes visible
    globalThis.addEventListener('visibilitychange', () => {
      if (!document.hidden) {
        this.forceLayoutUpdate();
      }
    });

    // Apply initial layout
    this.applyLayout();

    // Force layout update after a brief delay to handle any timing issues
    setTimeout(() => {
      this.forceLayoutUpdate();
    }, 100);
  }

  /**
   * Detect current layout mode based on screen size
   * @returns {string} Layout mode ('mobile', 'tablet', 'desktop', or 'ultrawide')
   */
  detectLayout() {
    const viewportWidth = globalThis.innerWidth;
    console.log(
      `🔍 Layout detection: viewport=${viewportWidth}px, mobile=${this.breakpoints.mobile.matches}, tablet=${this.breakpoints.tablet.matches}, desktop=${this.breakpoints.desktop.matches}, ultrawide=${this.breakpoints.ultrawide.matches}`,
    );

    if (this.breakpoints.mobile.matches) return 'mobile';
    if (this.breakpoints.tablet.matches) return 'tablet';
    if (this.breakpoints.ultrawide.matches) return 'ultrawide';
    return 'desktop';
  }

  /**
   * Calculate optimal layout dimensions and column allocation
   * @returns {LayoutResult} Layout data with dimensions and configuration
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
   * @param {LayoutResult} layoutData - Base layout data to modify
   * @returns {LayoutResult} Updated layout data for mobile
   */
  calculateMobileLayout(layoutData) {
    layoutData.uiColumns = 1;
    layoutData.uiColumnWidth = Math.min(layoutData.availableWidth, this.config.maxUIColumnWidth);
    layoutData.uiTotalWidth = layoutData.uiColumnWidth;

    // In mobile mode, allow canvas to scale properly within available space
    // Use a reasonable percentage of available width, but respect min/max constraints
    const maxCanvasSize = Math.min(
      layoutData.availableWidth * 0.8, // Use up to 80% of available width
      this.config.maxCanvasSize,
    );

    layoutData.canvasSize = Math.max(maxCanvasSize, this.config.minCanvasSize);
    layoutData.contentWidth = layoutData.canvasSize;
    layoutData.canExpand = layoutData.canvasSize > this.config.minCanvasSize;

    return layoutData;
  }

  /**
   * Calculate tablet layout (side-by-side, single UI column)
   * @param {LayoutResult} layoutData - Base layout data to modify
   * @returns {LayoutResult} Updated layout data for tablet
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
   * @param {LayoutResult} layoutData - Base layout data to modify
   * @returns {LayoutResult} Updated layout data for desktop
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
   * Calculate ultrawide layout (3-column grid with flexible UI columns)
   * @param {LayoutResult} layoutData - Base layout data to modify
   * @returns {LayoutResult} Updated layout data for ultrawide
   */
  calculateUltrawideLayout(layoutData) {
    // For ultra-wide, we use a flexible grid approach
    // Canvas gets a reasonable size, UI columns use flexible sizing

    const remainingWidth = layoutData.availableWidth - (this.config.horizontalGap * 2); // Account for gaps between 3 columns

    // Calculate optimal canvas size (not too large, leave room for UI)
    const maxCanvasSize = Math.min(
      remainingWidth * 0.5, // Canvas takes up to 50% of available space
      this.config.maxCanvasSize,
    );

    const optimalCanvasSize = Math.max(maxCanvasSize, this.config.minCanvasSize);

    // UI columns will use flexible sizing (minmax(280px, 1fr))
    // So we don't need to calculate exact pixel widths
    const uiColumnMinWidth = this.config.minUIColumnWidth; // 280px minimum

    layoutData.canvasSize = optimalCanvasSize;
    layoutData.uiColumns = 2; // Two UI columns in ultra-wide
    layoutData.uiColumnWidth = uiColumnMinWidth; // Minimum width, will expand with 1fr
    layoutData.uiTotalWidth = uiColumnMinWidth * 2; // Total minimum width
    layoutData.contentWidth = optimalCanvasSize;
    layoutData.canExpand = true;

    console.log(
      `Ultra-wide layout: canvas=${optimalCanvasSize}px, UI columns=2 (flexible), min width=${uiColumnMinWidth}px each`,
    );

    return layoutData;
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
   * Handle window resize for fine-grained adjustments and layout mode changes
   */
  handleResize() {
    // Debounce resize events
    clearTimeout(this.resizeTimeout);
    this.resizeTimeout = setTimeout(() => {
      // First, check if layout mode has changed (most important)
      const newLayout = this.detectLayout();
      const layoutModeChanged = newLayout !== this.currentLayout;

      if (layoutModeChanged) {
        console.log(`Layout mode change detected on resize: ${this.currentLayout} → ${newLayout}`);
        this.currentLayout = newLayout;
        this.layoutData = this.calculateLayoutData();
        this.applyLayout();
        return; // Layout mode change trumps fine-grained adjustments
      }

      // If layout mode hasn't changed, check for fine-grained adjustments
      const newLayoutData = this.calculateLayoutData();

      // Always update if there are any changes (remove the "significant" threshold for mode-specific updates)
      if (this.hasAnyLayoutChange(newLayoutData)) {
        this.layoutData = newLayoutData;
        this.applyLayout();
      }
    }, 100);
  }

  /**
   * Check if there are any layout changes worth updating
   */
  hasAnyLayoutChange(newLayoutData) {
    const threshold = 10; // Much smaller threshold for responsive updates
    return (
      Math.abs(newLayoutData.canvasSize - this.layoutData.canvasSize) > threshold ||
      newLayoutData.uiColumns !== this.layoutData.uiColumns ||
      Math.abs(newLayoutData.uiColumnWidth - this.layoutData.uiColumnWidth) > threshold ||
      Math.abs(newLayoutData.uiTotalWidth - this.layoutData.uiTotalWidth) > threshold ||
      newLayoutData.viewportWidth !== this.layoutData.viewportWidth
    );
  }

  /**
   * Check if layout change is significant enough to warrant update (legacy method, kept for compatibility)
   */
  hasSignificantLayoutChange(newLayoutData) {
    return this.hasAnyLayoutChange(newLayoutData);
  }

  /**
   * Apply the calculated layout to DOM elements
   */
  applyLayout() {
    const mainContainer = document.getElementById('main-container');
    const contentGrid = document.getElementById('content-grid');
    const canvasContainer = document.getElementById('canvas-container');
    const uiControls = document.getElementById('ui-controls');
    const uiControls2 = document.getElementById('ui-controls-2');
    const canvas = document.getElementById('myCanvas');

    if (!mainContainer || !contentGrid || !canvasContainer || !uiControls) {
      console.warn('Layout containers not found, cannot apply layout');
      return;
    }

    // Remove existing layout classes
    [mainContainer, contentGrid, canvasContainer, uiControls].forEach((el) => {
      el.classList.remove('mobile-layout', 'tablet-layout', 'desktop-layout', 'ultrawide-layout');
    });

    // Apply new layout classes
    const layoutClass = `${this.currentLayout}-layout`;
    [mainContainer, contentGrid, canvasContainer, uiControls].forEach((el) => {
      el.classList.add(layoutClass);
    });

    if (uiControls2) {
      uiControls2.classList.remove(
        'mobile-layout',
        'tablet-layout',
        'desktop-layout',
        'ultrawide-layout',
      );
      uiControls2.classList.add(layoutClass);
    }

    // Apply layout-specific styles
    this.applyContainerStyles(mainContainer, contentGrid, canvasContainer, uiControls, uiControls2);

    // Apply canvas sizing
    if (canvas) {
      this.applyCanvasStyles(canvas);
    }

    // Trigger custom event for other components
    globalThis.dispatchEvent(
      new CustomEvent('layoutChanged', {
        detail: {
          layout: this.currentLayout,
          data: this.layoutData,
        },
      }),
    );

    console.log(`Applied ${this.currentLayout} layout: ${this.getGridDescription()}`);
  }

  /**
   * Get description of current grid layout
   */
  getGridDescription() {
    switch (this.currentLayout) {
      case 'mobile':
        return '1×N grid (portrait)';
      case 'tablet':
      case 'desktop':
        return '2×N grid (landscape)';
      case 'ultrawide':
        return '3×N grid (ultra-wide)';
      default:
        return 'unknown grid';
    }
  }

  /**
   * Apply styles to main container elements
   */
  applyContainerStyles(mainContainer, contentGrid, canvasContainer, uiControls, uiControls2) {
    const isUltrawide = this.currentLayout === 'ultrawide';
    const isLandscape = this.currentLayout === 'tablet' || this.currentLayout === 'desktop';
    const isPortrait = this.currentLayout === 'mobile';

    // Determine if there are any UI elements to render. If none, switch to a
    // single-pane canvas layout and avoid grid-based presentation entirely.
    const hasUiElements = (() => {
      const ui1HasChildren = uiControls && uiControls.classList.contains('active') && uiControls.children.length > 0;
      const ui2HasChildren = uiControls2 && uiControls2.classList.contains('active') && uiControls2.children.length > 0;
      return !!(ui1HasChildren || ui2HasChildren);
    })();

    // Main container
    mainContainer.style.maxWidth = `${
      Math.min(this.layoutData.availableWidth + this.config.containerPadding * 2, 2000)
    }px`;

    // If there are no UI elements, present a simple centered canvas view and
    // suppress grid styling to avoid reserving space for non-existent UI.
    if (!hasUiElements) {
      // Container: simple vertical flow centered
      contentGrid.style.display = 'flex';
      contentGrid.style.width = '100%';
      contentGrid.style.flexDirection = 'column';
      contentGrid.style.rowGap = `${this.config.verticalGap}px`;
      contentGrid.style.justifyContent = 'flex-start';
      contentGrid.style.alignItems = 'center';

      // Clear any grid-specific properties that may have been set previously
      contentGrid.style.gridTemplateColumns = '';
      contentGrid.style.gridTemplateRows = '';
      contentGrid.style.gridTemplateAreas = '';
      contentGrid.style.gap = '';

      // Ensure UI containers are hidden
      if (uiControls) {
        uiControls.style.display = 'none';
      }
      if (uiControls2) {
        uiControls2.style.display = 'none';
      }

      // Canvas container should be centered in the flex layout
      canvasContainer.style.gridArea = '';
      canvasContainer.style.alignSelf = 'center';

      // Nothing else to do for no-UI mode
      return;
    }

    // Content grid - implements 1×N, 2×N, 3×N layout when UI exists
    contentGrid.style.display = 'grid';
    contentGrid.style.width = '100%';
    contentGrid.style.gap = `${this.config.verticalGap}px ${this.config.horizontalGap}px`;
    contentGrid.style.justifyContent = 'center';
    contentGrid.style.alignItems = 'start';

    if (isPortrait) {
      // 1×N grid (portrait)
      contentGrid.style.gridTemplateColumns = '1fr';
      contentGrid.style.gridTemplateRows = 'auto auto';
      contentGrid.style.gridTemplateAreas = '"canvas" "ui"';

      // Hide second UI container
      if (uiControls2) {
        uiControls2.style.display = 'none';
      }
    } else if (isLandscape) {
      // 2×N grid (landscape)
      contentGrid.style.gridTemplateColumns = `${this.layoutData.canvasSize}px minmax(280px, 1fr)`;
      contentGrid.style.gridTemplateRows = 'auto';
      contentGrid.style.gridTemplateAreas = '"canvas ui"';

      // Hide second UI container
      if (uiControls2) {
        uiControls2.style.display = 'none';
      }
    } else if (isUltrawide) {
      // 3×N grid (ultra-wide) - Place canvas in the middle between two UI columns
      const minUIWidth = 280; // Minimum width for UI columns
      const canvasWidth = this.layoutData.canvasSize;

      console.log(`🔍 Ultra-wide layout (center canvas): canvas=${canvasWidth}px, minUIWidth=${minUIWidth}px`);

      // Flexible UI columns on the sides, fixed canvas in the middle
      contentGrid.style.gridTemplateColumns =
        `minmax(${minUIWidth}px, 1fr) ${canvasWidth}px minmax(${minUIWidth}px, 1fr)`;
      contentGrid.style.gridTemplateRows = 'auto';
      contentGrid.style.gridTemplateAreas = '"ui canvas ui2"';

      // Show and configure second UI container
      if (uiControls2) {
        console.log('🔍 Showing ui-controls-2 container');
        uiControls2.style.display = 'flex';
        uiControls2.style.flexDirection = 'column';
        uiControls2.style.gap = '20px'; // Use 20px gaps between elements
        uiControls2.style.alignItems = 'stretch'; // Allow elements to expand
        uiControls2.style.gridArea = 'ui2';
        uiControls2.style.width = '100%';
        uiControls2.style.minWidth = `${minUIWidth}px`;

        // Force the container to be visible
        uiControls2.style.visibility = 'visible';
        uiControls2.style.opacity = '1';
      } else {
        console.warn('🔍 ui-controls-2 container not found!');
      }

      // Configure first UI container for ultra-wide
      uiControls.style.minWidth = `${minUIWidth}px`;
      uiControls.style.gridArea = 'ui';

      console.log('🔍 Ultra-wide grid applied:', contentGrid.style.gridTemplateColumns);
    }

    // Canvas container
    canvasContainer.style.gridArea = 'canvas';
    canvasContainer.style.justifySelf = 'center';

    // UI controls - Configure for optimal space usage
    uiControls.style.gridArea = 'ui';
    uiControls.style.display = 'flex';
    uiControls.style.flexDirection = 'column';
    uiControls.style.gap = '20px'; // Use 20px gaps between elements
    uiControls.style.alignItems = 'stretch'; // Allow elements to expand to fill width
    uiControls.style.width = '100%';

    console.log(`Applied grid layout: ${this.getGridDescription()}`);
  }

  /**
   * Apply canvas sizing and responsive behavior
   */
  applyCanvasStyles(canvas) {
    const { canvasSize } = this.layoutData;

    // Get the canvas's internal dimensions to determine aspect ratio
    const canvasWidth = canvas.width || 1;
    const canvasHeight = canvas.height || 1;
    const aspectRatio = canvasWidth / canvasHeight;

    // Calculate display dimensions that maintain aspect ratio
    let displayWidth;
    let displayHeight;

    if (aspectRatio >= 1) {
      // Landscape or square: constrain by width
      displayWidth = canvasSize;
      displayHeight = Math.round(canvasSize / aspectRatio);
    } else {
      // Portrait: constrain by height
      displayHeight = canvasSize;
      displayWidth = Math.round(canvasSize * aspectRatio);
    }

    // Set canvas display size while maintaining aspect ratio
    canvas.style.width = `${displayWidth}px`;
    canvas.style.height = `${displayHeight}px`;
    canvas.style.maxWidth = `${displayWidth}px`;
    canvas.style.maxHeight = `${displayHeight}px`;

    // Maintain pixel-perfect rendering
    canvas.style.imageRendering = 'pixelated';

    console.log(
      `Canvas sized to ${displayWidth}x${displayHeight}px (aspect ratio: ${
        aspectRatio.toFixed(2)
      }, expanded: ${this.layoutData.canExpand})`,
    );
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
   * Force a layout recalculation and update (bypasses debouncing)
   */
  forceLayoutUpdate() {
    console.log('Force layout update triggered');

    // First check if layout mode has changed
    const newLayout = this.detectLayout();
    if (newLayout !== this.currentLayout) {
      console.log(`Force update detected layout change: ${this.currentLayout} → ${newLayout}`);
      this.currentLayout = newLayout;
    }

    // Recalculate layout data and apply immediately
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
    globalThis.removeEventListener('visibilitychange', this.forceLayoutUpdate);
    clearTimeout(this.resizeTimeout);
  }
}

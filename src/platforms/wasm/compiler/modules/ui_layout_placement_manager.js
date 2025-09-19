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
 * @property {number} minContentRatio
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
    this.handleContainerResize = this.handleContainerResize.bind(this);

    // Listen for breakpoint changes
    Object.values(this.breakpoints).forEach((mq) => {
      mq.addEventListener('change', this.handleLayoutChange);
    });

    // Listen for window resize for fine-grained adjustments
    globalThis.addEventListener('resize', this.handleResize);

    // Set up ResizeObserver for container-specific resize handling
    this.setupResizeObserver();

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
      // Re-observe containers in case they were created after initial setup
      this.observeContainers();
    }, 100);
  }

  /**
   * Set up ResizeObserver to monitor specific containers for size changes
   */
  setupResizeObserver() {
    // Check if ResizeObserver is supported
    if (typeof ResizeObserver === 'undefined') {
      console.warn('ResizeObserver not supported, falling back to window resize events only');
      return;
    }

    this.resizeObserver = new ResizeObserver((entries) => {
      // Debounce ResizeObserver callbacks
      clearTimeout(this.resizeObserverTimeout);
      this.resizeObserverTimeout = setTimeout(() => {
        this.handleContainerResize(entries);
      }, 50);
    });

    // Observe key containers once they're available
    this.observeContainers();
  }

  /**
   * Start observing key layout containers
   */
  observeContainers() {
    if (!this.resizeObserver) return;

    const containersToObserve = [
      'main-container',
      'content-grid',
      'canvas-container',
      'ui-controls',
      'ui-controls-2',
    ];

    // Track which containers we're already observing to avoid duplicates
    if (!this.observedContainers) {
      this.observedContainers = new Set();
    }

    containersToObserve.forEach((id) => {
      const element = document.getElementById(id);
      if (element && !this.observedContainers.has(id)) {
        this.resizeObserver.observe(element);
        this.observedContainers.add(id);
        console.log(`🔍 ResizeObserver: Now observing ${id}`);
      }
    });
  }

  /**
   * Handle container resize events from ResizeObserver
   * @param {ResizeObserverEntry[]} entries - Array of resize entries
   */
  handleContainerResize(entries) {
    let shouldUpdate = false;
    const layoutInfo = this.layoutData;

    for (const entry of entries) {
      const { target, contentRect } = entry;
      const elementId = target.id;

      console.log(`🔍 Container resized: ${elementId} - ${contentRect.width}x${contentRect.height}`);

      // Check if this resize affects our layout calculations
      if (elementId === 'main-container' || elementId === 'content-grid') {
        // Major container resize - always update
        shouldUpdate = true;
        break;
      } else if (elementId === 'ui-controls' || elementId === 'ui-controls-2') {
        // UI container resize - check if it significantly affects layout
        const availableWidth = contentRect.width;
        const widthDiff = Math.abs(availableWidth - (layoutInfo?.availableWidth || 0));

        if (widthDiff > 20) { // 20px threshold for UI container changes
          shouldUpdate = true;
          break;
        }
      }
    }

    if (shouldUpdate) {
      console.log('🔍 ResizeObserver triggered layout update');

      // Check for layout mode changes first
      const newLayout = this.detectLayout();
      if (newLayout !== this.currentLayout) {
        console.log(`🔍 ResizeObserver detected layout mode change: ${this.currentLayout} → ${newLayout}`);
        this.currentLayout = newLayout;
      }

      this.layoutData = this.calculateLayoutData();
      this.applyLayout();
    }
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
    const requiredWidth = this.config.minCanvasSize + this.config.minUIColumnWidth
      + this.config.horizontalGap;

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
    const baseRequiredWidth = this.config.minCanvasSize + this.config.minUIColumnWidth
      + this.config.horizontalGap;

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
      Math.abs(newLayoutData.canvasSize - this.layoutData.canvasSize) > threshold
      || newLayoutData.uiColumns !== this.layoutData.uiColumns
      || Math.abs(newLayoutData.uiColumnWidth - this.layoutData.uiColumnWidth) > threshold
      || Math.abs(newLayoutData.uiTotalWidth - this.layoutData.uiTotalWidth) > threshold
      || newLayoutData.viewportWidth !== this.layoutData.viewportWidth
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

    // Ensure ResizeObserver is watching all containers after layout changes
    if (this.resizeObserver) {
      setTimeout(() => this.observeContainers(), 50);
    }

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
      case 'ultrawide': {
        // Check if we're actually using 3 columns or fell back to 2
        const contentGrid = document.getElementById('content-grid');
        if (contentGrid && contentGrid.style.gridTemplateAreas) {
          const areas = contentGrid.style.gridTemplateAreas;
          if (areas.includes('ui2')) {
            return '3×N grid (ultra-wide)';
          }
          return '2×N grid (ultra-wide fallback)';
        }
        return '3×N grid (ultra-wide)';
      }
      default:
        return 'unknown grid';
    }
  }

  /**
   * Apply layout configuration via CSS custom properties and classes
   * JS handles general layout decisions, CSS handles micro-layouts
   */
  applyContainerStyles(mainContainer, contentGrid, canvasContainer, uiControls, uiControls2) {
    // Determine if there are any UI elements to render
    const hasUiElements = (() => {
      // Check for children first (more reliable than 'active' class timing)
      const ui1HasChildren = uiControls && uiControls.children.length > 0;
      const ui2HasChildren = uiControls2 && uiControls2.children.length > 0;

      // If containers have children, assume UI elements exist even without 'active' class
      // This prevents race conditions where layout is applied before UI elements are fully initialized
      if (ui1HasChildren || ui2HasChildren) {
        return true;
      }

      // Fallback to checking active class for backward compatibility
      const ui1IsActive = uiControls && uiControls.classList.contains('active') && ui1HasChildren;
      const ui2IsActive = uiControls2 && uiControls2.classList.contains('active') && ui2HasChildren;
      return Boolean(ui1IsActive || ui2IsActive);
    })();

    // 🎯 REFACTORED APPROACH: Set CSS custom properties instead of inline styles
    // This allows CSS to handle micro-layouts while JS manages general layout
    const root = document.documentElement;

    // Set layout data as CSS custom properties
    root.style.setProperty('--layout-mode', this.currentLayout);
    root.style.setProperty('--canvas-size', `${this.layoutData.canvasSize}px`);
    root.style.setProperty('--ui-columns', `${this.layoutData.uiColumns}`);
    root.style.setProperty('--ui-total-width', `${this.layoutData.uiTotalWidth || 280}px`);
    root.style.setProperty('--container-max-width', `${Math.min(this.layoutData.availableWidth + this.config.containerPadding * 2, 2000)}px`);
    root.style.setProperty('--has-ui-elements', hasUiElements ? '1' : '0');

    // Apply layout mode class to main container for CSS targeting
    mainContainer.className = mainContainer.className.replace(/layout-\w+/g, '');
    mainContainer.classList.add(`layout-${this.currentLayout}`);

    // 🎯 REFACTORED: Manage UI container visibility via classes instead of inline styles
    if (!hasUiElements) {
      // Apply no-UI mode class for CSS to handle layout
      contentGrid.classList.add('no-ui-mode');
      contentGrid.classList.remove('has-ui-mode');

      // Ensure UI containers are hidden (only when truly no UI elements)
      if (uiControls) {
        uiControls.classList.remove('active');
        uiControls.classList.add('hidden');
      }
      if (uiControls2) {
        uiControls2.classList.remove('active');
        uiControls2.classList.add('hidden');
      }

      return;
    } else {
      // Apply UI mode class for CSS to handle layout
      contentGrid.classList.add('has-ui-mode');
      contentGrid.classList.remove('no-ui-mode');

      // Ensure UI containers are visible
      if (uiControls) {
        uiControls.classList.add('active');
        uiControls.classList.remove('hidden');
      }

      // Handle second UI container based on layout mode
      if (uiControls2) {
        if (this.currentLayout === 'ultrawide' && this.shouldUseThreeColumnLayout(uiControls, uiControls2)) {
          uiControls2.classList.add('active');
          uiControls2.classList.remove('hidden');
        } else {
          uiControls2.classList.remove('active');
          uiControls2.classList.add('hidden');
        }
      }
    }

    // 🎯 REFACTORED: CSS now handles all grid layout via custom properties and classes
    // JS only provides the data, CSS handles the micro-layout implementation

    // Set gap values as CSS custom properties
    root.style.setProperty('--vertical-gap', `${this.config.verticalGap}px`);
    root.style.setProperty('--horizontal-gap', `${this.config.horizontalGap}px`);

    // Layout-specific custom properties are already set above
    // CSS will handle the actual grid template based on layout mode classes
    // Add ultrawide-specific custom properties for three-column layout
    if (this.currentLayout === 'ultrawide' && this.shouldUseThreeColumnLayout(uiControls, uiControls2)) {
      const minUIWidth = Math.max(this.config.minUIColumnWidth, 280);
      const maxCanvasSize = Math.min(this.config.maxCanvasSize, 800);
      const canvasWidth = Math.min(this.layoutData.canvasSize, maxCanvasSize);

      root.style.setProperty('--ultrawide-min-ui-width', `${minUIWidth}px`);
      root.style.setProperty('--ultrawide-canvas-width', `${canvasWidth}px`);
      root.style.setProperty('--use-three-columns', '1');

      console.log('🔍 Ultra-wide three-column layout configured');
    } else {
      root.style.setProperty('--use-three-columns', '0');
      console.log(`🔍 Layout configured: ${this.currentLayout}`);
    }

    // 🎯 REFACTORED: All container styling now handled by CSS via classes and custom properties
    // No more inline styles - CSS takes full control of micro-layouts

    console.log(`Applied layout: ${this.getGridDescription()}`);
  }

  /**
   * Determine if ultrawide layout should use 3 columns based on content amount
   * @param {HTMLElement} uiControls - First UI container
   * @param {HTMLElement} uiControls2 - Second UI container
   * @returns {boolean} Whether to use 3-column layout
   */
  shouldUseThreeColumnLayout(uiControls, uiControls2) {
    if (!uiControls || !uiControls2) return false;

    // Count total groups and elements across both containers
    const container1Groups = uiControls.querySelectorAll('.ui-group').length;
    const container2Groups = uiControls2.querySelectorAll('.ui-group').length;
    const totalGroups = container1Groups + container2Groups;

    const container1Elements = uiControls.querySelectorAll('.ui-control').length;
    const container2Elements = uiControls2.querySelectorAll('.ui-control').length;
    const totalElements = container1Elements + container2Elements;

    // Only check if second container has content
    const hasContentInSecondContainer = container2Groups > 0 || container2Elements > 0;

    // Thresholds for 3-column layout (matching UI manager thresholds)
    const minGroupsFor3Col = 6;
    const minElementsFor3Col = 12;
    const minElementsPerGroup = 2;

    const hasEnoughGroups = totalGroups >= minGroupsFor3Col;
    const hasEnoughElements = totalElements >= minElementsFor3Col;
    const hasGoodDensity = totalGroups > 0 && (totalElements / totalGroups) >= minElementsPerGroup;

    const shouldUse3Col = hasContentInSecondContainer && (hasEnoughGroups || (hasEnoughElements && hasGoodDensity));

    console.log('🔍 3-Column layout analysis:');
    console.log(`  Container 1: ${container1Groups} groups, ${container1Elements} elements`);
    console.log(`  Container 2: ${container2Groups} groups, ${container2Elements} elements`);
    console.log(`  Total: ${totalGroups} groups, ${totalElements} elements`);
    console.log(`  Thresholds: ${minGroupsFor3Col} groups OR ${minElementsFor3Col} elements with ${minElementsPerGroup} avg density`);
    console.log(`  Result: ${shouldUse3Col ? 'USE 3-COLUMN' : 'USE 2-COLUMN FALLBACK'}`);

    return shouldUse3Col;
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
   * Refresh ResizeObserver to watch for new containers (useful after dynamic UI changes)
   */
  refreshResizeObserver() {
    if (this.resizeObserver) {
      console.log('🔍 Refreshing ResizeObserver for new containers');
      this.observeContainers();
    }
  }

  /**
   * Clean up event listeners and observers
   */
  destroy() {
    Object.values(this.breakpoints).forEach((mq) => {
      mq.removeEventListener('change', this.handleLayoutChange);
    });
    globalThis.removeEventListener('resize', this.handleResize);
    globalThis.removeEventListener('visibilitychange', this.forceLayoutUpdate);

    // Clean up ResizeObserver
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
      this.resizeObserver = null;
    }

    if (this.observedContainers) {
      this.observedContainers.clear();
    }

    clearTimeout(this.resizeTimeout);
    clearTimeout(this.resizeObserverTimeout);
  }
}

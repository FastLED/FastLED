/* eslint-disable no-console */
/* eslint-disable import/prefer-default-export */

/**
 * UI Layout Placement Manager
 *
 * Manages the positioning and layout of UI elements based on screen size and orientation.
 * Maintains the current behavior:
 * - Desktop/landscape mode (wide screens): UI controls on the right
 * - Portrait/narrow mode: UI controls below the canvas
 */
export class UILayoutPlacementManager {
  constructor() {
    this.mediaQuery = window.matchMedia('(min-width: 1200px)');
    this.isDesktopLayout = this.mediaQuery.matches;
    this.currentLayout = this.isDesktopLayout ? 'desktop' : 'portrait';

    // Bind the handler to maintain 'this' context
    this.handleLayoutChange = this.handleLayoutChange.bind(this);

    // Listen for changes in screen size
    this.mediaQuery.addEventListener('change', this.handleLayoutChange);

    // Apply initial layout
    this.applyLayout();
  }

  /**
   * Handle changes in screen size/orientation
   * @param {MediaQueryListEvent} e - The media query event
   */
  handleLayoutChange(e) {
    const wasDesktopLayout = this.isDesktopLayout;
    this.isDesktopLayout = e.matches;
    const newLayout = this.isDesktopLayout ? 'desktop' : 'portrait';

    if (this.currentLayout !== newLayout) {
      console.log(`Layout change: ${this.currentLayout} → ${newLayout}`);
      this.currentLayout = newLayout;
      this.applyLayout();
    }
  }

  /**
   * Apply the appropriate layout based on current screen size
   */
  applyLayout() {
    const mainContainer = document.getElementById('main-container');
    const topRow = document.getElementById('top-row');
    const canvasContainer = document.getElementById('canvas-container');
    const uiControls = document.getElementById('ui-controls');

    if (!mainContainer || !topRow || !canvasContainer || !uiControls) {
      console.warn('Layout containers not found, cannot apply layout');
      return;
    }

    // Remove existing layout classes
    mainContainer.classList.remove('desktop-layout', 'portrait-layout');
    topRow.classList.remove('desktop-layout', 'portrait-layout');
    canvasContainer.classList.remove('desktop-layout', 'portrait-layout');
    uiControls.classList.remove('desktop-layout', 'portrait-layout');

    // Apply new layout classes
    const layoutClass = this.isDesktopLayout ? 'desktop-layout' : 'portrait-layout';
    mainContainer.classList.add(layoutClass);
    topRow.classList.add(layoutClass);
    canvasContainer.classList.add(layoutClass);
    uiControls.classList.add(layoutClass);

    if (this.isDesktopLayout) {
      this.applyDesktopLayout();
    } else {
      this.applyPortraitLayout();
    }
  }

  /**
   * Apply desktop/landscape layout (UI on the right)
   */
  applyDesktopLayout() {
    const topRow = document.getElementById('top-row');
    const canvasContainer = document.getElementById('canvas-container');
    const uiControls = document.getElementById('ui-controls');

    // Set flex direction to row (side by side)
    if (topRow) {
      topRow.style.flexDirection = 'row';
      topRow.style.alignItems = 'flex-start';
      topRow.style.gap = '40px';
      topRow.style.justifyContent = 'center';
    }

    // Canvas on the left
    if (canvasContainer) {
      canvasContainer.style.order = '1';
      canvasContainer.style.flex = '0 0 auto';
    }

    // UI controls on the right
    if (uiControls) {
      uiControls.style.order = '2';
      uiControls.style.flex = '0 0 auto';
      uiControls.style.marginTop = '0';
      uiControls.style.maxWidth = '400px';
      uiControls.style.minWidth = '320px';
      uiControls.style.alignSelf = 'flex-start';
      uiControls.style.height = 'fit-content';
    }
  }

  /**
   * Apply portrait layout (UI below the canvas)
   */
  applyPortraitLayout() {
    const topRow = document.getElementById('top-row');
    const canvasContainer = document.getElementById('canvas-container');
    const uiControls = document.getElementById('ui-controls');

    // Set flex direction to column (stacked)
    if (topRow) {
      topRow.style.flexDirection = 'column';
      topRow.style.alignItems = 'center';
      topRow.style.gap = 'unset';
      topRow.style.justifyContent = 'center';
    }

    // Canvas on top
    if (canvasContainer) {
      canvasContainer.style.order = '1';
      canvasContainer.style.flex = '0 0 auto';
    }

    // UI controls below
    if (uiControls) {
      uiControls.style.order = '2';
      uiControls.style.flex = '1 1 auto';
      uiControls.style.marginTop = '20px';
      uiControls.style.maxWidth = 'unset';
      uiControls.style.minWidth = 'unset';
      uiControls.style.alignSelf = 'unset';
      uiControls.style.height = 'unset';
    }
  }

  /**
   * Get the current layout mode
   * @returns {string} Either 'desktop' or 'portrait'
   */
  getCurrentLayout() {
    return this.currentLayout;
  }

  /**
   * Check if currently in desktop layout
   * @returns {boolean} True if in desktop layout
   */
  isDesktop() {
    return this.isDesktopLayout;
  }

  /**
   * Check if currently in portrait layout
   * @returns {boolean} True if in portrait layout
   */
  isPortrait() {
    return !this.isDesktopLayout;
  }

  /**
   * Force a layout update (useful for dynamic changes)
   */
  forceLayoutUpdate() {
    this.applyLayout();
  }

  /**
   * Clean up event listeners
   */
  destroy() {
    this.mediaQuery.removeEventListener('change', this.handleLayoutChange);
  }
}

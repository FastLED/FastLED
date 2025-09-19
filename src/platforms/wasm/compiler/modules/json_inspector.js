/**
 * JSON UI Transport Inspector
 *
 * Development tool for monitoring JSON communication between
 * FastLED C++ backend and JavaScript frontend.
 */

export class JsonInspector {
  constructor() {
    this.isVisible = false;
    this.isPaused = false;
    this.eventBuffer = [];
    this.maxBufferSize = 1000;
    this.eventCounter = 0;
    this.dragData = null;
    this.lastDirection = null; // Track last direction for label optimization

    this.initializeDOM();
    this.setupEventListeners();

    // Make inspector globally accessible
    window.jsonInspector = this;
  }

  initializeDOM() {
    // Get DOM elements (button is now accessed via gear menu, so it's optional)
    this.button = document.getElementById('json-inspector-button'); // May be null - now in gear menu
    this.popup = document.getElementById('json-inspector-popup');
    this.overlay = document.getElementById('inspector-popup-overlay');
    this.closeBtn = document.getElementById('inspector-close-btn');
    this.logContainer = document.getElementById('inspector-log');
    this.clearBtn = document.getElementById('inspector-clear');
    this.pauseBtn = document.getElementById('inspector-pause');
    this.eventCountSpan = document.getElementById('event-count');
  }

  setupEventListeners() {
    // Button click to show/hide (only if button exists - now handled via gear menu)
    if (this.button) {
      this.button.addEventListener('click', () => this.toggle());
    }

    // Close button only (removed overlay click to allow interaction with UI below)
    this.closeBtn.addEventListener('click', () => this.hide());

    // Control buttons
    this.clearBtn.addEventListener('click', () => this.clear());
    this.pauseBtn.addEventListener('click', () => this.togglePause());

    // Dragging functionality
    this.setupDragging();
  }

  setupDragging() {
    const header = this.popup.querySelector('.inspector-header');

    header.addEventListener('mousedown', (e) => {
      this.dragData = {
        startX: e.clientX - this.popup.offsetLeft,
        startY: e.clientY - this.popup.offsetTop,
      };

      document.addEventListener('mousemove', this.handleDrag);
      document.addEventListener('mouseup', this.handleDragEnd);

      e.preventDefault();
    });
  }

  handleDrag = (e) => {
    if (!this.dragData) return;

    const x = e.clientX - this.dragData.startX;
    const y = e.clientY - this.dragData.startY;

    this.popup.style.left = `${Math.max(0, x)}px`;
    this.popup.style.top = `${Math.max(0, y)}px`;
  };

  handleDragEnd = () => {
    this.dragData = null;
    document.removeEventListener('mousemove', this.handleDrag);
    document.removeEventListener('mouseup', this.handleDragEnd);
  };

  toggle() {
    if (this.isVisible) {
      this.hide();
    } else {
      this.show();
    }
  }

  show() {
    this.isVisible = true;
    this.overlay.style.display = 'block';
    this.popup.style.display = 'block';
    this.button.classList.add('active');
  }

  hide() {
    this.isVisible = false;
    this.overlay.style.display = 'none';
    this.popup.style.display = 'none';
    this.button.classList.remove('active');
  }

  togglePause() {
    this.isPaused = !this.isPaused;
    this.pauseBtn.textContent = this.isPaused ? 'Resume' : 'Pause';
    this.pauseBtn.classList.toggle('paused', this.isPaused);
  }

  clear() {
    this.eventBuffer = [];
    this.eventCounter = 0;
    this.lastDirection = null;
    this.logContainer.innerHTML = '';
    this.updateEventCounter();
  }

  logEvent(data, direction, timestamp = null) {
    if (this.isPaused) return;

    const event = {
      id: ++this.eventCounter,
      timestamp: timestamp || new Date(),
      direction,
      data: typeof data === 'string' ? data : JSON.stringify(data, null, 2),
      type: this.getEventType(data),
    };

    this.eventBuffer.push(event);

    // Maintain buffer size
    if (this.eventBuffer.length > this.maxBufferSize) {
      this.eventBuffer.shift();
    }

    this.renderEvent(event);
    this.updateEventCounter();
    // Removed scrollToBottom() - newest events now appear at top
  }

  logInboundEvent(data, source = 'C++') {
    // If source already contains direction arrows, use it directly
    const direction = source.includes('→') || source.includes('←') ? source : `IN ← ${source}`;
    this.logEvent(data, direction);
  }

  logOutboundEvent(data, target = 'C++') {
    // If target already contains direction arrows, use it directly
    const direction = target.includes('→') || target.includes('←') ? target : `OUT → ${target}`;
    this.logEvent(data, direction);
  }

  getEventType(data) {
    if (typeof data === 'string') {
      try {
        const parsed = JSON.parse(data);
        return this.getEventType(parsed);
      } catch {
        return 'string';
      }
    }

    if (Array.isArray(data)) {
      if (data.length > 0 && data[0].name) {
        return 'ui-elements';
      }
      return 'array';
    }

    if (typeof data === 'object' && data !== null) {
      const keys = Object.keys(data);
      if (keys.some((key) => key.includes('slider') || key.includes('button'))) {
        return 'ui-update';
      }
      return 'object';
    }

    return 'unknown';
  }

  renderEvent(event) {
    const eventElement = document.createElement('div');
    eventElement.className = `inspector-event ${event.direction.includes('IN') ? 'inbound' : 'outbound'}`;

    // Only show direction if it's different from the last direction, or if it's the first event
    const showDirection = this.lastDirection === null || this.lastDirection !== event.direction;

    if (showDirection) {
      // Direction change: show direction on first row, then count + time + JSON on second row
      eventElement.innerHTML = `
                <div class="direction-header">
                    ${event.direction}
                </div>
                <div class="event-info-row">
                    <span class="event-id">#${event.id}</span>
                    <span class="separator">|</span>
                    <span class="event-timestamp">${event.timestamp.toLocaleTimeString()}</span>
                    <span class="separator">|</span>
                    <span class="json-data">${this.formatJson(event.data)}</span>
                </div>
            `;
    } else {
      // Same direction: show count + time + JSON on one row
      eventElement.innerHTML = `
                <div class="event-info-row">
                    <span class="event-id">#${event.id}</span>
                    <span class="separator">|</span>
                    <span class="event-timestamp">${event.timestamp.toLocaleTimeString()}</span>
                    <span class="separator">|</span>
                    <span class="json-data">${this.formatJson(event.data)}</span>
                </div>
            `;
    }

    // Update last direction
    this.lastDirection = event.direction;

    this.logContainer.insertBefore(eventElement, this.logContainer.firstChild);
  }

  formatJson(data) {
    try {
      const parsed = typeof data === 'string' ? JSON.parse(data) : data;
      const jsonString = JSON.stringify(parsed);

      // For small JSON objects (under 100 chars), keep on one line
      // For larger objects, use pretty printing
      if (jsonString.length < 100) {
        return jsonString;
      }
      return JSON.stringify(parsed, null, 2);
    } catch {
      return data;
    }
  }

  updateEventCounter() {
    this.eventCountSpan.textContent = String(this.eventCounter);
  }

  scrollToBottom() {
    this.logContainer.scrollTop = this.logContainer.scrollHeight;
  }

  wrapUiUpdateCallback(originalCallback) {
    return (jsonString) => {
      // Log the outbound call
      this.logOutboundEvent(jsonString, 'JS → C++');

      // Call the original callback
      return originalCallback(jsonString);
    };
  }
}

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
        
        this.initializeDOM();
        this.setupEventListeners();
        
        // Make inspector globally accessible
        window.jsonInspector = this;
    }

    initializeDOM() {
        // Get DOM elements
        this.button = document.getElementById('json-inspector-button');
        this.popup = document.getElementById('json-inspector-popup');
        this.overlay = document.getElementById('inspector-popup-overlay');
        this.closeBtn = document.getElementById('inspector-close-btn');
        this.logContainer = document.getElementById('inspector-log');
        this.clearBtn = document.getElementById('inspector-clear');
        this.pauseBtn = document.getElementById('inspector-pause');
        this.eventCountSpan = document.getElementById('event-count');
    }

    setupEventListeners() {
        // Button click to show/hide
        this.button.addEventListener('click', () => this.toggle());
        
        // Close button and overlay
        this.closeBtn.addEventListener('click', () => this.hide());
        this.overlay.addEventListener('click', () => this.hide());
        
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
                startY: e.clientY - this.popup.offsetTop
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
            type: this.getEventType(data)
        };
        
        this.eventBuffer.push(event);
        
        // Maintain buffer size
        if (this.eventBuffer.length > this.maxBufferSize) {
            this.eventBuffer.shift();
        }
        
        this.renderEvent(event);
        this.updateEventCounter();
        this.scrollToBottom();
    }

    logInboundEvent(data, source = 'C++') {
        this.logEvent(data, `IN ← ${source}`);
    }

    logOutboundEvent(data, target = 'C++') {
        this.logEvent(data, `OUT → ${target}`);
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
            if (keys.some(key => key.includes('slider') || key.includes('button'))) {
                return 'ui-update';
            }
            return 'object';
        }
        
        return 'unknown';
    }

    renderEvent(event) {
        const eventElement = document.createElement('div');
        eventElement.className = `inspector-event ${event.direction.includes('IN') ? 'inbound' : 'outbound'}`;
        eventElement.innerHTML = `
            <div class="event-header">
                <span class="event-direction">${event.direction}</span>
                <span class="event-timestamp">${event.timestamp.toLocaleTimeString()}</span>
                <span class="event-type">${event.type}</span>
                <span class="event-id">#${event.id}</span>
            </div>
            <div class="event-data">
                <pre>${this.formatJson(event.data)}</pre>
            </div>
        `;
        
        this.logContainer.appendChild(eventElement);
    }

    formatJson(data) {
        try {
            const parsed = typeof data === 'string' ? JSON.parse(data) : data;
            return JSON.stringify(parsed, null, 2);
        } catch {
            return data;
        }
    }

    updateEventCounter() {
        this.eventCountSpan.textContent = this.eventCounter;
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

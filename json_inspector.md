# JSON UI Transport Inspector Design Document

## Overview

The JSON UI Transport Inspector is a development tool that provides real-time visibility into JSON communication between the FastLED C++ backend and JavaScript frontend. This inspector will help developers understand the FastLED JSON UI system and debug communication issues.

## Purpose

- **Display JSON UI events** flowing in both directions (C++ ↔ JavaScript)
- **Provide developer insights** into the FastLED JSON UI communication protocol
- **Enable debugging** of UI synchronization issues
- **Document the communication flow** for developers integrating with FastLED

## Requirements

### Constraints
- ✅ **NO C++ changes** - All modifications must be in JavaScript/HTML/CSS files only
- ✅ **Platform restriction** - All changes must go into `src/platforms/wasm/**`
- ✅ **Non-intrusive** - Must not affect existing UI functionality
- ✅ **Development-focused** - Tool for developers, not end users

### UI Requirements
- **Console button**: Emoticon button (📊) representing a console/inspector
- **Popup window**: Draggable window with title bar and close (X) button  
- **Event display**: Shows JSON events with directional indicators
- **Buffer management**: 1000 line circular buffer with scrollbars
- **Real-time updates**: Live capture of JSON communication

## Technical Architecture

### Current JSON Communication Flow

```
C++ Backend ←→ JavaScript Frontend
     ↓              ↑
updateJs()    onFastLedUiUpdateFunction()
     ↓              ↑
EM_ASM({...})  Module.cwrap(...)
     ↓              ↑
FastLED_onUiElementsAdded  jsUpdateUiComponents
```

### Inspection Points

The inspector will hook into these existing communication channels:

1. **C++ → JS**: Hook `FastLED_onUiElementsAdded` calls
2. **JS → C++**: Hook `onFastLedUiUpdateFunction` calls

## Implementation Plan

### File Structure

All new files will be created in `src/platforms/wasm/compiler/modules/`:

```
src/platforms/wasm/compiler/modules/
├── json_inspector.js          (NEW - Main inspector module)
└── json_inspector.css         (NEW - Inspector-specific styles)
```

### Modified Files

#### 1. `src/platforms/wasm/compiler/index.html`
**Changes:**
- Add inspector button to the canvas container (near gear icon)
- Add inspector popup HTML structure
- Include new CSS file

```html
<!-- Add after gear-menu-container -->
<div id="json-inspector-container">
    <button id="json-inspector-button" class="inspector-button" 
            aria-label="JSON Inspector" title="JSON UI Inspector">
        📊
    </button>
</div>

<!-- Add before closing body tag -->
<div class="inspector-popup-overlay" id="inspector-popup-overlay"></div>
<div class="inspector-popup" id="json-inspector-popup">
    <div class="inspector-header">
        <span class="inspector-title">JSON UI Inspector</span>
        <button class="inspector-close" id="inspector-close-btn">&times;</button>
    </div>
    <div class="inspector-content">
        <div class="inspector-controls">
            <button id="inspector-clear">Clear</button>
            <button id="inspector-pause">Pause</button>
            <span class="event-counter">Events: <span id="event-count">0</span></span>
        </div>
        <div class="inspector-log" id="inspector-log"></div>
    </div>
</div>
```

#### 2. `src/platforms/wasm/compiler/index.css`
**Changes:**
- Add import for inspector CSS
- Add base styles for inspector button

```css
/* Add at top with other imports */
@import url('./modules/json_inspector.css');

/* Add after gear button styles */
.inspector-button {
    background: rgba(255, 255, 255, 0.1);
    border: 1px solid rgba(255, 255, 255, 0.2);
    color: #E0E0E0;
    border-radius: 6px;
    padding: 8px;
    cursor: pointer;
    font-size: 16px;
    margin-left: 8px;
    transition: all 0.2s ease;
}

.inspector-button:hover {
    background: rgba(255, 255, 255, 0.2);
    border-color: rgba(255, 255, 255, 0.3);
}
```

#### 3. `src/platforms/wasm/compiler/index.js`
**Changes:**
- Import and initialize the JSON inspector
- Set up inspector hooks

```javascript
// Add import at top
import { JsonInspector } from './modules/json_inspector.js';

// Add after uiManager initialization
const jsonInspector = new JsonInspector();

// Modify FastLED_onFrame function to hook inspector
function FastLED_onFrame(frameData, uiUpdateCallback) {
    // Hook the UI update callback
    const wrappedCallback = jsonInspector.wrapUiUpdateCallback(uiUpdateCallback);
    
    const changesJson = uiManager.processUiChanges();
    if (changesJson !== null) {
        const changesJsonStr = JSON.stringify(changesJson);
        wrappedCallback(changesJsonStr);
    }
    // ... rest of existing function
}

// Modify globalThis.FastLED_onUiElementsAdded assignment
globalThis.FastLED_onUiElementsAdded = function(jsonData) {
    jsonInspector.logInboundEvent(jsonData);
    // ... existing functionality
};
```

#### 4. `src/platforms/wasm/compiler/modules/ui_manager.js`
**Changes:**
- Add inspector logging to key methods
- Hook JSON processing

```javascript
// Add import at top
import { JsonInspector } from './json_inspector.js';

// In JsonUiManager class, modify updateUiComponents method
updateUiComponents(jsonString) {
    // Log the inbound update
    if (window.jsonInspector) {
        window.jsonInspector.logInboundEvent(jsonString, 'C++ → JS');
    }
    
    // ... rest of existing method
}

// In processUiChanges method, add logging before return
processUiChanges() {
    // ... existing logic ...
    
    if (hasChanges) {
        // Log outbound changes
        if (window.jsonInspector) {
            window.jsonInspector.logOutboundEvent(transformedChanges, 'JS → C++');
        }
        return transformedChanges;
    }
    
    return null;
}
```

## New Files Implementation

### 1. `src/platforms/wasm/compiler/modules/json_inspector.js`

```javascript
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
```

### 2. `src/platforms/wasm/compiler/modules/json_inspector.css`

```css
/* JSON Inspector Styles */

.inspector-button.active {
    background: rgba(100, 150, 255, 0.3);
    border-color: rgba(100, 150, 255, 0.5);
    color: #6495ff;
}

.inspector-popup-overlay {
    display: none;
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.5);
    z-index: 9999;
}

.inspector-popup {
    display: none;
    position: fixed;
    top: 50px;
    left: 50px;
    width: 800px;
    height: 600px;
    max-width: 90vw;
    max-height: 90vh;
    background: #1A1A1A;
    border: 1px solid #333;
    border-radius: 8px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
    z-index: 10000;
    overflow: hidden;
    resize: both;
    min-width: 400px;
    min-height: 300px;
}

.inspector-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 12px 16px;
    background: #2A2A2A;
    border-bottom: 1px solid #333;
    cursor: move;
    user-select: none;
}

.inspector-title {
    font-weight: bold;
    color: #E0E0E0;
}

.inspector-close {
    background: none;
    border: none;
    color: #E0E0E0;
    font-size: 20px;
    cursor: pointer;
    padding: 0;
    width: 24px;
    height: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 4px;
    transition: background-color 0.2s ease;
}

.inspector-close:hover {
    background: rgba(255, 255, 255, 0.1);
}

.inspector-content {
    display: flex;
    flex-direction: column;
    height: calc(100% - 49px);
}

.inspector-controls {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 16px;
    background: #222;
    border-bottom: 1px solid #333;
}

.inspector-controls button {
    background: #333;
    border: 1px solid #555;
    color: #E0E0E0;
    padding: 6px 12px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
    transition: all 0.2s ease;
}

.inspector-controls button:hover {
    background: #444;
    border-color: #666;
}

.inspector-controls button.paused {
    background: #ff6b35;
    border-color: #ff8c42;
}

.event-counter {
    color: #999;
    font-size: 12px;
    margin-left: auto;
}

.inspector-log {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
    font-family: 'Courier New', monospace;
    font-size: 12px;
    line-height: 1.4;
}

.inspector-event {
    margin-bottom: 12px;
    border: 1px solid #333;
    border-radius: 4px;
    overflow: hidden;
    background: #1E1E1E;
}

.inspector-event.inbound {
    border-left: 4px solid #4CAF50;
}

.inspector-event.outbound {
    border-left: 4px solid #2196F3;
}

.event-header {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 12px;
    background: #2A2A2A;
    border-bottom: 1px solid #333;
    font-size: 11px;
}

.event-direction {
    font-weight: bold;
    color: #E0E0E0;
    min-width: 80px;
}

.event-timestamp {
    color: #999;
    min-width: 80px;
}

.event-type {
    color: #6495ff;
    background: rgba(100, 149, 237, 0.1);
    padding: 2px 6px;
    border-radius: 3px;
    font-size: 10px;
}

.event-id {
    color: #666;
    margin-left: auto;
}

.event-data {
    padding: 8px 12px;
    max-height: 300px;
    overflow-y: auto;
}

.event-data pre {
    margin: 0;
    color: #E0E0E0;
    white-space: pre-wrap;
    word-wrap: break-word;
    font-size: 11px;
    line-height: 1.3;
}

/* JSON Syntax Highlighting */
.event-data pre .json-key {
    color: #6495ff;
}

.event-data pre .json-string {
    color: #90EE90;
}

.event-data pre .json-number {
    color: #FFD700;
}

.event-data pre .json-boolean {
    color: #FF6347;
}

.event-data pre .json-null {
    color: #999;
}

/* Scrollbar styling for webkit browsers */
.inspector-log::-webkit-scrollbar,
.event-data::-webkit-scrollbar {
    width: 8px;
}

.inspector-log::-webkit-scrollbar-track,
.event-data::-webkit-scrollbar-track {
    background: #1A1A1A;
}

.inspector-log::-webkit-scrollbar-thumb,
.event-data::-webkit-scrollbar-thumb {
    background: #444;
    border-radius: 4px;
}

.inspector-log::-webkit-scrollbar-thumb:hover,
.event-data::-webkit-scrollbar-thumb:hover {
    background: #555;
}

/* Responsive adjustments */
@media (max-width: 900px) {
    .inspector-popup {
        width: 95vw;
        height: 80vh;
        top: 10vh;
        left: 2.5vw;
    }
}
```

## Communication Flow with Inspector

```mermaid
graph TB
    A[C++ Backend] -->|updateJs()| B[FastLED_onUiElementsAdded]
    B --> C[JSON Inspector - Log IN]
    B --> D[UI Manager - addUiElements]
    
    E[UI Manager - processUiChanges] --> F[JSON Inspector - Log OUT]
    F --> G[onFastLedUiUpdateFunction]
    G -->|jsUpdateUiComponents| H[C++ Backend]
    
    I[Inspector Button] --> J[Toggle Inspector Popup]
    J --> K[Real-time Event Display]
    
    style C fill:#4CAF50
    style F fill:#2196F3
    style K fill:#FF9800
```

## Testing Plan

### Manual Testing
1. **Basic functionality**: Open inspector, verify button works
2. **Event capture**: Interact with UI elements, verify events are logged
3. **Buffer management**: Generate >1000 events, verify circular buffer
4. **Popup functionality**: Test dragging, resizing, close button
5. **Pause/resume**: Test event logging pause/resume
6. **Clear function**: Test event log clearing

### Integration Testing
1. **No interference**: Verify existing UI functionality is unaffected
2. **Memory usage**: Monitor memory consumption with inspector active
3. **Performance**: Verify no significant performance impact
4. **Error handling**: Test with malformed JSON, network issues

## Benefits for Developers

### Understanding FastLED JSON Protocol
- **Real-time visibility** into UI communication
- **Message structure** documentation through examples
- **Debugging capabilities** for integration issues
- **Performance monitoring** of UI update frequency

### Use Cases
1. **Custom UI development**: Understanding message formats
2. **Integration debugging**: Identifying communication failures
3. **Performance optimization**: Monitoring message frequency
4. **Protocol documentation**: Auto-generating API docs from captured events

## Future Enhancements

### Phase 2 Features (Not in Initial Implementation)
- **Message filtering**: Filter by event type or content
- **Export functionality**: Save captured events to file
- **Statistics panel**: Message frequency, size statistics
- **Search functionality**: Search through captured events
- **Message validation**: Validate JSON structure
- **Performance metrics**: Timing analysis of UI updates

## Alternative Implementation: Hidden Div Buffer Approach

### Overview of Alternative
Instead of the full-featured inspector module, this alternative uses a simpler approach:
- **Hidden div buffer** that accumulates JSON events as HTML
- **Lightweight popup** that displays the hidden div contents
- **Minimal JavaScript** for hooking and buffer management
- **Same 1000 line limit** maintained in the hidden div

### How It Works

#### 1. Hidden Div Buffer System
```html
<!-- Hidden div that accumulates events (added to index.html) -->
<div id="json-event-buffer" style="display: none;"></div>
```

#### 2. Event Capture & Buffering
```javascript
// Simple hooking system in index.js
function logJsonEvent(data, direction) {
    const buffer = document.getElementById('json-event-buffer');
    const timestamp = new Date().toLocaleTimeString();
    
    // Create event HTML
    const eventHtml = `
        <div class="json-event ${direction.includes('IN') ? 'inbound' : 'outbound'}">
            <div class="event-header">
                <span class="direction">${direction}</span>
                <span class="time">${timestamp}</span>
            </div>
            <pre class="event-data">${JSON.stringify(data, null, 2)}</pre>
        </div>
    `;
    
    // Append to buffer
    buffer.innerHTML += eventHtml;
    
    // Maintain 1000 line limit
    const events = buffer.children;
    if (events.length > 1000) {
        buffer.removeChild(events[0]);
    }
}

// Hook into existing communication points
const originalOnUiElementsAdded = globalThis.FastLED_onUiElementsAdded;
globalThis.FastLED_onUiElementsAdded = function(jsonData) {
    logJsonEvent(jsonData, 'IN ← C++');
    if (originalOnUiElementsAdded) originalOnUiElementsAdded(jsonData);
};

const originalUpdateCallback = uiUpdateCallback;
function wrappedUpdateCallback(jsonString) {
    logJsonEvent(JSON.parse(jsonString), 'OUT → C++');
    return originalUpdateCallback(jsonString);
}
```

#### 3. Simple Popup Display
```javascript
// Popup management
function toggleInspector() {
    const popup = document.getElementById('json-inspector-popup');
    const buffer = document.getElementById('json-event-buffer');
    const display = document.getElementById('inspector-display');
    
    if (popup.style.display === 'none') {
        // Copy buffer contents to popup display
        display.innerHTML = buffer.innerHTML;
        popup.style.display = 'block';
        
        // Auto-scroll to bottom
        display.scrollTop = display.scrollHeight;
    } else {
        popup.style.display = 'none';
    }
}

function clearBuffer() {
    document.getElementById('json-event-buffer').innerHTML = '';
    document.getElementById('inspector-display').innerHTML = '';
}
```

#### 4. Simplified HTML Structure
```html
<!-- Inspector button (same as main approach) -->
<button id="json-inspector-button" onclick="toggleInspector()">📊</button>

<!-- Simplified popup -->
<div id="json-inspector-popup" class="inspector-popup" style="display: none;">
    <div class="inspector-header">
        <span>JSON UI Inspector</span>
        <button onclick="clearBuffer()">Clear</button>
        <button onclick="toggleInspector()">&times;</button>
    </div>
    <div id="inspector-display" class="inspector-content"></div>
</div>
```

### Benefits of Hidden Div Approach

#### Advantages
- ✅ **Simpler implementation** - ~50 lines of JavaScript vs 400+
- ✅ **Less memory overhead** - HTML accumulation vs JavaScript objects
- ✅ **Easier debugging** - Can inspect hidden div in DevTools
- ✅ **Faster development** - No separate modules or complex state management
- ✅ **Built-in persistence** - Events remain in DOM until cleared
- ✅ **Automatic rendering** - Browser handles HTML formatting

#### Trade-offs
- ❌ **Less features** - No pause/resume, event filtering, or advanced controls
- ❌ **DOM pollution** - Hidden div grows with events (but limited to 1000)
- ❌ **Less flexibility** - Harder to add advanced features later
- ❌ **Memory behavior** - DOM nodes vs JavaScript objects have different GC behavior

### Implementation Comparison

| Feature | Full Inspector Module | Hidden Div Buffer |
|---------|----------------------|-------------------|
| **Code Complexity** | ~650 lines | ~100 lines |
| **Memory Usage** | JS objects + DOM | DOM only |
| **Features** | Full featured | Basic display |
| **Extensibility** | High | Limited |
| **Development Time** | 2-3 days | 4-6 hours |
| **Maintenance** | Moderate | Low |
| **Performance** | Good | Very good |

### Buffer Management Detail

#### 1000 Line Circular Buffer in Hidden Div
```javascript
function maintainBufferLimit(bufferElement, maxLines = 1000) {
    const events = bufferElement.children;
    
    // Remove oldest events when limit exceeded
    while (events.length > maxLines) {
        bufferElement.removeChild(events[0]);
    }
    
    // Optional: Add visual indicator when buffer is full
    if (events.length === maxLines) {
        const indicator = document.createElement('div');
        indicator.className = 'buffer-full-indicator';
        indicator.textContent = '--- Buffer Full (showing last 1000 events) ---';
        bufferElement.insertBefore(indicator, events[1]);
        bufferElement.removeChild(events[0]); // Remove oldest to make room
    }
}
```

#### Real-time Update Strategy
```javascript
// Option 1: Update popup in real-time (if visible)
function logJsonEvent(data, direction) {
    const buffer = document.getElementById('json-event-buffer');
    const popup = document.getElementById('json-inspector-popup');
    const display = document.getElementById('inspector-display');
    
    // Add to buffer
    appendEventToBuffer(buffer, data, direction);
    maintainBufferLimit(buffer);
    
    // If popup is visible, update it too
    if (popup.style.display !== 'none') {
        appendEventToBuffer(display, data, direction);
        maintainBufferLimit(display);
        display.scrollTop = display.scrollHeight;
    }
}

// Option 2: Refresh popup content on toggle (simpler)
function toggleInspector() {
    const popup = document.getElementById('json-inspector-popup');
    const buffer = document.getElementById('json-event-buffer');
    const display = document.getElementById('inspector-display');
    
    if (popup.style.display === 'none') {
        // Copy entire buffer to display
        display.innerHTML = buffer.innerHTML;
        popup.style.display = 'block';
        display.scrollTop = display.scrollHeight;
    } else {
        popup.style.display = 'none';
    }
}
```

### Recommended Approach

**For Rapid Prototyping**: Use the Hidden Div Buffer approach
- Faster to implement and test
- Provides immediate value for developers
- Can be enhanced later if needed

**For Production Tools**: Use the Full Inspector Module approach
- More professional and feature-complete
- Better performance for heavy usage
- More maintainable long-term

**Hybrid Approach**: Start with Hidden Div, migrate to Full Module
1. Implement Hidden Div Buffer for immediate debugging needs
2. Collect feedback from developers using it
3. Enhance with Full Inspector Module features based on real usage patterns

## Installation Instructions

### For Developers
1. Copy new files to `src/platforms/wasm/compiler/modules/`
2. Apply modifications to existing files as documented
3. Rebuild WASM application
4. Inspector button will appear next to the gear icon
5. Click to open/close the JSON inspector popup

### No Configuration Required
The inspector is automatically available in development builds and requires no configuration or setup beyond file installation.

---

*This document serves as the complete specification for implementing the JSON UI Transport Inspector for the FastLED WASM platform.* 

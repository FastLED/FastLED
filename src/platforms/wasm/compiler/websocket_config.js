/**
 * FastLED WebSocket Configuration Module
 * 
 * Automatically configures emscripten WebSocket URL for Flask WebSocket proxy.
 * This enables POSIX socket emulation over WebSockets when the Flask server
 * is running with WebSocket proxy support at /wss_socket_proxy.
 * 
 * Key Features:
 * - Automatic WebSocket URL configuration 
 * - No complex detection logic - just works or fails gracefully
 * - Points to Flask server WebSocket proxy route
 * - Falls back gracefully when proxy unavailable
 */

/**
 * Configures the emscripten Module for WebSocket proxy support
 * @param {Object} Module - The emscripten Module object to configure
 */
function configureWebSocketProxy(Module) {
    // Get the current page's host and port
    const currentHost = window.location.hostname || 'localhost';
    const currentPort = window.location.port || '8089';
    
    // Configure WebSocket URL for Flask proxy route
    const websocketUrl = `ws://${currentHost}:${currentPort}/wss_socket_proxy`;
    
    console.log(`🌐 Configuring WebSocket proxy: ${websocketUrl}`);
    
    // Set the WebSocket URL that emscripten will use for socket proxying
    Module.websockifyURL = websocketUrl;
    
    console.log('✅ WebSocket proxy configured - sockets will use Flask /wss_socket_proxy route');
    console.log('ℹ️  If Flask WebSocket proxy is not available, socket calls will fail gracefully');
}

/**
 * Pre-run hook to configure WebSocket proxy before Module initialization
 * This ensures the WebSocket URL is set before any socket operations
 */
function addWebSocketPreRun() {
    // Add to Module.preRun array to configure before WASM loads
    if (!window.Module) {
        window.Module = {};
    }
    
    if (!window.Module.preRun) {
        window.Module.preRun = [];
    }
    
    // Add our configuration function to preRun
    window.Module.preRun.push(function() {
        configureWebSocketProxy(window.Module);
    });
    
    console.log('📡 WebSocket configuration added to Module.preRun');
}

// Auto-configure when this module loads
addWebSocketPreRun();

// Export for manual configuration if needed
export { configureWebSocketProxy, addWebSocketPreRun }; 

/**
 * FastLED UI Recorder Test Module
 *
 * Simple tests to verify UI recording functionality works correctly.
 * This module can be imported to run basic tests on the recording system.
 *
 * @module UIRecorderTest
 */

/* eslint-disable no-console */

import { UIRecorder } from './ui_recorder.js';
import { UIPlayback } from './ui_playback.js';

/**
 * Runs basic tests on the UI recording system
 * @returns {Promise<boolean>} Test success status
 */
export async function runUIRecorderTests() {
    console.log('🧪 Running UI Recorder Tests...');

    try {
        // Test 1: Basic recorder functionality
        const testResult1 = await testBasicRecording();
        if (!testResult1) {
            console.error('❌ Basic recording test failed');
            return false;
        }

        // Test 2: Recording serialization
        const testResult2 = await testRecordingSerialization();
        if (!testResult2) {
            console.error('❌ Recording serialization test failed');
            return false;
        }

        // Test 3: Playback functionality (if UI manager is available)
        if (window.uiManager) {
            const testResult3 = await testPlaybackFunctionality();
            if (!testResult3) {
                console.error('❌ Playback functionality test failed');
                return false;
            }
        } else {
            console.log('⚠️ UI Manager not available, skipping playback test');
        }

        console.log('✅ All UI Recorder tests passed!');
        return true;
    } catch (error) {
        console.error('❌ UI Recorder tests failed with error:', error);
        return false;
    }
}

/**
 * Tests basic recording functionality
 * @returns {Promise<boolean>} Test success status
 */
async function testBasicRecording() {
    console.log('  Testing basic recording...');

    const recorder = new UIRecorder({ debugMode: true });

    // Start recording
    const recordingId = recorder.startRecording({ testMode: true });
    if (!recordingId) {
        console.error('    Failed to start recording');
        return false;
    }

    // Simulate some UI events
    recorder.recordElementAdd('test_slider', {
        type: 'slider',
        id: 'test_slider',
        name: 'Test Slider',
        min: 0,
        max: 100,
        value: 50,
        step: 1
    });

    const SLIDER_VALUE_1 = 75;
    const SLIDER_VALUE_2 = 25;
    const INITIAL_VALUE = 50;
    recorder.recordElementUpdate('test_slider', SLIDER_VALUE_1, INITIAL_VALUE);
    recorder.recordElementUpdate('test_slider', SLIDER_VALUE_2, SLIDER_VALUE_1);

    recorder.recordElementAdd('test_checkbox', {
        type: 'checkbox',
        id: 'test_checkbox',
        name: 'Test Checkbox',
        value: false
    });

    recorder.recordElementUpdate('test_checkbox', true, false);
    recorder.recordElementRemove('test_slider');

    // Wait a bit to get different timestamps
    const TIMESTAMP_DELAY = 100;
    await new Promise(resolve => {
        setTimeout(resolve, TIMESTAMP_DELAY);
    });

    // Stop recording
    const recording = recorder.stopRecording();
    if (!recording) {
        console.error('    Failed to stop recording');
        return false;
    }

    // Verify recording structure
    if (!recording.recording || !recording.events) {
        console.error('    Invalid recording structure');
        return false;
    }

    if (recording.events.length !== 5) {
        console.error(`    Expected 5 events, got ${recording.events.length}`);
        return false;
    }

    // Verify event types
    const expectedTypes = ['add', 'update', 'update', 'add', 'update'];
    for (let i = 0; i < expectedTypes.length; i++) {
        if (recording.events[i].type !== expectedTypes[i]) {
            console.error(`    Event ${i}: expected ${expectedTypes[i]}, got ${recording.events[i].type}`);
            return false;
        }
    }

    console.log('  ✅ Basic recording test passed');
    return true;
}

/**
 * Tests recording serialization and import
 * @returns {Promise<boolean>} Test success status
 */
async function testRecordingSerialization() {
    console.log('  Testing recording serialization...');

    const recorder = new UIRecorder({ debugMode: true });

    // Create a simple recording
    recorder.startRecording({ testMode: true });
    recorder.recordElementAdd('test_button', {
        type: 'button',
        id: 'test_button',
        name: 'Test Button',
        value: false
    });
    recorder.recordElementUpdate('test_button', true, false);

    const recording = recorder.stopRecording();

    // Export to JSON
    const jsonString = recorder.exportRecording();
    if (!jsonString) {
        console.error('    Failed to export recording');
        return false;
    }

    // Verify JSON can be parsed
    try {
        JSON.parse(jsonString);
    } catch (error) {
        console.error('    Failed to parse exported JSON:', error);
        return false;
    }

    // Create a new recorder and import
    const recorder2 = new UIRecorder({ debugMode: true });
    const importSuccess = recorder2.importRecording(jsonString);
    if (!importSuccess) {
        console.error('    Failed to import recording');
        return false;
    }

    // Verify imported data matches original
    const importedEvents = recorder2.events;
    if (importedEvents.length !== recording.events.length) {
        console.error('    Imported event count mismatch');
        return false;
    }

    console.log('  ✅ Recording serialization test passed');
    return true;
}

/**
 * Tests playback functionality
 * @returns {Promise<boolean>} Test success status
 */
async function testPlaybackFunctionality() {
    console.log('  Testing playback functionality...');

    // Create a simple recording
    const recorder = new UIRecorder({ debugMode: true });
    recorder.startRecording({ testMode: true });

    recorder.recordElementAdd('playback_test', {
        type: 'slider',
        id: 'playback_test',
        name: 'Playback Test',
        min: 0,
        max: 100,
        value: 0
    });

    const EVENT_DELAY = 50;
    const PLAYBACK_VALUE_1 = 50;
    const PLAYBACK_VALUE_2 = 100;
    const PLAYBACK_INITIAL = 0;

    await new Promise(resolve => {
        setTimeout(resolve, EVENT_DELAY);
    });
    recorder.recordElementUpdate('playback_test', PLAYBACK_VALUE_1, PLAYBACK_INITIAL);

    await new Promise(resolve => {
        setTimeout(resolve, EVENT_DELAY);
    });
    recorder.recordElementUpdate('playback_test', PLAYBACK_VALUE_2, PLAYBACK_VALUE_1);

    const recording = recorder.stopRecording();

    // Test playback
    const playback = new UIPlayback(window.uiManager, { debugMode: true });

    const loadSuccess = playback.loadRecording(recording);
    if (!loadSuccess) {
        console.error('    Failed to load recording for playback');
        return false;
    }

    // Test playback controls
    const startSuccess = playback.startPlayback();
    if (!startSuccess) {
        console.error('    Failed to start playback');
        return false;
    }

    // Let it play for a moment
    const PLAYBACK_DELAY = 200;
    await new Promise(resolve => {
        setTimeout(resolve, PLAYBACK_DELAY);
    });

    // Test pause/resume
    playback.pausePlayback();
    const status1 = playback.getStatus();
    if (!status1.isPaused) {
        console.error('    Playback pause failed');
        return false;
    }

    playback.resumePlayback();
    const status2 = playback.getStatus();
    if (status2.isPaused) {
        console.error('    Playback resume failed');
        return false;
    }

    // Stop playback
    playback.stopPlayback();
    const status3 = playback.getStatus();
    if (status3.isPlaying) {
        console.error('    Playback stop failed');
        return false;
    }

    // Clean up
    playback.destroy();

    console.log('  ✅ Playback functionality test passed');
    return true;
}

/**
 * Demonstrates UI recording capabilities
 */
export function demonstrateUIRecording() {
    console.log('🎬 UI Recording Demonstration');
    console.log('');
    console.log('Available global functions:');
    console.log('  startUIRecording(metadata) - Start recording UI events');
    console.log('  stopUIRecording() - Stop recording and get data');
    console.log('  getUIRecordingStatus() - Get current recording status');
    console.log('  exportUIRecording() - Export recording as JSON');
    console.log('  clearUIRecording() - Clear current recording');
    console.log('');
    console.log('Playback functions:');
    console.log('  loadUIRecordingForPlayback(recording) - Load recording for playback');
    console.log('  startUIPlayback() - Start playback');
    console.log('  pauseUIPlayback() - Pause playback');
    console.log('  resumeUIPlayback() - Resume playback');
    console.log('  stopUIPlayback() - Stop playback');
    console.log('  setUIPlaybackSpeed(speed) - Set playback speed (1.0 = normal)');
    console.log('  getUIPlaybackStatus() - Get playback status');
    console.log('');
    console.log('Example usage:');
    console.log('  // Start recording');
    console.log('  startUIRecording({ name: "My Test" });');
    console.log('  // ... interact with UI elements ...');
    console.log('  // Stop and export');
    console.log('  const recording = stopUIRecording();');
    console.log('  const json = exportUIRecording();');
    console.log('  // Later, load and playback');
    console.log('  loadUIRecordingForPlayback(recording);');
    console.log('  startUIPlayback();');
}

// Make test function available globally
if (typeof window !== 'undefined') {
    window.runUIRecorderTests = runUIRecorderTests;
    window.demonstrateUIRecording = demonstrateUIRecording;
}
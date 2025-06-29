/**
 * AudioWorklet Processor for FastLED Audio Processing
 * Runs on the audio thread for better performance and consistency
 */
class FastLEDAudioProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.sampleBuffer = new Int16Array(512); // Match AUDIO_SAMPLE_BLOCK_SIZE
    this.isProcessing = false;
    this.debugCounter = 0;

    console.log('🎵 FastLEDAudioProcessor: 🚀 Constructor called, worklet initialized');

    // Listen for messages from main thread
    console.log('🎵 FastLEDAudioProcessor: Setting up message handler');
    this.port.onmessage = (event) => {
      const { type, data } = event.data;

      switch (type) {
        case 'start':
          this.isProcessing = true;
          console.log('🎵 FastLEDAudioProcessor: ▶️ Processing started');
          break;
        case 'stop':
          this.isProcessing = false;
          console.log('🎵 FastLEDAudioProcessor: ⏸️ Processing stopped');
          break;
        case 'config':
          // Future: handle configuration changes
          console.log('🎵 FastLEDAudioProcessor: Config message received');
          break;
        default:
          console.log(`🎵 FastLEDAudioProcessor: Unknown message type: ${type}`);
      }
    };
  }

  process(inputs, outputs, parameters) {
    // Debug logging (every 500 calls for more visibility)
    this.debugCounter++;
    if (this.debugCounter % 500 === 0) {
      console.log(
        `🎵 FastLEDAudioProcessor: 🔄 process() called ${this.debugCounter} times, isProcessing: ${this.isProcessing}, inputs.length: ${inputs.length}`,
      );
    }

    // Only process if we have input audio and processing is enabled
    if (!this.isProcessing || inputs.length === 0 || inputs[0].length === 0) {
      if (this.debugCounter % 1000 === 0 && this.debugCounter > 0) {
        console.log(
          `🎵 FastLEDAudioProcessor: ⏭️ Skipping processing - isProcessing: ${this.isProcessing}, inputs: ${inputs.length}`,
        );
      }
      return true;
    }

    const input = inputs[0];
    const inputChannel = input[0]; // Left channel

    if (inputChannel && inputChannel.length === 512) {
      // Convert float32 audio data to int16 format
      for (let i = 0; i < inputChannel.length; i++) {
        // Convert from float32 (-1.0 to 1.0) to int16 range (-32768 to 32767)
        this.sampleBuffer[i] = Math.floor(inputChannel[i] * 32767);
      }

      // Get high-resolution timestamp from AudioContext (fixed: use this.currentTime)
      const timestamp = Math.floor(this.currentTime * 1000);

      // Debug: Log sample data more frequently
      if (this.debugCounter % 500 === 0) {
        const avgSample = this.sampleBuffer.slice(0, 10).reduce((a, b) => a + Math.abs(b), 0) / 10;
        console.log(
          `🎵 FastLEDAudioProcessor: 📤 Sending audio data, timestamp: ${timestamp}ms, avg sample magnitude: ${
            avgSample.toFixed(1)
          }`,
        );
      }

      // Send audio data to main thread
      this.port.postMessage({
        type: 'audioData',
        samples: this.sampleBuffer.slice(), // Copy the buffer
        timestamp: timestamp,
      });
    } else if (this.debugCounter % 500 === 0) {
      console.log(
        `🎵 FastLEDAudioProcessor: ❌ Invalid input channel - length: ${
          inputChannel ? inputChannel.length : 'null'
        }`,
      );
    }

    // Copy input to output for passthrough
    if (outputs.length > 0 && outputs[0].length > 0) {
      const output = outputs[0];
      for (let channel = 0; channel < Math.min(input.length, output.length); channel++) {
        if (input[channel] && output[channel]) {
          output[channel].set(input[channel]);
        }
      }
    }

    return true; // Keep processor alive
  }
}

// Register the processor
console.log('🎵 FastLEDAudioProcessor: 📝 Registering worklet processor');
registerProcessor('fastled-audio-processor', FastLEDAudioProcessor);
console.log('🎵 FastLEDAudioProcessor: ✅ Worklet processor registered successfully');

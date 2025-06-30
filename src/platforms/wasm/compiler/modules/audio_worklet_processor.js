/**
 * @fileoverview FastLED Audio Worklet Processor
 * Processes audio input for LED visualization
 */

/**
 * @extends AudioWorkletProcessor
 */
class FastLEDAudioProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.bufferSize = 512;
    this.sampleRate = 44100;
    this.initialized = false;

    /**
     * @param {MessageEvent} event - Message event from main thread
     */
    this.port.onmessage = (event) => {
      const { type, data } = event.data;

      switch (type) {
        case 'init':
          this.sampleRate = data.sampleRate || 44100;
          this.bufferSize = data.bufferSize || 512;
          this.initialized = true;
          break;
        case 'config':
          // Handle configuration updates
          if (data.bufferSize) this.bufferSize = data.bufferSize;
          if (data.sampleRate) this.sampleRate = data.sampleRate;
          break;
        default:
          console.log('Unknown message type:', type);
      }
    };
  }

  /**
   * Process audio data
   * @param {Float32Array[][]} inputs - Input audio data
   * @param {Float32Array[][]} outputs - Output audio data
   * @param {Record<string, Float32Array>} parameters - Audio parameters
   * @returns {boolean} - Whether to continue processing
   */
  process(inputs, outputs, parameters) {
    // Process input audio
    const input = inputs[0];
    if (!input || input.length === 0) {
      return true; // Continue processing even without input
    }

    const inputChannel = input[0];
    if (!inputChannel || inputChannel.length === 0) {
      return true;
    }

    // Calculate RMS and other audio features
    let sum = 0;
    for (let i = 0; i < inputChannel.length; i++) {
      sum += inputChannel[i] * inputChannel[i];
    }
    const rms = Math.sqrt(sum / inputChannel.length);

    // Find peak
    let peak = 0;
    for (let i = 0; i < inputChannel.length; i++) {
      const abs = Math.abs(inputChannel[i]);
      if (abs > peak) peak = abs;
    }

    // Create audio data packet
    if (this.initialized && (rms > 0.001 || peak > 0.001)) {
      const timestamp = Math.floor(this.currentTime * 1000);

      // Create sample data - convert to Int16 for efficiency
      const samples = new Int16Array(inputChannel.length);
      for (let i = 0; i < inputChannel.length; i++) {
        samples[i] = Math.round(inputChannel[i] * 32767);
      }

      // Send processed audio data to main thread
      this.port.postMessage({
        type: 'audioData',
        data: {
          timestamp,
          samples: samples,
          rms,
          peak,
          sampleRate: this.sampleRate,
          bufferSize: inputChannel.length,
        },
      });
    }

    // Pass through to output (optional)
    if (outputs.length > 0 && outputs[0].length > 0) {
      const output = outputs[0][0];
      if (output && inputChannel) {
        for (let i = 0; i < Math.min(output.length, inputChannel.length); i++) {
          output[i] = inputChannel[i];
        }
      }
    }

    return true; // Continue processing
  }
}

// Register the processor
console.log('🎵 FastLEDAudioProcessor: 📝 Registering worklet processor');
registerProcessor('fastled-audio-processor', FastLEDAudioProcessor);
console.log('🎵 FastLEDAudioProcessor: ✅ Worklet processor registered successfully');

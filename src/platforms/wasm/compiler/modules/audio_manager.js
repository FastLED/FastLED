/**
 * AudioManager class for handling audio file uploads and analysis
 */
export class AudioManager {
  constructor(options = {}) {
    // Default options
    this.options = {
      uploadButtonId: 'upload_mp3',
      fileInputId: 'file_input',
      playerId: 'player',
      canvasLabelId: 'canvas-label',
      outputId: 'output',
      fftSize: 2048,
      ...options
    };
    
    // Initialize elements
    this.uploadBtn = document.getElementById(this.options.uploadButtonId);
    this.fileInput = document.getElementById(this.options.fileInputId);
    this.player = document.getElementById(this.options.playerId);
    
    // Bind methods to this instance
    this.handleUploadClick = this.handleUploadClick.bind(this);
    this.handleFileChange = this.handleFileChange.bind(this);
    
    // Set up event listeners
    this.setupEventListeners();
  }
  
  /**
   * Set up event listeners for audio controls
   */
  setupEventListeners() {
    if (this.uploadBtn) {
      this.uploadBtn.addEventListener('click', this.handleUploadClick);
    }
    
    if (this.fileInput) {
      this.fileInput.addEventListener('change', this.handleFileChange);
    }
  }
  
  /**
   * Handle upload button click
   */
  handleUploadClick() {
    this.fileInput.click();
  }
  
  /**
   * Handle file selection
   * @param {Event} e - The change event
   */
  handleFileChange(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    // Create a temporary URL & set it on the audio element
    const url = URL.createObjectURL(file);
    this.player.src = url;
    this.player.style.display = 'block';  // show the player
    
    // Update button text to show the file name
    this.uploadBtn.textContent = file.name.length > 20 
      ? file.name.substring(0, 17) + '...' 
      : file.name;
    
    // Set up audio analysis
    this.setupAudioAnalysis(this.player);
    
    // Start playback
    this.player.play();
  }
  
  /**
   * Set up audio analysis to process and display audio data
   * @param {HTMLAudioElement} audioElement - The audio element to analyze
   */
  setupAudioAnalysis(audioElement) {
    // Create audio context
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    this.audioContext = new AudioContext();
    
    // Create analyzer
    this.analyzer = this.audioContext.createAnalyser();
    this.analyzer.fftSize = this.options.fftSize;
    
    // Connect audio element to analyzer
    this.source = this.audioContext.createMediaElementSource(audioElement);
    this.source.connect(this.analyzer);
    this.analyzer.connect(this.audioContext.destination);
    
    // Buffer for frequency data
    this.bufferLength = this.analyzer.frequencyBinCount;
    this.dataArray = new Uint8Array(this.bufferLength);
    
    // Start updating the data
    this.updateSampleData();
  }
  
  /**
   * Update and display sample data
   */
  updateSampleData() {
    // Get frequency data
    this.analyzer.getByteFrequencyData(this.dataArray);
    
    // Calculate average amplitude
    let sum = 0;
    let nonZeroCount = 0;
    
    for (let i = 0; i < this.bufferLength; i++) {
      if (this.dataArray[i] > 0) {
        sum += this.dataArray[i];
        nonZeroCount++;
      }
    }
    
    // Only update if we have data and audio is playing
    if (nonZeroCount > 0 && !this.player.paused) {
      const average = Math.round(sum / nonZeroCount);
      const timestamp = (this.player.currentTime).toFixed(2);
      
      // Add to output
      const message = `[${timestamp}s] Audio sample size: ${this.bufferLength}, Average amplitude: ${average}`;
      
      // Update the canvas label to show the current audio data
      const label = document.getElementById(this.options.canvasLabelId);
      if (label) {
        label.textContent = `Audio: ${average}`;
        
        // Make sure the label is visible
        if (!label.classList.contains('show-animation')) {
          label.classList.add('show-animation');
        }
      }
      
      // Also log to console for debugging
      console.log(message);
    }
    
    // Continue updating
    requestAnimationFrame(() => this.updateSampleData());
  }
  
  /**
   * Clean up resources when no longer needed
   */
  destroy() {
    // Remove event listeners
    if (this.uploadBtn) {
      this.uploadBtn.removeEventListener('click', this.handleUploadClick);
    }
    
    if (this.fileInput) {
      this.fileInput.removeEventListener('change', this.handleFileChange);
    }
    
    // Close audio context if it exists
    if (this.audioContext && this.audioContext.state !== 'closed') {
      this.audioContext.close();
    }
  }
}

// Export a function to create and initialize the audio manager
export function initAudioManager(options = {}) {
  return new AudioManager(options);
}

void setup() {
    // Initialize serial communication at 9600 baud rate
    Serial.begin(9600);
    
    // Wait for serial port to connect (needed for some boards)
    while (!Serial) {
        ; // wait for serial port to connect
    }
    
    // Print Hello World message
    Serial.println("Hello World!");
}

void loop() {
    // Print Hello World every 2 seconds
    Serial.println("Hello World!");
    delay(2000);
}
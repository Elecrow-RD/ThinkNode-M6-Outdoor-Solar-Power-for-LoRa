/*******************Light Sensor*******************/
#include "Arduino.h"
#include <Adafruit_TinyUSB.h>
#include <BH1750.h>            // Include BH1750 light sensor library
BH1750 lightMeter(0x5C);      // Create sensor object with I2C address 0x51
float lux;                    // Stores measured light intensity in lux
#define SDA 41
#define SCL 8

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
  Wire.setPins(SDA, SCL);

  // Initialize I2C communication (required for BH1750)
  Wire.begin();

  // Initialize BH1750 sensor in continuous high-resolution mode (1 lux precision)
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire)) {
    Serial.println("BH1750 Advanced begin");  // Success message
  } else {
    Serial.println("Error initialising BH1750");  // Error message
  }

  // Configure LED pin as output
}

void loop() {
  // Check if light measurement is ready (blocking wait if true)
  if (lightMeter.measurementReady(true)) {
    lux = lightMeter.readLightLevel();  // Read light level in lux
    Serial.print("[-] Light: [");
    Serial.print(lux);
    Serial.println("] lx");             // Print light level to serial monitor
  }

  // Small delay to avoid excessive loop iterations
  delay(1000);
}

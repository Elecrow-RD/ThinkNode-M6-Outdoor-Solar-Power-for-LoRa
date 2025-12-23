/*
  RadioLib LoRaWAN End Device Reference Example

  This example joins a LoRaWAN network and will send
  uplink packets. Before you start, you will have to
  register your device at https://www.thethingsnetwork.org/
  After your device is registered, you can run this example.
  The device will join the network and start uploading data.

  Also, most of the possible and available functions are
  shown here for reference.

  LoRaWAN v1.0.4/v1.1 requires the use of EEPROM (persistent storage).
  Running this examples REQUIRES you to check "Resets DevNonces"
  on your LoRaWAN dashboard. Refer to the notes or the
  network's documentation on how to do this.
  To comply with LoRaWAN's persistent storage, refer to
  https://github.com/radiolib-org/radiolib-persistence

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/

  For LoRaWAN details, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/LoRaWAN

*/

#include "config.h"
#include <SPI.h>
#include "Arduino.h"
#include <Adafruit_TinyUSB.h>
#include <BH1750.h>            // Include BH1750 light sensor library
#include <SoftwareSerial.h>

BH1750 lightMeter(0x5C);

uint32_t lux;                    // Stores measured light intensity in lux
#define SDA 41
#define SCL 8
#define Blue_LED 7
#define Red_LED 12

/*SERIAL2*/
#define PIN_RX 22
#define PIN_TX 24

SoftwareSerial mySerial(PIN_RX, PIN_TX);

static char g_rxBuffer[64];
static int g_rxIndex = 0;

float humidity = 0.0f;
float temperature = 0.0f;

void setup() {
  Serial.begin(9600);
  pinMode(Blue_LED, OUTPUT);
  pinMode(Red_LED, OUTPUT);
  digitalWrite(Red_LED, HIGH);
  digitalWrite(Blue_LED, LOW);
  while (!Serial); // Wait for serial to be initialised
  mySerial.begin(9600);
  delay(1000);
  mySerial.println("Hand");
  unsigned long startTime = millis();
  bool okReceived = false;
  char responseBuffer[4] = {0}; // Buffer for "OK" + null terminator
  int responseIndex = 0;
  const unsigned long timeout = 1000; // 1 second timeout
  while ((millis() - startTime < timeout) && !okReceived) {
    if (mySerial.available()) {
      char c = mySerial.read();
      // Build response string character by character
      if (responseIndex < 2) { // Only need first 2 chars for "OK"
        responseBuffer[responseIndex++] = c;
        responseBuffer[responseIndex] = '\0'; // Null terminate

        // Check if we received "OK"
        if (strcmp(responseBuffer, "OK") == 0) {
          okReceived = true;
          Serial.println(F("✓ Received 'OK' response"));
          break;
        }
      }
    }
  }
  if (!okReceived) {
    Serial.println(F("✗ Warning: Did not receive 'OK' response within timeout"));
    // Optional: you can choose to halt here or continue
    // while(1) { delay(1000); } // Uncomment to halt on error
  } else {
    // Clear mySerial buffer after successful verification
    Serial.println(F("Clearing mySerial buffer..."));
    while (mySerial.available()) {
      char c = mySerial.read(); // Read and discard any remaining characters
      Serial.print(F("Discarded char: "));
      Serial.println(c);
    }
    Serial.println(F("✓ Buffer cleared"));
    delay(100); // Small delay after handshake for device to stabilize
  }
  mySerial.println("Read");
  Serial.println(F("\nSetup"));
  Wire.setPins(SDA, SCL);

  // Initialize I2C communication (required for BH1750)
  Wire.begin();

  // Initialize BH1750 sensor in continuous high-resolution mode (1 lux precision)
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire)) {
    Serial.println("BH1750 Advanced begin");  // Success message
  } else {
    Serial.println("Error initialising BH1750");  // Error message
  }

  int16_t state = 0;  // return value for calls to RadioLib

  Serial.println(F("Initialise the radio"));
  SPI.setPins(47, 45, 46);
  SPI.begin();
  state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, F("Initialise radio failed"), state, true);

  // Override the default join rate
  uint8_t joinDR = 2;

  // Setup the OTAA session information
  node.beginOTAA(joinEUI, devEUI, NULL, appKey);

  Serial.println(F("Join ('login') the LoRaWAN Network"));
  state = node.activateOTAA(joinDR);
  debug(state != RADIOLIB_LORAWAN_NEW_SESSION, F("Join failed"), state, true);

  // Print the DevAddr
  Serial.print("[LoRaWAN] DevAddr: ");
  Serial.println((unsigned long)node.getDevAddr(), HEX);

  // Enable the ADR algorithm (on by default which is preferable)
  node.setADR(true);

  // Set a datarate to start off with
  node.setDatarate(5);

  // Manages uplink intervals to the TTN Fair Use Policy
  node.setDutyCycle(true, 1250);

  // Enable the dwell time limits - 400ms is the limit for the US
  node.setDwellTime(true, 400);

  Serial.println(F("Ready!\n"));

}

void loop() {
  int16_t state = RADIOLIB_ERR_NONE;

  if (lightMeter.measurementReady(true)) {
    lux = lightMeter.readLightLevel();  // Read light level in lux
    Serial.print("[-] Light: [");
    Serial.print(lux);
    Serial.println("] lx");             // Print light level to serial monitor
  }
  mySerial.println("Read");
  while (mySerial.available()) {
    char c = mySerial.read();
    if (c == '\n' || c == '\r' || g_rxIndex >= sizeof(g_rxBuffer) - 1) {
      g_rxBuffer[g_rxIndex] = '\0';
      if (g_rxIndex > 0) {
        parseSensorData(g_rxBuffer);
        g_rxIndex = 0;
      }
    } else {
      g_rxBuffer[g_rxIndex++] = c;
    }
  }
  uint16_t humidityInt = (uint16_t)(humidity * 10.0f + 0.5f);
  uint16_t tempInt = (uint16_t)(temperature * 10.0f + 0.5f);
  // Build payload byte array
  uint8_t uplinkPayload[6];
  uplinkPayload[0] = lux & 0xFF;
  uplinkPayload[1] = (lux >> 8) & 0xFF;
  uplinkPayload[2] = humidityInt & 0xFF;
  uplinkPayload[3] = (humidityInt >> 8) & 0xFF;
  uplinkPayload[4] = tempInt & 0xFF;
  uplinkPayload[5] = (tempInt >> 8) & 0xFF;


  uint8_t downlinkPayload[10];  // Make sure this fits your plans!
  size_t  downlinkSize;         // To hold the actual payload size received

  // you can also retrieve additional information about an uplink or
  // downlink by passing a reference to LoRaWANEvent_t structure
  LoRaWANEvent_t uplinkDetails;
  LoRaWANEvent_t downlinkDetails;

  uint8_t fPort = 10;

  // Retrieve the last uplink frame counter
  uint32_t fCntUp = node.getFCntUp();

  // Send a confirmed uplink on the second uplink
  // and also request the LinkCheck and DeviceTime MAC commands
  Serial.println(F("Sending uplink"));
  digitalWrite(Blue_LED, HIGH);
  if (fCntUp == 1) {
    Serial.println(F("and requesting LinkCheck and DeviceTime"));
    node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_LINK_CHECK);
    node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
    state = node.sendReceive(uplinkPayload, sizeof(uplinkPayload), fPort, downlinkPayload, &downlinkSize, true, &uplinkDetails, &downlinkDetails);
  } else {
    state = node.sendReceive(uplinkPayload, sizeof(uplinkPayload), fPort, downlinkPayload, &downlinkSize, false, &uplinkDetails, &downlinkDetails);
  }
  digitalWrite(Blue_LED, LOW);
  debug(state < RADIOLIB_ERR_NONE, F("Error in sendReceive"), state, false);

  // Check if a downlink was received
  // (state 0 = no downlink, state 1/2 = downlink in window Rx1/Rx2)
  if (state > 0) {
    Serial.println(F("Received a downlink"));
    // Did we get a downlink with data for us
    if (downlinkSize > 0) {
      Serial.println(F("Downlink data: "));
      arrayDump(downlinkPayload, downlinkSize);
    } else {
      Serial.println(F("<MAC commands only>"));
    }

    // print RSSI (Received Signal Strength Indicator)
    Serial.print(F("[LoRaWAN] RSSI:\t\t"));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // print SNR (Signal-to-Noise Ratio)
    Serial.print(F("[LoRaWAN] SNR:\t\t"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    // print extra information about the event
    Serial.println(F("[LoRaWAN] Event information:"));
    Serial.print(F("[LoRaWAN] Confirmed:\t"));
    Serial.println(downlinkDetails.confirmed);
    Serial.print(F("[LoRaWAN] Confirming:\t"));
    Serial.println(downlinkDetails.confirming);
    Serial.print(F("[LoRaWAN] Datarate:\t"));
    Serial.println(downlinkDetails.datarate);
    Serial.print(F("[LoRaWAN] Frequency:\t"));
    Serial.print(downlinkDetails.freq, 3);
    Serial.println(F(" MHz"));
    Serial.print(F("[LoRaWAN] Frame count:\t"));
    Serial.println(downlinkDetails.fCnt);
    Serial.print(F("[LoRaWAN] Port:\t\t"));
    Serial.println(downlinkDetails.fPort);
    Serial.print(F("[LoRaWAN] Time-on-air: \t"));
    Serial.print(node.getLastToA());
    Serial.println(F(" ms"));
    Serial.print(F("[LoRaWAN] Rx window: \t"));
    Serial.println(state);

    uint8_t margin = 0;
    uint8_t gwCnt = 0;
    if (node.getMacLinkCheckAns(&margin, &gwCnt) == RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRaWAN] LinkCheck margin:\t"));
      Serial.println(margin);
      Serial.print(F("[LoRaWAN] LinkCheck count:\t"));
      Serial.println(gwCnt);
    }

    uint32_t networkTime = 0;
    uint8_t fracSecond = 0;
    if (node.getMacDeviceTimeAns(&networkTime, &fracSecond, true) == RADIOLIB_ERR_NONE) {
      Serial.print(F("[LoRaWAN] DeviceTime Unix:\t"));
      Serial.println(networkTime);
      Serial.print(F("[LoRaWAN] DeviceTime second:\t1/"));
      Serial.println(fracSecond);
    }

  } else {
    Serial.println(F("[LoRaWAN] No downlink received"));
  }

  // wait before sending another packet
  uint32_t minimumDelay = uplinkIntervalSeconds * 1000UL;
  uint32_t interval = node.timeUntilUplink();     // calculate minimum duty cycle delay (per FUP & law!)
  uint32_t delayMs = max(interval, minimumDelay); // cannot send faster than duty cycle allows

  Serial.print(F("[LoRaWAN] Next uplink in "));
  Serial.print(delayMs / 1000);
  Serial.println(F(" seconds\n"));

  delay(delayMs);
}


void parseSensorData(const char* data)
{
  if (strlen(data) < 15 || data[0] != 'R' || data[1] != ':') {
    Serial.print(F("Frame error: "));
    Serial.println(data);
    return;
  }

  char humidityStr[6] = {0};
  memcpy(humidityStr, &data[2], 5);

  char tempStr[6] = {0};
  memcpy(tempStr, &data[10], 5);

  humidity = atof(humidityStr);
  temperature = atof(tempStr);

  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F(" %RH, Temperature: "));
  Serial.print(temperature);
  Serial.println(F(" °C"));
}

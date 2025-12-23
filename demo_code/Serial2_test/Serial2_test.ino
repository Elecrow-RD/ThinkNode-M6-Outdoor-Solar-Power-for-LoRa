#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <SoftwareSerial.h>

#define PIN_RX 22
#define PIN_TX 24

SoftwareSerial mySerial(PIN_RX, PIN_TX);

static char g_rxBuffer[64];
static int g_rxIndex = 0;

void setup()
{
  Serial.begin(9600);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  mySerial.begin(9600);
  delay(100);
  mySerial.println("Hand");
  delay(100);
}

void loop() // run over and over//
{

  static unsigned long g_lastSendTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - g_lastSendTime >= 1000) {
    g_lastSendTime = currentTime;
    mySerial.println("Read");
  }

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

  float humidity = atof(humidityStr);
  float temperature = atof(tempStr);

  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F(" %RH, Temperature: "));
  Serial.print(temperature);
  Serial.println(F(" °C"));
}

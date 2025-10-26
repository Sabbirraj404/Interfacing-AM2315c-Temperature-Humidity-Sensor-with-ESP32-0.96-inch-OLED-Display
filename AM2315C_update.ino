#include <Wire.h>
#include <WiFi.h>
#include "ThingSpeak.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I2C pins and AM2315C address
#define AM2315_ADDR 0x38
#define I2C_SDA 21
#define I2C_SCL 22

// WiFi and ThingSpeakGlobe.packing
const char* ssid = "Globe.packing";
const char* password = "Www.globe#";
WiFiClient client;
#define CHANNEL_ID 3010443
#define CHANNEL_API_KEY "ZIA27DGSWHLHPBJU"

// Compute CRC-8 for AM2315C
uint8_t computeCRC8(uint8_t *data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }
  return crc;
}

// Updated Display Function to match image layout
void displayStatus(float temperature, float humidity, String wifiStatus, String uploadStatus) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ==== Left Column: WiFi Status ====
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Wifi");
  display.setCursor(0, 12);
  display.println(wifiStatus);  // "ok" or "X"

  // ==== Right Column: ThingSpeak Status ====
  display.setCursor(110, 0);
  display.println("TS");
  display.setCursor(112, 12);
  display.println(uploadStatus);  // "✔" or "X"

  // ==== Center: Temperature ====
  display.setTextSize(1);
  display.setCursor(32, 0);
  display.println("Temperature");

  display.setTextSize(2);
  String tempStr = isnan(temperature) ? "--.- C" : String(temperature, 1) + " C";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 16);
  display.println(tempStr);

  // ==== Center: Humidity ====
  display.setTextSize(1);
  display.setCursor(40, 38);
  display.println("Humidity");

  display.setTextSize(2);
  String humStr = isnan(humidity) ? "--.-%" : String(humidity, 1) + "%";
  display.getTextBounds(humStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.println(humStr);

  display.display();
}

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  // WiFi Connect
  WiFi.begin(ssid, password);
  display.println("Connecting WiFi...");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  display.println("WiFi Connected");
  display.display();
  Serial.println("WiFi Connected");

  // ThingSpeak Init
  ThingSpeak.begin(client);
}

void loop() {
  float temperature = NAN;
  float humidity = NAN;

  // Trigger sensor
  Wire.beginTransmission(AM2315_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);

  // Read response
  Wire.requestFrom(AM2315_ADDR, 7);
  if (Wire.available() == 7) {
    uint8_t buffer[7];
    for (int i = 0; i < 7; i++) buffer[i] = Wire.read();

    if (computeCRC8(buffer, 6) != buffer[6]) {
      Serial.println("CRC failed");
    } else {
      uint32_t rawHum = ((uint32_t)buffer[1] << 12) |
                        ((uint32_t)buffer[2] << 4) |
                        ((buffer[3] & 0xF0) >> 4);
      uint32_t rawTemp = ((uint32_t)(buffer[3] & 0x0F) << 16) |
                         ((uint32_t)buffer[4] << 8) |
                         buffer[5];

      humidity = (rawHum / 1048576.0) * 100.0;
      temperature = (rawTemp / 1048576.0) * 200.0 - 50.0;

      Serial.printf("Humidity: %.2f %%\tTemperature: %.2f C\n", humidity, temperature);
    }
  } else {
    Serial.println("Sensor not responding.");
  }

  // Send to ThingSpeak
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  int httpCode = ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);

  String wifiStat = (WiFi.status() == WL_CONNECTED) ? "ok" : "X";
  String uploadStat = (httpCode == 200) ? "" : "X";

  // Update OLED
  displayStatus(temperature, humidity, wifiStat, uploadStat);

  delay(15000);  // Wait 15 seconds between updates
}

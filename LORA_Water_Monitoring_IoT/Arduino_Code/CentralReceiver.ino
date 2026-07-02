/*
  ==========================================================================
  AquaLoRa - Central Receiver / Gateway Firmware
  --------------------------------------------------------------------------
  Runs on a single ESP32 that acts as the central LoRa receiver for ALL
  tank nodes.

  Responsibilities:
    1. Listen for incoming LoRa packets from every tank node.
    2. Parse the packet: TANK:<id>,TDS:<ppm>,LEVEL:<percent>,PUMP:<ON/OFF>
    3. Connect to WiFi and forward each tank's readings to ThingSpeak
       (one ThingSpeak channel per tank is recommended, or use separate
       fields on a single channel for a small number of tanks).

  Libraries required:
    - "LoRa" by Sandeep Mistry
    - "ThingSpeak" by MathWorks (install via Library Manager)
  ==========================================================================
*/

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include "ThingSpeak.h"

// ------------------------- CONFIGURATION --------------------------------
// WiFi credentials
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ThingSpeak configuration
unsigned long THINGSPEAK_CHANNEL_ID = 0000000;          // your channel ID
const char* THINGSPEAK_WRITE_API_KEY = "YOUR_WRITE_API_KEY";

// ThingSpeak field mapping (adjust per your channel setup)
// Field1 = Tank 1 Level, Field2 = Tank 1 TDS
// Field3 = Tank 2 Level, Field4 = Tank 2 TDS   ... extend as needed

// LoRa module (SX1278) pins - default VSPI wiring on ESP32
#define LORA_SS               15
#define LORA_RST              14
#define LORA_DIO0             2
#define LORA_FREQUENCY        433E6   // 433 MHz

// ------------------------- GLOBAL STATE ----------------------------------
WiFiClient client;

struct TankReading {
  int   tankId;
  float tdsPpm;
  float levelPercent;
  bool  pumpOn;
  bool  valid;
};

// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  connectToWiFi();

  ThingSpeak.begin(client);

  Serial.println("Initializing LoRa receiver...");
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed! Check wiring.");
    while (true) { delay(1000); }
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();

  Serial.println("Central Receiver ready. Listening for tank packets...");
}

// --------------------------------------------------------------------------
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    int rssi = LoRa.packetRssi();
    Serial.println("Received: " + incoming + "  (RSSI: " + String(rssi) + ")");

    TankReading reading = parsePacket(incoming);
    if (reading.valid) {
      uploadToThingSpeak(reading);
    } else {
      Serial.println("Warning: malformed packet, discarding.");
    }
  }

  // Reconnect WiFi if dropped
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
}

// --------------------------------------------------------------------------
void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed, will retry in loop().");
  }
}

// --------------------------------------------------------------------------
// Parses packets of the form:
// TANK:<id>,TDS:<ppm>,LEVEL:<percent>,PUMP:<ON/OFF>
TankReading parsePacket(const String &data) {
  TankReading r = {0, 0, 0, false, false};

  int tankIdx  = data.indexOf("TANK:");
  int tdsIdx   = data.indexOf(",TDS:");
  int lvlIdx   = data.indexOf(",LEVEL:");
  int pumpIdx  = data.indexOf(",PUMP:");

  if (tankIdx == -1 || tdsIdx == -1 || lvlIdx == -1 || pumpIdx == -1) {
    return r; // invalid = valid stays false
  }

  r.tankId       = data.substring(tankIdx + 5, tdsIdx).toInt();
  r.tdsPpm       = data.substring(tdsIdx + 5, lvlIdx).toFloat();
  r.levelPercent = data.substring(lvlIdx + 7, pumpIdx).toFloat();
  r.pumpOn       = data.substring(pumpIdx + 6) == "ON";
  r.valid        = true;

  return r;
}

// --------------------------------------------------------------------------
// Uploads a tank's reading to ThingSpeak. Field mapping assumes two fields
// per tank (level, TDS) - extend the switch statement for more tanks.
void uploadToThingSpeak(const TankReading &r) {
  switch (r.tankId) {
    case 1:
      ThingSpeak.setField(1, r.levelPercent);
      ThingSpeak.setField(2, r.tdsPpm);
      break;
    case 2:
      ThingSpeak.setField(3, r.levelPercent);
      ThingSpeak.setField(4, r.tdsPpm);
      break;
    default:
      Serial.println("Unmapped tank ID: " + String(r.tankId));
      return;
  }

  int status = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);

  if (status == 200) {
    Serial.println("ThingSpeak update successful for Tank " + String(r.tankId));
  } else {
    Serial.println("ThingSpeak update failed. HTTP error: " + String(status));
  }
}

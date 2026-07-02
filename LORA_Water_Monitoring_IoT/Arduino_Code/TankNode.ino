/*
  ==========================================================================
  AquaLoRa - Tank Node Firmware
  --------------------------------------------------------------------------
  Runs on the ESP32 attached to EACH water tank.

  Responsibilities:
    1. Measure water level using an HC-SR04 ultrasonic sensor.
    2. Measure water quality (Total Dissolved Solids) using a TDS sensor.
    3. Automatically control a pump via a relay based on water level.
    4. Package readings as "[TDS_ppm,WaterLevel_percent]" and transmit
       them over a 433 MHz LoRa link to the Central Receiver node.

  Library required: "LoRa" by Sandeep Mistry (install via Library Manager)

  IMPORTANT: Set a unique TANK_ID for every tank node you flash, so the
  central receiver / cloud dashboard can distinguish between tanks.
  ==========================================================================
*/

#include <SPI.h>
#include <LoRa.h>

// ------------------------- CONFIGURATION --------------------------------
#define TANK_ID              1        // Unique ID per tank (1, 2, 3, ...)

// HC-SR04 ultrasonic sensor pins
#define TRIG_PIN             5
#define ECHO_PIN             18

// TDS sensor analog input pin
#define TDS_PIN              34

// Relay module pin (controls the pump)
#define RELAY_PIN            26

// LoRa module (SX1278) pins - default VSPI wiring on ESP32
#define LORA_SS               15
#define LORA_RST              14
#define LORA_DIO0             2
#define LORA_FREQUENCY        433E6   // 433 MHz

// Tank physical dimensions (cm) - calibrate for your tank
#define TANK_HEIGHT_CM        100.0   // distance from sensor to empty-tank bottom
#define TANK_EMPTY_OFFSET_CM  5.0     // distance reading when tank is 100% full

// Water level thresholds for automated pump control
#define LEVEL_LOW_THRESHOLD    25.0   // % - pump turns ON below this
#define LEVEL_HIGH_THRESHOLD   25.0   // % - pump turns OFF above this

// TDS sensor calibration
#define VREF                  3.3     // ESP32 ADC reference voltage
#define ADC_RESOLUTION        4095.0  // 12-bit ADC

// Timing
#define SEND_INTERVAL_MS      5000    // how often to sample & transmit

// ------------------------- GLOBAL STATE ----------------------------------
unsigned long lastSendTime = 0;
bool pumpOn = false;

// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // pump OFF initially

  Serial.println("Initializing LoRa transmitter...");
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed! Check wiring.");
    while (true) { delay(1000); }
  }

  LoRa.setSpreadingFactor(9);      // balance of range vs. speed
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();

  Serial.println("Tank Node ready. Tank ID: " + String(TANK_ID));
}

// --------------------------------------------------------------------------
void loop() {
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    float waterLevelPercent = readWaterLevelPercent();
    float tdsPpm = readTdsPpm();

    controlPump(waterLevelPercent);

    sendPacket(tdsPpm, waterLevelPercent);

    Serial.printf("Tank %d -> Level: %.2f%%  TDS: %.2f ppm  Pump: %s\n",
                  TANK_ID, waterLevelPercent, tdsPpm, pumpOn ? "ON" : "OFF");
  }
}

// --------------------------------------------------------------------------
// Reads distance from the HC-SR04 and converts it to a fill percentage.
float readWaterLevelPercent() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout (~5m range)
  if (duration == 0) {
    Serial.println("Warning: ultrasonic echo timeout.");
    return -1.0; // sensor error indicator
  }

  float distanceCm = duration * 0.0343 / 2.0;

  // Convert distance-to-water-surface into a fill percentage
  float usableRange = TANK_HEIGHT_CM - TANK_EMPTY_OFFSET_CM;
  float levelPercent = ((TANK_HEIGHT_CM - distanceCm) / usableRange) * 100.0;

  levelPercent = constrain(levelPercent, 0.0, 100.0);
  return levelPercent;
}

// --------------------------------------------------------------------------
// Reads the TDS sensor and converts the analog voltage into ppm.
float readTdsPpm() {
  int rawAdc = analogRead(TDS_PIN);
  float voltage = rawAdc * (VREF / ADC_RESOLUTION);

  // Standard TDS conversion formula (temperature-compensated, 25C assumed)
  float compensationCoefficient = 1.0; // adjust if using a temp sensor
  float compensatedVoltage = voltage / compensationCoefficient;

  float tdsValue = (133.42 * pow(compensatedVoltage, 3)
                    - 255.86 * pow(compensatedVoltage, 2)
                    + 857.39 * compensatedVoltage) * 0.5;

  return max(tdsValue, 0.0f);
}

// --------------------------------------------------------------------------
// Simple hysteresis-based pump automation.
void controlPump(float levelPercent) {
  if (levelPercent < 0) return; // ignore on sensor error

  if (levelPercent < LEVEL_LOW_THRESHOLD && !pumpOn) {
    pumpOn = true;
    digitalWrite(RELAY_PIN, HIGH);
  } else if (levelPercent >= LEVEL_HIGH_THRESHOLD && pumpOn) {
    pumpOn = false;
    digitalWrite(RELAY_PIN, LOW);
  }
}

// --------------------------------------------------------------------------
// Formats and sends the data packet over LoRa:
// TANK:<id>,TDS:<ppm>,LEVEL:<percent>,PUMP:<ON/OFF>
void sendPacket(float tdsPpm, float levelPercent) {
  String packet = "TANK:" + String(TANK_ID) +
                   ",TDS:" + String(tdsPpm, 2) +
                   ",LEVEL:" + String(levelPercent, 2) +
                   ",PUMP:" + String(pumpOn ? "ON" : "OFF");

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
}

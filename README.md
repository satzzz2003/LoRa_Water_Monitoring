# LoRa-powered water management system for real time monitoring and control in gated communities 

A low-power, long-range **LoRa-based water level and quality monitoring system** for smart cities, gated communities, and rural households. Each water tank is fitted with an ESP32 node that reads water level (ultrasonic sensor) and water quality (TDS sensor), then transmits the data over a 433 MHz LoRa link to a central receiver, which pushes it to the cloud (ThingSpeak) for real-time visualization and control via a mobile app (Blynk).



---

## Key Features

- **Multi-tank monitoring** — scales to any number of tanks without extra wiring or gateways.
- **Long-range, low-power communication** — LoRa at 433 MHz, up to ~15 km line-of-sight, with Adaptive Data Rate (ADR).
- **Water level sensing** — HC-SR04 ultrasonic sensor calculates tank fill percentage from echo return time.
- **Water quality sensing** — TDS (Total Dissolved Solids) sensor estimates dissolved solids concentration in ppm.
- **Automated pump control** — relay module switches the pump ON/OFF based on configurable level thresholds (e.g., ON below 25%, OFF above 25%).
- **Cloud storage & analytics** — sensor packets are streamed to ThingSpeak for trend graphs, gauges, and alerts.
- **Mobile app dashboard** — Blynk app shows live TDS ppm, water level %, and pump status, with remote control and notifications.
- **Collision-free multi-node transmission** — a toggle/round-robin mechanism lets each tank's LoRa transmitter send data one at a time.

---

## System Architecture

```
 ┌────────────┐        ┌────────────┐
 │  WATER TANK 1        │  WATER TANK 2      ...
 │  ┌────────┐          │  ┌────────┐
 │  │Ultrasonic│         │  │Ultrasonic│
 │  │  Sensor  │         │  │  Sensor  │
 │  └────┬────┘         │  └────┬────┘
 │  ┌────┴────┐         │  ┌────┴────┐
 │  │  TDS     │         │  │  TDS     │
 │  │ Sensor   │         │  │ Sensor   │
 │  └────┬────┘         │  └────┬────┘
 │     ESP32 (1)                ESP32 (2)
 │       │                        │
 │  LoRa Transmitter (1)     LoRa Transmitter (2)
 │       │                        │
 │  Relay → Pump (1)         Relay → Pump (2)
 └───────┴────────────┬───────────┘
                       ▼
              LoRa Central Receiver (433 MHz)
                       │
                     ESP32 (Gateway)
                       │
                 ThingSpeak Cloud
                       │
              Blynk Mobile App (Dashboard)
```

Each tank node packages its readings as:

```
[Turbidity/TDS (ppm), Water Level (%)]
```

and sends them over UART to the LoRa module, which transmits at **433 MHz**. The central receiver collects packets from all tanks and forwards them to the cloud — no additional gateway hardware required.

---

## Hardware Requirements

| Component | Quantity (per tank) | Purpose |
|---|---|---|
| ESP32 Dev Board | 1 | Main microcontroller |
| HC-SR04 Ultrasonic Sensor | 1 | Water level measurement |
| TDS Sensor Module | 1 | Water quality (dissolved solids) measurement |
| LoRa Module (SX1278, 433 MHz) | 1 | Long-range wireless transmission |
| 5V Relay Module | 1 | Pump ON/OFF automation |
| Water Pump | 1 | Fills/empties the tank |
| Additional ESP32 + LoRa Module | 1 (shared) | Central receiver / gateway |
| Power supply / battery | as needed | Field deployment |

---

## 📊 TDS Reference Table

| TDS Level (ppm) | Water Quality | Remarks |
|---|---|---|
| 0 – 150 | Excellent to Good | Ideal for drinking |
| 150 – 500 | Fair to Poor | May affect taste; higher levels indicate pollution |
| 500 – 1,000 | Very Poor | Not recommended for drinking |
| 1,000+ | Unsafe | Highly contaminated; unsafe for consumption |

---

## Software Setup

1. Install **Arduino IDE** (or PlatformIO).
2. Add the **ESP32 board package** via Boards Manager.
3. Install the following libraries via Library Manager:
   - `LoRa` by Sandeep Mistry
   - `WiFi` (bundled with ESP32 core)
   - `ThingSpeak` by MathWorks
4. Flash `src/TankNode/TankNode.ino` to each tank's ESP32 — update `TANK_ID` for each node.
5. Flash `src/CentralReceiver/CentralReceiver.ino` to the gateway ESP32 — update your WiFi credentials and ThingSpeak Write API Key.
6. Wire the sensors and LoRa module per the pin definitions at the top of each `.ino` file.
   Set up a **Blynk** project and link the ThingSpeak fields to your dashboard widgets for gauges, graphs, and pump control notifications.

---




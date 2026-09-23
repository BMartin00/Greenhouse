# Greenhouse
Software Development for Connected Devices Year 3 Semester 1 Project

# 🌱 Smart Greenhouse Control System

### ESP32 • Android App • MQTT • Touchscreen UI

*A complete IoT‑based environmental monitoring and automation system*

---

## 📌 Overview

This project implements a fully integrated **smart greenhouse control system** featuring:

- An **Android mobile application** for remote monitoring and control
- An **ESP32‑WROOM‑32 controller** managing sensors, actuators, and automation
- An **ESP32‑2432S028 CYD touchscreen interface** for local operation
- **MQTT communication** for remote control
- **Serial UART** communication between ESP32 units

The system monitors **temperature, humidity, soil moisture, and light level**, and controls a **fan**, **water pump**, and **UV grow light** using both **automatic rules** and **manual override**.

---

## 🚀 Features

### 🔍 Sensor Monitoring

- Temperature (DHT11)
- Humidity (DHT11)
- Soil Moisture (Capacitive sensor)
- Light Level (Photoresistor)

### ⚙️ Actuator Control

- Ventilation Fan (L293D motor driver)
- Water Pump (5V relay)
- UV Grow Light (GPIO + resistor)

### 🤖 Automation Logic

- Temperature‑based fan control with hysteresis
- Soil moisture‑based irrigation control
- Light‑based UV grow light activation
- Auto/Manual mode switching for all components

### 📱 Dual User Interfaces

- **Android App (Java + MQTT)**
- **ESP32 Touchscreen UI (ILI9341 + XPT2046)**
- Real‑time sensor data
- Manual override
- Mode switching
- Visual feedback (colors, icons, transparency)

### 🔗 Communication

- MQTT topics for sensor data, control commands, and status
- JSON‑formatted messages
- UART serial communication between ESP32 boards

---

# 🔧 Setup & Installation

This project contains **three components** that must be set up in order:

1. ESP32 Controller (Main greenhouse logic)
2. ESP32‑2432S028 CYD Touchscreen Interface
3. MQTT Broker
4. Android App

Follow the steps below.

---

## 1️⃣ ESP32 Controller Setup (Main Greenhouse Unit)

### **Hardware Required**

- ESP32‑WROOM‑32
- DHT11 sensor
- Capacitive soil moisture sensor
- Photoresistor
- L293D motor driver
- 5V relay
- UV LED + resistor
- Fan + water pump
- USB 5V power supply

### **Pin Layout**

| Component | Pin |
|---|---|
| DHT11 | GPIO4 |
| Soil Moisture | GPIO34 |
| Light Sensor | GPIO33 |
| Fan (L293D) | GPIO25 |
| Water Pump (Relay) | GPIO26 |
| UV Light | GPIO27 |
| UART TX | GPIO17 |
| UART RX | GPIO16 |

### **Steps**

1. Install **Arduino IDE**
2. Add ESP32 board support: [`https://dl.espressif.com/dl/package_esp32_index.json`](https://dl.espressif.com/dl/package_esp32_index.json)
3. Install libraries:
   - PubSubClient
   - ArduinoJson
   - DHT sensor library
4. Open the controller `.ino` file
5. Update WiFi + MQTT settings:
   ```
   const char* ssid = "YOUR_WIFI";
   const char* password = "YOUR_PASSWORD";
   const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
   ```
6. Wire all sensors and actuators
7. Upload the code
8. Open Serial Monitor to confirm MQTT connection

---

## 2️⃣ ESP32‑2432S028 CYD Touchscreen Setup

### **Hardware Required**

- ESP32‑2432S028 CYD
- ILI9341 TFT display
- XPT2046 touch controller

### **SPI Pins**

| Signal | Pin |
|---|---|
| MOSI | GPIO32 |
| MISO | GPIO39 |
| CLK | GPIO25 |
| CS (Display) | GPIO33 |
| Touch IRQ | GPIO36 |
| UART TX | GPIO27 |
| UART RX | GPIO22 |

### **Steps**

1. Install libraries:
   - TFT_eSPI
   - XPT2046_Touchscreen
   - ArduinoJson
2. Edit `User_Setup.h` in TFT_eSPI to match your pins
3. Set UART in code:
   ```
   Serial2.begin(9600, SERIAL_8N1, 22, 27);
   ```
4. Upload touchscreen code
5. Test touch + serial communication

---

## 3️⃣ MQTT Broker Setup

### **Options**

- Mosquitto (recommended)
- HiveMQ Cloud
- EMQX

### **Steps**

1. Install Mosquitto
2. Start the broker
3. Ensure both ESP32 boards are on the same WiFi
4. Use these topics:
   - `greenhouse/sensors/data`
   - `greenhouse/control`
   - `greenhouse/status`
5. Test using MQTT Explorer

---

## 4️⃣ Android App Setup

### **Requirements**

- Android Studio
- Java
- Paho MQTT library

### **Steps**

1. Open project in Android Studio
2. Update MQTT broker IP:
   ```
   final String MQTT_HOST = "tcp://YOUR_MQTT_IP:1883";
   ```
3. Connect your phone to the same WiFi
4. Run the app on a physical device

---

# 🖥️ System Architecture

```
Sensors → ESP32 Controller → MQTT Broker → Android App
Android App → MQTT Broker → ESP32 Controller → Actuators
ESP32 Controller ↔ UART ↔ ESP32 Touchscreen
```

---

# 📡 Communication Formats

### Sensor Data (MQTT)

```
{
  "temperature": "22.5",
  "humidity": "65.0",
  "soil_moisture": 42,
  "light_raw": 850,
  "fan_mode": "auto",
  "water_mode": "auto",
  "uv_mode": "auto"
}
```

### Control Commands

```
{"fan_mode": "manual"}
{"fan": "on"}
{"water": "off"}
{"uv": "on"}
```

### Serial Messages

```
BUTTON_FAN_PRESSED|COUNT:5|TIME:123456
SOIL:42
TEMP:22.5:HUM:65.0
```

---

# 🧪 Performance

- Sensor update: every 4s
- Automation check: every 2s
- Status broadcast: every 5s
- Touchscreen update: every 10s
- Response time: < 1s

---

# 🛠️ Challenges & Solutions

### MQTT Stability

Automatic reconnection + status indicators → stable communication

### Mode Conflicts

State machine + 1s propagation delay → no race conditions

### DHT11 Reliability

Improved timing + retry logic → 95% success rate

### State Synchronisation

Periodic status broadcast → all interfaces synced within 2s

---

# 🌟 Future Enhancements

### Hardware

- CO₂ sensor
- pH sensor
- Water level sensor
- Heating mat
- Solar power + battery backup

### Software

- Data logging
- Web dashboard
- Push notifications
- Predictive algorithms
- Multi‑greenhouse support

---

# 🏁 Conclusion

This project demonstrates a fully functional **IoT greenhouse automation system** with:

- Dual user interfaces
- Reliable automation
- Real‑time monitoring
- Robust communication
- Expandable hardware architecture

It serves as a strong foundation for advanced agricultural automation and provides excellent educational value in embedded systems, mobile development, and IoT communication.


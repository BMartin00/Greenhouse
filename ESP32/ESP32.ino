#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mqtt_server = "MQTT_IP";
const int mqtt_port = 1883;
const char* mqtt_topic = "greenhouse/sensors/data";
const char* mqtt_control_topic = "greenhouse/control";
const char* mqtt_status_topic = "greenhouse/status";

// Serial communication with other ESP32
#define SERIAL_ESP32 Serial2  // Using UART2 for communication

#define FAN 25
#define WATER 26
#define UV 27
#define DHT_PIN 4
#define PHOTORESISTOR_PIN 33  // GPIO 33 for photoresistor

WiFiClient espClient;
PubSubClient client(espClient);

String fan_status = "off";
String water_status = "off";
String uv_status = "off";

int sensor_pin = 34;
int sensor_analog_value;
int moisture_percentage;

// Photoresistor variables
int photoresistor_value = 0;
const int LIGHT_THRESHOLD = 1000;

// Temperature control
const float MAX_TEMP = 22.0;  // Turn fan ON above this temperature
const float TEMP_HYSTERESIS = 1.0;  // Turn fan OFF when temp drops 1°C below max

// Soil moisture control
const int SOIL_MIN = 30;  // Turn water pump ON below this
const int SOIL_MAX = 50;  // Turn water pump OFF above this

// int dryValue = 4095; // Resistive Soil Moisture Sensor
// int wetValue = 960; // Resistive Soil Moisture Sensor
int dryValue = 3200;
int wetValue = 1100;

// Mode control for all three components
enum ComponentMode { AUTO, MANUAL };
ComponentMode fan_mode = AUTO;
ComponentMode water_mode = AUTO;
ComponentMode uv_mode = AUTO;

bool manual_fan_state = false;
bool manual_water_state = false;
bool manual_uv_state = false;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000;

// DHT11 variables
float temperature = 0.0;
float humidity = 0.0;
bool dht_success = false;
unsigned long lastDHTRead = 0;
const unsigned long DHT_READ_INTERVAL = 4000;

unsigned long lastStatusSend = 0;
const unsigned long statusInterval = 5000;

void setup() {
  Serial.begin(115200);
  SERIAL_ESP32.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("Greenhouse System - Full Automation");
  Serial.println("Temperature: Fan ON > 22°C, OFF < 21°C");
  Serial.println("Soil Moisture: Water ON < 40%, OFF > 70%");
  Serial.println("UV Light: ON when dark (<450), OFF when bright (>450)");

  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);

  pinMode(WATER, OUTPUT);
  digitalWrite(WATER, LOW);

  pinMode(UV, OUTPUT);
  digitalWrite(UV, LOW);

  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, HIGH);
  delay(1000);

  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  sendSensorStatus();
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 40) {
      Serial.println("\nWiFi FAILED!");
      while(1) delay(1000);
    }
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  delay(2000);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    
    String clientId = "ESP32-CYD-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
    } else {
      Serial.println(" retry in 5s");
      delay(5000);
    }
    client.subscribe(mqtt_control_topic);
    Serial.println("Subscribed to control topic");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message received [");
  Serial.print(topic);
  Serial.print("]: ");

  String jsonString;
  for (unsigned int i = 0; i < length; i++) {
    jsonString += (char)payload[i];
  }
  Serial.println(jsonString);

  if (String(topic) == mqtt_control_topic) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
      Serial.println("JSON parse error!");
      return;
    }

    if (doc.containsKey("request") && doc["request"] == "status") {
      sendSensorStatus();
      return;
    }

    // Check for mode changes
    if (doc.containsKey("fan_mode")) {
      String mode = doc["fan_mode"];
      fan_mode = (mode == "auto") ? AUTO : MANUAL;
      Serial.print("Fan Mode: ");
      Serial.println(fan_mode == AUTO ? "AUTO" : "MANUAL");
    }

    if (doc.containsKey("water_mode")) {
      String mode = doc["water_mode"];
      water_mode = (mode == "auto") ? AUTO : MANUAL;
      Serial.print("Water Mode: ");
      Serial.println(water_mode == AUTO ? "AUTO" : "MANUAL");
    }

    if (doc.containsKey("uv_mode")) {
      String mode = doc["uv_mode"];
      uv_mode = (mode == "auto") ? AUTO : MANUAL;
      Serial.print("UV Mode: ");
      Serial.println(uv_mode == AUTO ? "AUTO" : "MANUAL");
    }

    bool stateChanged = false;
    
    // Fan control (manual override)
    if (doc.containsKey("fan")) {
      String fanCmd = doc["fan"];
      fan_mode = MANUAL;  // Switch to manual mode
      
      if (fanCmd == "on") {
        digitalWrite(FAN, HIGH);
        fan_status = "on";
        manual_fan_state = true;
        stateChanged = true;
        Serial.println("Fan manually turned ON");
      }
      else if (fanCmd == "off") {
        digitalWrite(FAN, LOW);
        fan_status = "off";
        manual_fan_state = false;
        stateChanged = true;
        Serial.println("Fan manually turned OFF");
      }
    }

    // Water control (manual override)
    if (doc.containsKey("water")) {
      String waterCmd = doc["water"];
      water_mode = MANUAL;  // Switch to manual mode
      
      if (waterCmd == "on") {
        digitalWrite(WATER, HIGH);
        water_status = "on";
        manual_water_state = true;
        stateChanged = true;
        Serial.println("Water manually turned ON");
      }
      else if (waterCmd == "off") {
        digitalWrite(WATER, LOW);
        water_status = "off";
        manual_water_state = false;
        stateChanged = true;
        Serial.println("Water manually turned OFF");
      }
    }

    // UV control (manual override)
    if (doc.containsKey("uv")) {
      String uvCmd = doc["uv"];
      uv_mode = MANUAL;  // Switch to manual mode
      
      if (uvCmd == "on") {
        digitalWrite(UV, HIGH);
        uv_status = "on";
        manual_uv_state = true;
        stateChanged = true;
        Serial.println("UV manually turned ON");
      }
      else if (uvCmd == "off") {
        digitalWrite(UV, LOW);
        uv_status = "off";
        manual_uv_state = false;
        stateChanged = true;
        Serial.println("UV manually turned OFF");
      }
    }

    if (stateChanged) {
      Serial.println("Updated relays from MQTT control!");
      sendSensorStatus();
    }
  }
}

bool readDHT11() {
  uint8_t data[5] = {0,0,0,0,0};
  
  // Send start signal
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(25);
  digitalWrite(DHT_PIN, HIGH);
  delayMicroseconds(30);
  pinMode(DHT_PIN, INPUT_PULLUP);
  delayMicroseconds(10);
  
  // Wait for DHT response with timeout
  unsigned long timeout = micros() + 200;
  while (digitalRead(DHT_PIN) == HIGH) {
    if (micros() > timeout) {
      return false;
    }
  }
  
  // Wait for LOW pulse (80us)
  timeout = micros() + 200;
  while (digitalRead(DHT_PIN) == LOW) {
    if (micros() > timeout) {
      return false;
    }
  }
  
  // Wait for HIGH pulse (80us)  
  timeout = micros() + 200;
  while (digitalRead(DHT_PIN) == HIGH) {
    if (micros() > timeout) {
      return false;
    }
  }
  
  // Read 40 bits
  for (int i = 0; i < 40; i++) {
    timeout = micros() + 100;
    while (digitalRead(DHT_PIN) == LOW) {
      if (micros() > timeout) {
        return false;
      }
    }
    
    // Measure HIGH duration
    unsigned long start = micros();
    timeout = start + 100;
    while (digitalRead(DHT_PIN) == HIGH) {
      if (micros() > timeout) {
        return false;
      }
    }
    unsigned long duration = micros() - start;
    
    // Store bit
    data[i/8] <<= 1;
    if (duration > 30) {
      data[i/8] |= 1;
    }
  }
  
  // Verify checksum
  if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    humidity = data[0] + data[1] * 0.1;
    temperature = data[2] + data[3] * 0.1;
    return true;
  } else {
    Serial.println("DHT11: Checksum error");
    return false;
  }
}

void controlAutomation() {
  static unsigned long lastControl = 0;
  if (millis() - lastControl < 2000) return;  // Check every 2 seconds
  lastControl = millis();
  
  // Read photoresistor
  photoresistor_value = analogRead(PHOTORESISTOR_PIN);
  
  // Read soil moisture
  sensor_analog_value = readSoilRaw();
  moisture_percentage = soilPercent(sensor_analog_value, dryValue, wetValue);
  moisture_percentage = constrain(moisture_percentage, 0, 100);
  
  bool stateChanged = false;
  
  // Temperature-based fan control (AUTO mode only)
  if (fan_mode == AUTO && dht_success) {
    if (temperature > MAX_TEMP) {
      // Temperature too high - turn fan ON
      if (fan_status != "on") {
        digitalWrite(FAN, HIGH);
        fan_status = "on";
        stateChanged = true;
        Serial.print("Fan: ON (AUTO - Temp: ");
        Serial.print(temperature);
        Serial.println("°C > 22°C)");
      }
    } else if (temperature < (MAX_TEMP - TEMP_HYSTERESIS)) {
      // Temperature normal - turn fan OFF
      if (fan_status != "off") {
        digitalWrite(FAN, LOW);
        fan_status = "off";
        stateChanged = true;
        Serial.print("Fan: OFF (AUTO - Temp: ");
        Serial.print(temperature);
        Serial.println("°C < 21°C)");
      }
    }
  }
  
  // Soil moisture-based watering (AUTO mode only)
  if (water_mode == AUTO) {
    if (moisture_percentage < SOIL_MIN) {
      // Soil too dry - turn water ON
      if (water_status != "on") {
        digitalWrite(WATER, HIGH);
        water_status = "on";
        stateChanged = true;
        Serial.print("Water: ON (AUTO - Soil: ");
        Serial.print(moisture_percentage);
        Serial.println("% < 40%)");
      }
    } else if (moisture_percentage > SOIL_MAX) {
      // Soil moist enough - turn water OFF
      if (water_status != "off") {
        digitalWrite(WATER, LOW);
        water_status = "off";
        stateChanged = true;
        Serial.print("Water: OFF (AUTO - Soil: ");
        Serial.print(moisture_percentage);
        Serial.println("% > 70%)");
      }
    }
  }
  
  // Light-based UV control (AUTO mode only)
  if (uv_mode == AUTO) {
    if (photoresistor_value > LIGHT_THRESHOLD) {
      // Bright - turn UV OFF
      if (uv_status != "off") {
        digitalWrite(UV, LOW);
        uv_status = "off";
        stateChanged = true;
        Serial.print("UV: OFF (AUTO - Light: ");
        Serial.print(photoresistor_value);
        Serial.println(" > 450)");
      }
    } else {
      // Dark - turn UV ON
      if (uv_status != "on") {
        digitalWrite(UV, HIGH);
        uv_status = "on";
        stateChanged = true;
        Serial.print("UV: ON (AUTO - Light: ");
        Serial.print(photoresistor_value);
        Serial.println(" < 450)");
      }
    }
  }
  
  // Manual mode: maintain manual states
  if (fan_mode == MANUAL) {
    digitalWrite(FAN, manual_fan_state ? HIGH : LOW);
    fan_status = manual_fan_state ? "on" : "off";
  }
  
  if (water_mode == MANUAL) {
    digitalWrite(WATER, manual_water_state ? HIGH : LOW);
    water_status = manual_water_state ? "on" : "off";
  }
  
  if (uv_mode == MANUAL) {
    digitalWrite(UV, manual_uv_state ? HIGH : LOW);
    uv_status = manual_uv_state ? "on" : "off";
  }
  
  if (stateChanged) {
    sendSensorStatus();
  }
}

int soilPercent(int raw, int dryVal, int wetVal) {
    float percent = (float)(dryVal - raw) / (float)(dryVal - wetVal) * 100.0;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return (int)percent;
}

int readSoilRaw() {
  const int samples = 10;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(sensor_pin);
    delay(5);
  }
  return sum / samples;
}

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read soil moistures
  int raw = readSoilRaw();
  // Serial.print("Raw soil ADC: ");
  // Serial.println(raw);
  delay(500);

  // Send status
  if (millis() - lastStatusSend >= statusInterval) {
    sendSensorStatus();
    lastStatusSend = millis();
  }

  // Read DHT11
  if (millis() - lastDHTRead >= DHT_READ_INTERVAL) {
    dht_success = readDHT11();
    lastDHTRead = millis();
  }
  
  // Control automation
  controlAutomation();

  // Send data to CYD display
  if (millis() - lastSendTime >= sendInterval) {
    sendSoilMoistureData();
    sendDHTData();
    publishMessage();
    lastSendTime = millis();
  }
  
  // Check for button commands from CYD
  if (SERIAL_ESP32.available()) {
    String message = SERIAL_ESP32.readStringUntil('\n');
    Serial.println("Received from CYD: " + message);

    if (message.indexOf("BUTTON_") != -1) {
      if (message.indexOf("BUTTON_FAN_PRESSED") != -1) {
        fan_mode = MANUAL;
        manual_fan_state = !manual_fan_state;
        fan_status = manual_fan_state ? "on" : "off";
        digitalWrite(FAN, manual_fan_state ? HIGH : LOW);
        Serial.print("Fan button: ");
        Serial.println(fan_status);
        sendSensorStatus();
      } else if (message.indexOf("BUTTON_WATER_PRESSED") != -1) {
        water_mode = MANUAL;
        manual_water_state = !manual_water_state;
        water_status = manual_water_state ? "on" : "off";
        digitalWrite(WATER, manual_water_state ? HIGH : LOW);
        Serial.print("Water button: ");
        Serial.println(water_status);
        sendSensorStatus();
      } else if (message.indexOf("BUTTON_UV_PRESSED") != -1) {
        uv_mode = MANUAL;
        manual_uv_state = !manual_uv_state;
        uv_status = manual_uv_state ? "on" : "off";
        digitalWrite(UV, manual_uv_state ? HIGH : LOW);
        Serial.print("UV button: ");
        Serial.println(uv_status);
        sendSensorStatus();
      }
    }
  }
  
  delay(1000);
}

void sendSoilMoistureData() {
  SERIAL_ESP32.println("SOIL:" + String(moisture_percentage));
}

void sendDHTData() {
  if (dht_success) {
    SERIAL_ESP32.println("TEMP:" + String(temperature, 1) + ":HUM:" + String(humidity, 1));
  } else {
    SERIAL_ESP32.println("TEMP:---:HUM:---");
  }
}

void sendSensorStatus() {
  StaticJsonDocument<512> doc;
  doc["fan"] = fan_status;
  doc["water"] = water_status;
  doc["uv"] = uv_status;
  doc["fan_mode"] = fan_mode == AUTO ? "auto" : "manual";
  doc["water_mode"] = water_mode == AUTO ? "auto" : "manual";
  doc["uv_mode"] = uv_mode == AUTO ? "auto" : "manual";
  doc["temperature"] = dht_success ? temperature : -999;
  doc["humidity"] = dht_success ? humidity : -999;
  doc["soil_moisture"] = moisture_percentage;
  doc["light_raw"] = photoresistor_value;

  char buffer[512];
  size_t n = serializeJson(doc, buffer);

  client.publish("greenhouse/status", buffer, n);

  Serial.print("Status: ");
  Serial.println(buffer);
}

void publishMessage() {
  char msg[300];
  if (dht_success) {
    snprintf(msg, 300, "{\"temperature\":%.1f,\"humidity\":%.1f,\"soil_moisture\":%d,\"light_raw\":%d,\"fan_mode\":\"%s\",\"water_mode\":\"%s\",\"uv_mode\":\"%s\"}", 
             temperature, humidity, moisture_percentage, photoresistor_value,
             fan_mode == AUTO ? "auto" : "manual",
             water_mode == AUTO ? "auto" : "manual",
             uv_mode == AUTO ? "auto" : "manual");
  } else {
    snprintf(msg, 300, "{\"temperature\":\"---\",\"humidity\":\"---\",\"soil_moisture\":%d,\"light_raw\":%d,\"fan_mode\":\"%s\",\"water_mode\":\"%s\",\"uv_mode\":\"%s\"}", 
             moisture_percentage, photoresistor_value,
             fan_mode == AUTO ? "auto" : "manual",
             water_mode == AUTO ? "auto" : "manual",
             uv_mode == AUTO ? "auto" : "manual");
  }

  client.publish(mqtt_topic, msg);
}
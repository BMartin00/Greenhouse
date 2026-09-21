#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

const char* ssid = "SSID";
const char* password = "PASSWORD";

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Serial communication with other ESP32
#define SERIAL_ESP32 Serial2  // Using UART2 for communication

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();
WiFiClient espClient;

int buttonPressCounter = 0;
unsigned long lastTouchTime = 0;

// Sensor data variables
float temperature = 0.0;
float humidity = 0.0;
int soilMoisture = 0;

void setup_wifi();
void drawUI();
void checkTouch();
void updateSensorDisplay();
void sendSerialCommand(const String& button);
void processSerialMessage(const String& message);
void updateSoilMoisture(int moisture);

void setup() {
  Serial.begin(115200);
  SERIAL_ESP32.begin(9600, SERIAL_8N1, 22, 27);  // RX=22, TX=27 for ESP32 communication
  
  delay(1000);
  
  Serial.println("ESP32 MQTT Button & Sensor Demo\n");
  
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(1);
  
  Serial.println("Touch initialized");
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  tft.setCursor(10, 10);
  tft.println("MQTT & Sensor Demo");
  tft.setCursor(10, 40);
  tft.println("Martin");
  
  setup_wifi();
  
  // Initialize with some dummy sensor data
  temperature = 22.5;
  humidity = 65.0;
  soilMoisture = 42;
  
  drawUI();

  Serial.println("Press Button Fan, Water, or UV");
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  tft.setCursor(10, 70);
  tft.print("WiFi: ");
  tft.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 40) {
      Serial.println("\nWiFi FAILED!");
      tft.setCursor(10, 100);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("WiFi FAILED!");
      while(1) delay(1000);
    }
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  tft.setCursor(10, 100);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("WiFi Connected!");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 130);
  tft.setTextSize(1);
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  tft.setTextSize(2);
  
  delay(2000);
}

void drawUI() {
  tft.fillScreen(TFT_BLACK);
  
  // Button A (Left - Blue)
  tft.fillRoundRect(10, 40, 90, 80, 10, TFT_BLUE);
  tft.drawRoundRect(10, 40, 90, 80, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(35, 70);
  tft.println("Fan");
  
  // Button B (Middle - Green)
  tft.fillRoundRect(115, 40, 90, 80, 10, TFT_DARKGREEN);
  tft.drawRoundRect(115, 40, 90, 80, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(130, 70);
  tft.println("Water");
  
  // Button C (Right - Red)
  tft.fillRoundRect(220, 40, 90, 80, 10, TFT_RED);
  tft.drawRoundRect(220, 40, 90, 80, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(250, 70);
  tft.println("UV");
  
  // Sensor data display area
  tft.drawRoundRect(10, 130, 300, 100, 10, TFT_CYAN);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(15, 135);
  tft.println("Sensor Data:");
  
  updateSensorDisplay();
  
  // Status bar
  tft.setTextSize(2);
  tft.setCursor(5, 15);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Press Fan, Water, or UV");

  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 240);
  tft.print("Presses: ");
  tft.print(buttonPressCounter);
}

void updateSensorDisplay() {
  // Temperature
  tft.fillRect(20, 155, 280, 20, TFT_BLACK);
  
  // Temperature color coding
  if (temperature < 10) {
    tft.setTextColor(TFT_BLUE, TFT_BLACK); // Cold - blue
  } else if (temperature < 25) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // Comfortable - green
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); // Hot - red
  }
  
  tft.setCursor(20, 155);
  tft.print("Temp: ");
  tft.print(temperature, 1);
  tft.print(" C");
  
  // Humidity
  tft.fillRect(20, 180, 280, 20, TFT_BLACK);
  
  // Humidity color coding
  if (humidity < 30) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK); // Dry - orange
  } else if (humidity < 70) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // Comfortable - green
  } else {
    tft.setTextColor(TFT_CYAN, TFT_BLACK); // Humid - cyan
  }
  
  tft.setCursor(20, 180);
  tft.print("Humidity: ");
  tft.print(humidity, 1);
  tft.print(" %");
  
  // Soil Moisture
  tft.fillRect(20, 205, 280, 20, TFT_BLACK);
  
  // Soil moisture color coding
  if (soilMoisture < 30) {
    tft.setTextColor(TFT_RED, TFT_BLACK); // Dry - red
  } else if (soilMoisture < 70) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // Good - green
  } else {
    tft.setTextColor(TFT_BLUE, TFT_BLACK); // Wet - blue
  }
  
  tft.setCursor(20, 205);
  tft.print("Soil: ");
  tft.print(soilMoisture);
  tft.print(" %");
}

void checkTouch() {
  if (ts.tirqTouched() && ts.touched()) {
    // Debounce: 300ms minimum between touches
    if (millis() - lastTouchTime < 300) {
      return;
    }
    
    TS_Point p = ts.getPoint();
    
    Serial.print("Touch - Pressure=");
    Serial.print(p.z);
    Serial.print(", x=");
    Serial.print(p.x);
    Serial.print(", y=");
    Serial.println(p.y);
    
    // Filter low-pressure touches
    if (p.z < 300) {
      Serial.println("too low");
      return;
    }
    
    lastTouchTime = millis();
    buttonPressCounter++;
    
    char msg[100];
    String button;
    
    // Determine which button was pressed based on X coordinate
    if (p.x < 1500) {
      button = "Fan";
      snprintf(msg, 100, "{\"button\":\"Fan\",\"count\":%d,\"pressure\":%d}", buttonPressCounter, p.z);
      
      tft.fillRoundRect(10, 40, 90, 80, 10, TFT_CYAN);
      Serial.println("  -> BUTTON FAN PRESSED");
      
    } else if (p.x < 2500) {
      button = "Water";
      snprintf(msg, 100, "{\"button\":\"Water\",\"count\":%d,\"pressure\":%d}", buttonPressCounter, p.z);
      
      tft.fillRoundRect(115, 40, 90, 80, 10, TFT_GREEN);
      Serial.println("  -> BUTTON WATER PRESSED");
      
    } else {
      button = "UV";
      snprintf(msg, 100, "{\"button\":\"UV\",\"count\":%d,\"pressure\":%d}", buttonPressCounter, p.z);
      
      tft.fillRoundRect(220, 40, 90, 80, 10, TFT_ORANGE);
      Serial.println("  -> BUTTON UV PRESSED");
    }
    
    // Send serial command to ESP32 Controller
    sendSerialCommand(button);
    
    // Update counter display
    tft.fillRect(0, 240, 320, 20, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 240);
    tft.print("Presses: ");
    tft.print(buttonPressCounter);
    tft.print(" (");
    tft.print(button);
    tft.print(")");
    
    delay(200);
    drawUI();
  }
}

void sendSerialCommand(const String& button) {
  String command;
  
  if (button == "Fan") {
    command = "BUTTON_FAN_PRESSED";
    
  } else if (button == "Water") {
    command = "BUTTON_WATER_PRESSED";
    
  } else if (button == "UV") {
    command = "BUTTON_UV_PRESSED";
  }
  
  // Add timestamp and counter to command
  command += "|COUNT:" + String(buttonPressCounter) + "|TIME:" + String(millis());
  
  // Send command to ESP32 Controller
  SERIAL_ESP32.println(command);
  Serial.println("Sent to ESP32: " + command);
  
  // Update sensor display
  updateSensorDisplay();
}

void loop() {
  checkTouch();
  
  // Check for incoming serial messages from other ESP32
  if (SERIAL_ESP32.available()) {
    String message = SERIAL_ESP32.readStringUntil('\n');
    message.trim();
    // Serial.println("Received from ESP32: " + message);
    
    // Process incoming messages
    processSerialMessage(message);
  }
  
  delay(100);
}

void processSerialMessage(const String& message) {
  // Check if it's soil moisture data
  if (message.startsWith("SOIL:")) {
    String moistureStr = message.substring(5); // Remove "SOIL:"
    int newMoisture = moistureStr.toInt();
    
    if (newMoisture >= 0 && newMoisture <= 100) {
      soilMoisture = newMoisture;
      // Serial.print("Updated soil moisture: ");
      Serial.print(soilMoisture);
      Serial.println("%");
      
      // Update the display
      updateSensorDisplay();
    }
  }
  // Check if it's temperature and humidity data
  else if (message.startsWith("TEMP:")) {
    // Format: TEMP:temperature:HUM:humidity
    int tempStart = 5; // Start after "TEMP:"
    int humStart = message.indexOf(":HUM:");
    
    if (humStart != -1) {
      String tempStr = message.substring(tempStart, humStart);
      String humStr = message.substring(humStart + 5); // Start after ":HUM:"
      
      // Check if data is valid (not "---")
      if (tempStr != "---" && humStr != "---") {
        float newTemp = tempStr.toFloat();
        float newHum = humStr.toFloat();
        
        // Validate reasonable ranges
        if (newTemp >= -40 && newTemp <= 80 && newHum >= 0 && newHum <= 100) {
          temperature = newTemp;
          humidity = newHum;
          
          // Serial.print("Updated sensor data - Temp: ");
          // Serial.print(temperature);
          // Serial.print("C, Hum: ");
          // Serial.print(humidity);
          // Serial.println("%");
          
          // Update the display
          updateSensorDisplay();
        }
      } else {
        // Handle invalid data (sensor error)
        Serial.println("DHT11 sensor error - no valid data");
      }
    }
  }
}
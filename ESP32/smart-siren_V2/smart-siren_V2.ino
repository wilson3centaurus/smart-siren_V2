#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// ================================
// CONFIGURATION VARIABLES - ADJUST HERE
// ================================
const char* WIFI_SSID = "Wilson-Note20-Ultra";
const char* WIFI_PASSWORD = "punisher";
const char* NTP_SERVER = "pool.ntp.org";
const int GMT_OFFSET_SEC = 2 * 3600; // GMT+2 (Zimbabwe)
const int DAYLIGHT_OFFSET_SEC = 0;

// GPIO Pin Configuration
const int SIREN_PIN = 23;        // 3V DC Buzzer
const int BELL_PIN = 22;         // Servo Motor
const int STATUS_LED_PIN = 2;    // Built-in LED
const int ACTIVITY_LED_PIN = 4;  // Activity indicator

// Timing Configuration (milliseconds)
const int DEFAULT_SIREN_DURATION = 5000;  // 5 seconds
const int DEFAULT_BELL_DURATION = 3000;   // 3 seconds
const int SERVO_SWING_TIME = 1000;        // 1 second per swing cycle
const int EMERGENCY_PATTERN_CYCLES = 3;   // Number of alternating cycles

// System Configuration
const int MAX_ALARMS = 20;
const int WEB_SERVER_PORT = 80;
const int ALARM_CHECK_WINDOW = 5; // Check alarms within first 5 seconds of minute

// ================================
// GLOBAL VARIABLES
// ================================
WebServer server(WEB_SERVER_PORT);
Preferences preferences;
Servo bellServo;

struct Alarm {
  String name;
  int hour;
  int minute;
  int dayMask;  // Bit mask: Monday=1, Tuesday=2, Wednesday=4, etc.
  int type;     // 0=siren, 1=bell
  bool enabled;
};

Alarm alarms[MAX_ALARMS];
int alarmCount = 0;
bool sirenActive = false;
bool bellActive = false;
unsigned long sirenEndTime = 0;
unsigned long bellEndTime = 0;
unsigned long servoStartTime = 0;
int servoPosition = 0;
bool servoMovingUp = true;

// ================================
// SETUP FUNCTION
// ================================
void setup() {
  Serial.begin(115200);
  Serial.println("|============= FOR DEBUGGING =============|");
  Serial.println("Rusununguko High School Smart Siren System Starting...");
  
  // Initialize GPIO pins
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(ACTIVITY_LED_PIN, HIGH); // Default on
  
  // Initialize servo
  bellServo.attach(BELL_PIN);
  bellServo.write(0); // Rest position
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  
  // Initialize preferences
  preferences.begin("siren_system", false);
  loadAlarms();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Setup web server routes
  setupWebServer();
  
  // Start web server
  server.begin();
  Serial.println("Web server started on port " + String(WEB_SERVER_PORT));
  Serial.println("System ready!");
}

// ================================
// MAIN LOOP
// ================================
void loop() {
  server.handleClient();
  
  // Handle active siren
  if (sirenActive && millis() >= sirenEndTime) {
    stopSiren();
  }
  
  // Handle active bell with servo movement
  if (bellActive) {
    handleBellServo();
    if (millis() >= bellEndTime) {
      stopBell();
    }
  }
  
  // Check for scheduled alarms (once per minute)
  static unsigned long lastAlarmCheck = 0;
  if (millis() - lastAlarmCheck >= 60000) { // Check every minute
    checkScheduledAlarms();
    lastAlarmCheck = millis();
  }
  
  delay(50); // Small delay to prevent overwhelming the CPU
}

// ================================
// WIFI CONNECTION
// ================================
void connectToWiFi() {

  // Setting static IP
  IPAddress local_IP(192, 168, 141, 90);
  IPAddress gateway(192, 168, 192, 112);     // Your router's IP
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8);
  IPAddress secondaryDNS(8, 8, 4, 4);
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure!");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Blink status LED while connecting
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.println("IP address: " + WiFi.localIP().toString());
  digitalWrite(STATUS_LED_PIN, HIGH); // Solid on when connected
}

// ================================
// HARDWARE CONTROL FUNCTIONS
// ================================
void triggerSiren(int duration = DEFAULT_SIREN_DURATION) {
  Serial.println("Triggering siren for " + String(duration) + "ms");
  digitalWrite(SIREN_PIN, HIGH);
  sirenActive = true;
  sirenEndTime = millis() + duration;
  startActivityLED();
}

void stopSiren() {
  Serial.println("Stopping siren");
  digitalWrite(SIREN_PIN, LOW);
  sirenActive = false;
  stopActivityLED();
}

void triggerBell(int duration = DEFAULT_BELL_DURATION) {
  Serial.println("Triggering bell for " + String(duration) + "ms");
  bellActive = true;
  bellEndTime = millis() + duration;
  servoStartTime = millis();
  servoPosition = 0;
  servoMovingUp = true;
  bellServo.write(0);
  startActivityLED();
}

void stopBell() {
  Serial.println("Stopping bell");
  bellActive = false;
  bellServo.write(0); // Return to rest position
  stopActivityLED();
}

void handleBellServo() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - servoStartTime;
  
  // Calculate position based on time (0-90-0 cycle every SERVO_SWING_TIME ms)
  float cycleProgress = (float)(elapsedTime % SERVO_SWING_TIME) / SERVO_SWING_TIME;
  
  int targetPosition;
  if (cycleProgress < 0.5) {
    // First half: 0 to 90
    targetPosition = (int)(cycleProgress * 2 * 90);
  } else {
    // Second half: 90 to 0
    targetPosition = (int)((2 - cycleProgress * 2) * 90);
  }
  
  if (targetPosition != servoPosition) {
    servoPosition = targetPosition;
    bellServo.write(servoPosition);
  }
}

void triggerEmergencyPattern() {
  Serial.println("Triggering emergency pattern");
  for (int i = 0; i < EMERGENCY_PATTERN_CYCLES; i++) {
    triggerSiren(1000);
    while (sirenActive) {
      server.handleClient();
      if (millis() >= sirenEndTime) stopSiren();
      delay(10);
    }
    delay(200);
    
    triggerBell(1000);
    while (bellActive) {
      server.handleClient();
      handleBellServo();
      if (millis() >= bellEndTime) stopBell();
      delay(10);
    }
    delay(200);
  }
}

void startActivityLED() {
  // Activity LED will fade in loop - implement simple PWM fade
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void stopActivityLED() {
  digitalWrite(ACTIVITY_LED_PIN, HIGH); // Return to default on state
}

// ================================
// ALARM MANAGEMENT
// ================================
void loadAlarms() {
  alarmCount = preferences.getInt("alarmCount", 0);
  for (int i = 0; i < alarmCount; i++) {
    String prefix = "alarm" + String(i) + "_";
    alarms[i].name = preferences.getString((prefix + "name").c_str(), "");
    alarms[i].hour = preferences.getInt((prefix + "hour").c_str(), 0);
    alarms[i].minute = preferences.getInt((prefix + "minute").c_str(), 0);
    alarms[i].dayMask = preferences.getInt((prefix + "dayMask").c_str(), 0);
    alarms[i].type = preferences.getInt((prefix + "type").c_str(), 0);
    alarms[i].enabled = preferences.getBool((prefix + "enabled").c_str(), true);
  }
  Serial.println("Loaded " + String(alarmCount) + " alarms");
}

void saveAlarms() {
  preferences.putInt("alarmCount", alarmCount);
  for (int i = 0; i < alarmCount; i++) {
    String prefix = "alarm" + String(i) + "_";
    preferences.putString((prefix + "name").c_str(), alarms[i].name);
    preferences.putInt((prefix + "hour").c_str(), alarms[i].hour);
    preferences.putInt((prefix + "minute").c_str(), alarms[i].minute);
    preferences.putInt((prefix + "dayMask").c_str(), alarms[i].dayMask);
    preferences.putInt((prefix + "type").c_str(), alarms[i].type);
    preferences.putBool((prefix + "enabled").c_str(), alarms[i].enabled);
  }
  Serial.println("Saved " + String(alarmCount) + " alarms");
}

void checkScheduledAlarms() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time for alarm check");
    return;
  }
  
  // Only check in the first few seconds of each minute to prevent duplicates
  if (timeinfo.tm_sec > ALARM_CHECK_WINDOW) {
    return;
  }
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentDayMask = 1 << timeinfo.tm_wday; // tm_wday: 0=Sunday, 1=Monday, etc.
  
  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].enabled && 
        alarms[i].hour == currentHour && 
        alarms[i].minute == currentMinute &&
        (alarms[i].dayMask & currentDayMask)) {
      
      Serial.println("Triggering scheduled alarm: " + alarms[i].name);
      if (alarms[i].type == 0) {
        triggerSiren();
      } else {
        triggerBell();
      }
    }
  }
}

// ================================
// WEB SERVER SETUP
// ================================
void setupWebServer() {
  // Serve static files
  server.onNotFound([]() {
    String path = server.uri();
    if (path.endsWith("/")) path += "main.html";
    
    if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, "r");
      String contentType = getContentType(path);
      server.streamFile(file, contentType);
      file.close();
    } else {
      // Redirect to main page
      server.sendHeader("Location", "/");
      server.send(302);
    }
  });
  
  // API Routes
  server.on("/api/alarms", HTTP_GET, handleGetAlarms);
  server.on("/api/alarms", HTTP_POST, handleAddAlarm);
  server.on("/api/alarms/update", HTTP_POST, handleUpdateAlarm);
  server.on("/api/alarms/delete", HTTP_POST, handleDeleteAlarm);
  server.on("/api/trigger", HTTP_POST, handleTrigger);
  server.on("/api/time", HTTP_GET, handleGetTime);
  server.on("/api/system", HTTP_GET, handleGetSystem);
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

// ================================
// API HANDLERS
// ================================
void handleGetAlarms() {
  DynamicJsonDocument doc(4096);
  JsonArray alarmsArray = doc.createNestedArray("alarms");
  
  for (int i = 0; i < alarmCount; i++) {
    JsonObject alarm = alarmsArray.createNestedObject();
    alarm["id"] = i;
    alarm["name"] = alarms[i].name;
    alarm["hour"] = alarms[i].hour;
    alarm["minute"] = alarms[i].minute;
    alarm["dayMask"] = alarms[i].dayMask;
    alarm["type"] = alarms[i].type;
    alarm["enabled"] = alarms[i].enabled;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAddAlarm() {
  if (alarmCount >= MAX_ALARMS) {
    server.send(400, "application/json", "{\"error\":\"Maximum alarms reached\"}");
    return;
  }
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server.arg("plain"));
  
  alarms[alarmCount].name = doc["name"].as<String>();
  alarms[alarmCount].hour = doc["hour"];
  alarms[alarmCount].minute = doc["minute"];
  alarms[alarmCount].dayMask = doc["dayMask"];
  alarms[alarmCount].type = doc["type"];
  alarms[alarmCount].enabled = doc["enabled"];
  
  alarmCount++;
  saveAlarms();
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleUpdateAlarm() {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server.arg("plain"));
  
  int id = doc["id"];
  if (id < 0 || id >= alarmCount) {
    server.send(400, "application/json", "{\"error\":\"Invalid alarm ID\"}");
    return;
  }
  
  alarms[id].name = doc["name"].as<String>();
  alarms[id].hour = doc["hour"];
  alarms[id].minute = doc["minute"];
  alarms[id].dayMask = doc["dayMask"];
  alarms[id].type = doc["type"];
  alarms[id].enabled = doc["enabled"];
  
  saveAlarms();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeleteAlarm() {
  DynamicJsonDocument doc(512);
  deserializeJson(doc, server.arg("plain"));
  
  int id = doc["id"];
  if (id < 0 || id >= alarmCount) {
    server.send(400, "application/json", "{\"error\":\"Invalid alarm ID\"}");
    return;
  }
  
  // Shift alarms down
  for (int i = id; i < alarmCount - 1; i++) {
    alarms[i] = alarms[i + 1];
  }
  alarmCount--;
  
  saveAlarms();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleTrigger() {
  DynamicJsonDocument doc(512);
  deserializeJson(doc, server.arg("plain"));
  
  int type = doc["type"];
  
  switch (type) {
    case 0:
      triggerSiren();
      break;
    case 1:
      triggerBell();
      break;
    case 2:
      triggerEmergencyPattern();
      break;
    default:
      server.send(400, "application/json", "{\"error\":\"Invalid trigger type\"}");
      return;
  }
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    server.send(500, "application/json", "{\"error\":\"Failed to get time\"}");
    return;
  }
  
  DynamicJsonDocument doc(512);
  doc["hour"] = timeinfo.tm_hour;
  doc["minute"] = timeinfo.tm_min;
  doc["second"] = timeinfo.tm_sec;
  doc["day"] = timeinfo.tm_mday;
  doc["month"] = timeinfo.tm_mon + 1;
  doc["year"] = timeinfo.tm_year + 1900;
  doc["weekday"] = timeinfo.tm_wday;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetSystem() {
  DynamicJsonDocument doc(1024);
  doc["uptime"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["wifiStatus"] = (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected";
  doc["wifiSSID"] = WiFi.SSID();
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["ipAddress"] = WiFi.localIP().toString();
  doc["alarmCount"] = alarmCount;
  doc["sirenActive"] = sirenActive;
  doc["bellActive"] = bellActive;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

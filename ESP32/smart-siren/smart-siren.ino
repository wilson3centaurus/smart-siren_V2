#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

AsyncWebServer server(80);
const char* ssid = "Digital";
const char* password = "12345678";

// Hardware pins
#define BUZZER_PIN 23
#define LED_WIFI 2
#define LED_SIREN 4
#define LED_BELL 5

void setup() {
  Serial.begin(115200);
  while(!Serial); // Wait for serial port to connect
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_SIREN, OUTPUT);
  pinMode(LED_BELL, OUTPUT);

  // Initialize SPIFFS with error handling
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    while(1) delay(1000); // Halt if filesystem fails
  }

  // Initialize data files if they don't exist
  initializeDataFiles();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_WIFI, HIGH);
  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());

  // Configure time with error handling
  configTime(0, 0, "pool.ntp.org");
  Serial.println("Waiting for NTP time sync");
  time_t now = time(nullptr);
  while(now < 24*3600){
    delay(100);
    now = time(nullptr);
  }

  setupServer();
  server.begin();
  Serial.println("HTTP server started");
}

void initializeDataFiles() {
  // Create required directories
  if(!SPIFFS.exists("/data")){
    SPIFFS.mkdir("/data");
  }

  // Initialize users.json if it doesn't exist
  if(!SPIFFS.exists("/data/users.json")){
    File file = SPIFFS.open("/data/users.json", "w");
    if(file){
      file.println("{\"users\":[{\"username\":\"admin\",\"password\":\"admin\",\"role\":\"admin\"}]}");
      file.close();
    }
  }

  // Initialize alarms.json if it doesn't exist
  if(!SPIFFS.exists("/data/alarms.json")){
    File file = SPIFFS.open("/data/alarms.json", "w");
    if(file){
      file.println("{\"alarms\":[]}");
      file.close();
    }
  }

  // Initialize status.json if it doesn't exist
  if(!SPIFFS.exists("/data/status.json")){
    File file = SPIFFS.open("/data/status.json", "w");
    if(file){
      file.println("{\"siren\":{\"status\":\"inactive\",\"lastTriggered\":0},\"bell\":{\"status\":\"inactive\",\"lastTriggered\":0}}");
      file.close();
    }
  }

  // Initialize logs.json if it doesn't exist
  if(!SPIFFS.exists("/data/logs.json")){
    File file = SPIFFS.open("/data/logs.json", "w");
    if(file){
      file.println("{\"logs\":[]}");
      file.close();
    }
  }
}

// [Rest of your existing functions remain the same until getRequestBody]

String getRequestBody(AsyncWebServerRequest *request) {
  if(!request) return String();
  
  // Check for plain text body
  if(request->hasParam("plain", true)){
    AsyncWebParameter* p = request->getParam("plain", true);
    if(p) return p->value();
  }
  
  // Check for form data
  if(request->hasParam("body", true)){
    AsyncWebParameter* p = request->getParam("body", true);
    if(p) return p->value();
  }
  
  return String();
}

// [Rest of your existing code continues...]

void loop() {
  checkAlarms();
  delay(1000);
}

void checkAlarms() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  File file = SPIFFS.open("/data/alarms.json", "r");
  if (!file) return;

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, file);
  file.close();

  for (JsonObject alarm : doc["alarms"].as<JsonArray>()) {
    int alarmHour = alarm["time"].as<String>().substring(0, 2).toInt();
    int alarmMinute = alarm["time"].as<String>().substring(3, 5).toInt();

    if (timeinfo.tm_hour == alarmHour &&
        timeinfo.tm_min == alarmMinute &&
        timeinfo.tm_sec == 0) {

      String alarmType = alarm["type"];
      if (alarmType == "siren") {
        triggerSiren();
      } else {
        triggerBell();
      }

      logActivity("Alarm triggered: " + alarm["name"].as<String>(), "info");
    }
  }
}

void triggerSiren() {
  digitalWrite(LED_SIREN, HIGH);
  tone(BUZZER_PIN, 1000, 5000);
  delay(5000);
  digitalWrite(LED_SIREN, LOW);
  updateStatus("siren", "active");
}

void triggerBell() {
  digitalWrite(LED_BELL, HIGH);
  tone(BUZZER_PIN, 800, 3000);
  delay(3000);
  digitalWrite(LED_BELL, LOW);
  updateStatus("bell", "active");
}

void updateStatus(String device, String status) {
  File file = SPIFFS.open("/data/status.json", "r");
  DynamicJsonDocument doc(256);
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }

  doc[device]["status"] = status;
  doc[device]["lastTriggered"] = time(nullptr);

  file = SPIFFS.open("/data/status.json", "w");
  serializeJson(doc, file);
  file.close();
}

void logActivity(String message, String type) {
  File file = SPIFFS.open("/data/logs.json", "r");
  DynamicJsonDocument doc(2048);
  if (file && file.size() > 0) {
    deserializeJson(doc, file);
  }
  file.close();

  JsonObject log = doc["logs"].createNestedObject();
  log["timestamp"] = time(nullptr);
  log["message"] = message;
  log["type"] = type;

  file = SPIFFS.open("/data/logs.json", "w");
  serializeJson(doc, file);
  file.close();
}

// ---------------------- API Handlers ----------------------

void handleStatus(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["ip"] = WiFi.localIP().toString();

  File statusFile = SPIFFS.open("/data/status.json", "r");
  if (statusFile) {
    DynamicJsonDocument statusDoc(256);
    deserializeJson(statusDoc, statusFile);
    doc["siren"] = statusDoc["siren"];
    doc["bell"] = statusDoc["bell"];
    statusFile.close();
  }

  doc["system"]["uptime"] = millis() / 1000;
  doc["system"]["freeHeap"] = ESP.getFreeHeap();

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  request->send(200, "application/json", jsonResponse);
}

void handleGetAlarms(AsyncWebServerRequest *request) {
  File file = SPIFFS.open("/data/alarms.json", "r");
  if (!file) {
    request->send(500, "text/plain", "Error reading alarms");
    return;
  }
  request->send(200, "application/json", file.readString());
  file.close();
}

void handleAddAlarm(AsyncWebServerRequest *request) {
  String body = getRequestBody(request);
  if (body.length() == 0) {
    request->send(400, "text/plain", "Missing request body");
    return;
  }

  DynamicJsonDocument newAlarm(256);
  DeserializationError error = deserializeJson(newAlarm, body);
  if (error) {
    request->send(400, "text/plain", "Invalid JSON: " + String(error.c_str()));
    return;
  }

  File file = SPIFFS.open("/data/alarms.json", "r");
  DynamicJsonDocument doc(1024);
  if (file && file.size() > 0) {
    deserializeJson(doc, file);
  }
  file.close();

  int newId = 1;
  for (JsonObject alarm : doc["alarms"].as<JsonArray>()) {
    if (alarm["id"].as<int>() >= newId) newId = alarm["id"].as<int>() + 1;
  }
  newAlarm["id"] = newId;
  doc["alarms"].add(newAlarm);

  file = SPIFFS.open("/data/alarms.json", "w");
  serializeJson(doc, file);
  file.close();

  request->send(200, "application/json", "{\"success\":true}");
}

void handleRingSiren(AsyncWebServerRequest *request) {
  triggerSiren();
  request->send(200, "text/plain", "Siren activated");
}

void handleRingBell(AsyncWebServerRequest *request) {
  triggerBell();
  request->send(200, "text/plain", "Bell activated");
}

void handleGetLogs(AsyncWebServerRequest *request) {
  File file = SPIFFS.open("/data/logs.json", "r");
  if (!file) {
    request->send(500, "text/plain", "Error reading logs");
    return;
  }
  request->send(200, "application/json", file.readString());
  file.close();
}

void handleLogin(AsyncWebServerRequest *request) {
  String body = getRequestBody(request);
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  File file = SPIFFS.open("/data/users.json", "r");
  DynamicJsonDocument usersDoc(512);
  deserializeJson(usersDoc, file);
  file.close();

  for (JsonObject user : usersDoc["users"].as<JsonArray>()) {
    if (user["username"] == doc["username"] &&
        user["password"] == doc["password"]) {

      DynamicJsonDocument response(128);
      response["username"] = user["username"];
      response["role"] = user["role"];

      String jsonResponse;
      serializeJson(response, jsonResponse);
      request->send(200, "application/json", jsonResponse);
      return;
    }
  }

  request->send(401, "text/plain", "Invalid credentials");
}

void setupServer() {
  server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");

  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/alarms", HTTP_GET, handleGetAlarms);
  server.on("/api/alarms", HTTP_POST, handleAddAlarm);
  server.on("/api/ring-siren", HTTP_POST, handleRingSiren);
  server.on("/api/ring-bell", HTTP_POST, handleRingBell);
  server.on("/api/logs", HTTP_GET, handleGetLogs);

  server.on("/ws", HTTP_GET, [](AsyncWebServerRequest *request) {
    // WebSocket upgrade handled internally
  });
}

// ---------------------- Utility ----------------------

String getRequestBody(AsyncWebServerRequest *request) {
  String body = "";
  if (request->hasParam("plain", true)) {
    body = request->getParam("plain", true)->value();
  }
  return body;
}

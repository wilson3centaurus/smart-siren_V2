// ======== Smart Siren System V2 for Rusununguko ZIMFEP High School ======== 

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <time.h>

// ===== CONFIGURATION =====

// WiFi credentials
const char* WIFI_SSID = "Wilson-Note20-Ultra";
const char* WIFI_PASSWORD = "punisher";

// Time configuration
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 7200;      // Zimbabwe is GMT+2 (7200 seconds)
const int   DAYLIGHT_OFFSET_SEC = 0;    // No DST

// Pin definitions
const int SIREN_PIN = 23;               // School siren output pin
const int BELL_PIN = 22;                // Dining hall bell output pin
const int STATUS_LED_PIN = 2;           // Built-in status LED

// Default durations
const int DEFAULT_SIREN_DURATION = 5000;    // Default siren duration (ms)
const int DEFAULT_BELL_DURATION = 3000;     // Default bell duration (ms)

bool serverStarted = false;

// ===== GLOBAL VARIABLES =====

// Server instance
AsyncWebServer server(80);

// For storing alarm settings
Preferences preferences;

// Time tracking
struct tm timeinfo;
bool timeSet = false;
unsigned long lastTimeCheck = 0;
const unsigned long TIME_CHECK_INTERVAL = 60000; // Check time every minute

// Alarm structure
struct Alarm {
  String name;
  int hour;
  int minute;
  uint8_t days;    // Bit field: bit 0 = Monday, bit 1 = Tuesday, etc.
  int type;        // 0 = siren, 1 = bell
  bool enabled;
};

// Alarm list
std::vector<Alarm> alarms;

// ===== FUNCTION DECLARATIONS =====
//void connectToWiFi();
void setupServer();
void setupTime();
void loadAlarms();
void saveAlarms();
void checkAlarms();
void blinkLED(int times);
void triggerSiren(int duration);
void triggerBell(int duration);
String getTimeString();
String getDayOfWeek(int day);
String getFormattedTime(int hour, int minute);
String generateTokenForUser(String username);
bool validateToken(String token);
void logMessage(String message);

void setup() {
  Serial.begin(115200);
  Serial.println("=== Smart Siren Starting ===");

  pinMode(SIREN_PIN, OUTPUT);
  pinMode(BELL_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(SIREN_PIN, LOW);
  digitalWrite(BELL_PIN, LOW);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS ready");
  }

  preferences.begin("smart-siren", false);
  loadAlarms();

  WiFi.onEvent(WiFiEvent);

  connectToWiFi();

  blinkLED(3);
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case IP_EVENT_STA_GOT_IP:
      Serial.println("📶 WiFi connected! IP: " + WiFi.localIP().toString());
      setupTime();

      if (!serverStarted) {
        setupServer();
        server.begin();  // ✅ Safe to start now
        serverStarted = true;
        Serial.println("🌐 Web server started");
      }
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      Serial.println("⚠️ WiFi lost. Reconnecting...");
      WiFi.reconnect();
      serverStarted = false;
      break;

    default:
      break;
  }
}


// ===== MAIN LOOP =====
void loop() {
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    connectToWiFi();
  }
  
  // Update time periodically and check alarms
  unsigned long currentMillis = millis();
  if (currentMillis - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    lastTimeCheck = currentMillis;
    
    if (!getLocalTime(&timeinfo)) {
      Serial.println("ERROR: Failed to obtain time");
      timeSet = false;
    } else {
      timeSet = true;
      Serial.print("Current time: ");
      Serial.println(getTimeString());
      
      // Check if any alarms should be triggered
      checkAlarms();
    }
  }
  
  // Small delay to prevent hogging CPU
  delay(10);
}

// ===== WIFI FUNCTIONS =====
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait for connection with timeout
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN)); // Blink while connecting
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(STATUS_LED_PIN, HIGH); // LED on when connected
    setupServer();
    server.begin();  // ✅ Safe to start now
    serverStarted = true;
    Serial.println("🌐 Web server started");
  } else {
    Serial.println("\nWiFi connection failed!");
    digitalWrite(STATUS_LED_PIN, LOW);  // LED off when disconnected
  }
}

// ===== TIME FUNCTIONS =====

void setupTime() {
  Serial.println("Configuring time...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Try to get time
  if (!getLocalTime(&timeinfo)) {
    Serial.println("ERROR: Failed to obtain time");
    timeSet = false;
  } else {
    timeSet = true;
    Serial.print("Current time: ");
    Serial.println(getTimeString());
  }
}

String getTimeString() {
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

String getFormattedTime(int hour, int minute) {
  char buffer[6];
  sprintf(buffer, "%02d:%02d", hour, minute);
  return String(buffer);
}

String getDayOfWeek(int day) {
  const char* days[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  if (day >= 0 && day < 7) {
    return days[day];
  }
  return "Unknown";
}

// ===== ALARM FUNCTIONS =====
void loadAlarms() {
  Serial.println("Loading alarms from storage...");
  
  // Clear existing alarms
  alarms.clear();
  
  // Get number of stored alarms
  int alarmCount = preferences.getInt("alarmCount", 0);
  Serial.printf("Found %d alarms\n", alarmCount);
  
  // Load each alarm
  for (int i = 0; i < alarmCount; i++) {
    String prefix = "alarm" + String(i) + "_";
    
    Alarm alarm;
    // Convert String concatenation to const char* using c_str()
    alarm.name = preferences.getString((prefix + "name").c_str(), "Alarm " + String(i+1));
    alarm.hour = preferences.getInt((prefix + "hour").c_str(), 0);
    alarm.minute = preferences.getInt((prefix + "minute").c_str(), 0);
    alarm.days = preferences.getUChar((prefix + "days").c_str(), 0);
    alarm.type = preferences.getInt((prefix + "type").c_str(), 0);
    alarm.enabled = preferences.getBool((prefix + "enabled").c_str(), true);
    
    alarms.push_back(alarm);
    
    Serial.printf("Loaded alarm: %s, %02d:%02d, Type: %d, Days: %d, Enabled: %d\n", 
                  alarm.name.c_str(), alarm.hour, alarm.minute, alarm.type, alarm.days, alarm.enabled);
  }
}

void saveAlarms() {
  Serial.println("Saving alarms to storage...");
  
  // Clear previous settings
  preferences.clear();
  
  // Save number of alarms
  preferences.putInt("alarmCount", alarms.size());
  
  // Save each alarm
  for (size_t i = 0; i < alarms.size(); i++) {
    String prefix = "alarm" + String(i) + "_";
    
    // Convert String concatenation to const char* using c_str()
    preferences.putString((prefix + "name").c_str(), alarms[i].name);
    preferences.putInt((prefix + "hour").c_str(), alarms[i].hour);
    preferences.putInt((prefix + "minute").c_str(), alarms[i].minute);
    preferences.putUChar((prefix + "days").c_str(), alarms[i].days);
    preferences.putInt((prefix + "type").c_str(), alarms[i].type);
    preferences.putBool((prefix + "enabled").c_str(), alarms[i].enabled);
  }
  
  Serial.printf("Saved %d alarms\n", alarms.size());
}

void checkAlarms() {
  if (!timeSet) return;
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentSecond = timeinfo.tm_sec;
  
  // Only check when seconds are 0-5 to avoid multiple triggers in the same minute
  if (currentSecond > 5) return;
  
  // Convert day of week to our format (0 = Monday, 6 = Sunday)
  // tm_wday is (0 = Sunday, 6 = Saturday)
  int dayIndex = timeinfo.tm_wday == 0 ? 6 : timeinfo.tm_wday - 1;
  uint8_t dayBit = 1 << dayIndex;
  
  Serial.printf("Checking alarms for %02d:%02d on day bit %d\n", currentHour, currentMinute, dayBit);
  
  // Check each alarm
  for (const Alarm& alarm : alarms) {
    if (alarm.enabled && 
        alarm.hour == currentHour && 
        alarm.minute == currentMinute && 
        (alarm.days & dayBit)) {
      
      Serial.printf("Triggering alarm: %s\n", alarm.name.c_str());
      
      if (alarm.type == 0) {
        triggerSiren(DEFAULT_SIREN_DURATION);
      } else {
        triggerBell(DEFAULT_BELL_DURATION);
      }
    }
  }
}

// ===== HARDWARE CONTROL FUNCTIONS =====

void triggerSiren(int duration) {
  Serial.printf("Activating siren for %d ms\n", duration);
  
  digitalWrite(SIREN_PIN, HIGH);
  delay(duration);
  digitalWrite(SIREN_PIN, LOW);
}

void triggerBell(int duration) {
  Serial.printf("Activating bell for %d ms\n", duration);
  
  digitalWrite(BELL_PIN, HIGH);
  delay(duration);
  digitalWrite(BELL_PIN, LOW);
}

void blinkLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(100);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(100);
  }
}

// ===== AUTHENTICATION FUNCTIONS =====

String generateTokenForUser(String username) {
  // Very simple token generation for demo purposes
  // In production, use a more secure method
  return username + String(random(100000, 999999));
}

bool validateToken(String token) {
  // This is a simplified validation
  // In production, implement proper token validation
  return token.length() > 10;
}

// ===== SERVER SETUP =====

void setupServer() {
  // Default route - serve web interface
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("main.html");
  
  // API endpoints
  /*
  // Get current time
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!timeSet) {
      request->send(500, "application/json", "{\"error\":\"Time not set\"}");
      return;
    }
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["time"] = getTimeString();
    doc["hour"] = timeinfo.tm_hour;
    doc["minute"] = timeinfo.tm_min;
    doc["second"] = timeinfo.tm_sec;
    doc["day"] = timeinfo.tm_mday;
    doc["month"] = timeinfo.tm_mon + 1;
    doc["year"] = timeinfo.tm_year + 1900;
    doc["weekday"] = timeinfo.tm_wday;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Authentication
  AsyncCallbackJsonWebHandler* loginHandler = new AsyncCallbackJsonWebHandler("/api/login", [](AsyncWebServerRequest *request, JsonVariant &json) {
    DynamicJsonDocument data(1024);
    if (json.is<JsonObject>()) {
      //data = json.as<JsonObject>();
      data.set(json);
      
      String username = data["username"];
      String password = data["password"];
      
      // Hardcoded credentials for demo - replace with proper authentication
      bool isValidUser = 
        (username == "admin" && password == "admin123") ||
        (username == "headmaster" && password == "head123") ||
        (username == "dhmaster" && password == "dh123");
      
      if (isValidUser) {
        DynamicJsonDocument response(256);
        response["success"] = true;
        response["token"] = generateTokenForUser(username);
        response["user"] = username;
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
      } else {
        request->send(401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  });
  server.addHandler(loginHandler);

  // Get all alarms
  server.on("/api/alarms", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Authentication check would go here in production
    
    DynamicJsonDocument doc(4096);
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    
    for (size_t i = 0; i < alarms.size(); i++) {
      JsonObject alarmObj = alarmsArray.createNestedObject();
      alarmObj["id"] = i;
      alarmObj["name"] = alarms[i].name;
      alarmObj["hour"] = alarms[i].hour;
      alarmObj["minute"] = alarms[i].minute;
      alarmObj["days"] = alarms[i].days;
      alarmObj["type"] = alarms[i].type;
      alarmObj["enabled"] = alarms[i].enabled;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Add new alarm
  AsyncCallbackJsonWebHandler* addAlarmHandler = new AsyncCallbackJsonWebHandler("/api/alarms", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // Authentication check would go here in production
    
    DynamicJsonDocument data(1024);
    if (json.is<JsonObject>()) {
      //data = json.as<JsonObject>();
      data.set(json);
      
      Alarm newAlarm;
      newAlarm.name = data["name"] | "New Alarm";
      newAlarm.hour = data["hour"] | 0;
      newAlarm.minute = data["minute"] | 0;
      newAlarm.days = data["days"] | 0;
      newAlarm.type = data["type"] | 0;
      newAlarm.enabled = data["enabled"] | true;
      
      alarms.push_back(newAlarm);
      saveAlarms();
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Alarm added\"}");
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  });
  server.addHandler(addAlarmHandler);

  // Update existing alarm
  AsyncCallbackJsonWebHandler* updateAlarmHandler = new AsyncCallbackJsonWebHandler("/api/alarms/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // Authentication check would go here in production
    
    DynamicJsonDocument data(1024);
    if (json.is<JsonObject>()) {
      //data = json.as<JsonObject>();
      data.set(json);
      
      int id = data["id"] | -1;
      if (id >= 0 && id < (int)alarms.size()) {
        if (data.containsKey("name")) alarms[id].name = data["name"].as<String>();
        if (data.containsKey("hour")) alarms[id].hour = data["hour"];
        if (data.containsKey("minute")) alarms[id].minute = data["minute"];
        if (data.containsKey("days")) alarms[id].days = data["days"];
        if (data.containsKey("type")) alarms[id].type = data["type"];
        if (data.containsKey("enabled")) alarms[id].enabled = data["enabled"];
        
        saveAlarms();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Alarm updated\"}");
      } else {
        request->send(404, "application/json", "{\"success\":false,\"message\":\"Alarm not found\"}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  });
  server.addHandler(updateAlarmHandler);

  // Delete alarm
  AsyncCallbackJsonWebHandler* deleteAlarmHandler = new AsyncCallbackJsonWebHandler("/api/alarms/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // Authentication check would go here in production
    
    DynamicJsonDocument data(1024);
    if (json.is<JsonObject>()) {
      //data = json.as<JsonObject>();
      data.set(json);
      int id = data["id"] | -1;
      if (id >= 0 && id < (int)alarms.size()) {
        alarms.erase(alarms.begin() + id);
        saveAlarms();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Alarm deleted\"}");
      } else {
        request->send(404, "application/json", "{\"success\":false,\"message\":\"Alarm not found\"}");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  });
  server.addHandler(deleteAlarmHandler);

  // Trigger siren or bell manually
  AsyncCallbackJsonWebHandler* triggerHandler = new AsyncCallbackJsonWebHandler("/api/trigger", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // Authentication check would go here in production
    
    DynamicJsonDocument data(1024);
    if (json.is<JsonObject>()) {
      //data = json.as<JsonObject>();
      data.set(json);
      
      int type = data["type"] | 0;    // 0=siren, 1=bell, 2=emergency
      int duration = data["duration"] | DEFAULT_SIREN_DURATION;
      
      // Limit duration for safety
      if (duration > 30000) duration = 30000;
      
      switch (type) {
        case 0:  // School siren
          triggerSiren(duration);
          break;
        case 1:  // Dining hall bell
          triggerBell(duration);
          break;
        case 2:  // Emergency (alternating)
          for (int i = 0; i < 3; i++) {
            triggerSiren(1000);
            delay(500);
            triggerBell(1000);
            delay(500);
          }
          break;
        default:
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid type\"}");
          return;
      }
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Trigger activated\"}");
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  });
  server.addHandler(triggerHandler);

  // System info
  server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    doc["uptime"] = millis() / 1000;
    doc["ip"] = WiFi.localIP().toString();
    doc["wifi_ssid"] = WIFI_SSID;
    doc["wifi_strength"] = WiFi.RSSI();
    doc["time_sync"] = timeSet;
    doc["alarm_count"] = alarms.size();
    doc["free_heap"] = ESP.getFreeHeap();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Fallback for all other routes
  server.onNotFound([](AsyncWebServerRequest *request) {
    // Redirect to index for any unknown path
    request->redirect("/");
  });
*/
}

// ===== UTILITY FUNCTIONS =====

void logMessage(String message) {
  Serial.println(message);
  // You could also log to a file on SPIFFS or send to a remote logging service
}

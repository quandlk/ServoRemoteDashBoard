/**
 * Dùng SPIFFS để lưu trữ các file giao diện
 * Thử nghiệm trên mạch ESP32-S3 16MB Flash và 8MB SPRAM
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#define LED 34          // LED trên GPIO 34
#define PIN_NEOPIXEL 33 // NeoPixel trên GPIO 33

String ssid = "GauTruc";
String password = "0967062707";

// Định nghĩa 8 servo trên GPIO 1-8
const int servoPins[8] = {1, 2, 3, 4, 5, 6, 7, 8};
const int freq = 50;       // Tần số 50 Hz
const int resolution = 14; // Giảm xuống 14-bit (tối đa cho ESP32-S3)
const int minDuty = 410;   // Duty cho 0° (500µs tại 50 Hz, 14-bit)
const int maxDuty = 2048;   // Duty cho 180° (2400µs tại 50 Hz, 14-bit)

// Hàm chuyển đổi góc thành duty cycle
int angleToDuty(int angle) {
  return map(angle, 0, 180, minDuty, maxDuty);
}

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences preferences;

String sessionToken = "";

// Biến trạng thái cho 8 servo
bool servoMoving[8] = {false};

String generateSessionToken() {
  String token = "";
  for (int i = 0; i < 16; i++) {
    token += (char)random(65, 90);
  }
  Serial.printf("[%.3f] Generated session token: %s\n", millis() / 1000.0, token.c_str());
  return token;
}

String getPublicIP() {
  HTTPClient http;
  http.begin("http://api.ipify.org");
  int httpCode = http.GET();
  String ip = "Unknown";
  if (httpCode == HTTP_CODE_OK) {
    ip = http.getString();
  }
  http.end();
  return ip;
}

void sendStatus() {
  StaticJsonDocument<300> doc;
  doc["wifiName"] = ssid;
  doc["wifiStrength"] = WiFi.RSSI();
  doc["localIp"] = WiFi.localIP().toString();
  doc["publicIp"] = getPublicIP();
  doc["temperature"] = temperatureRead();
  for (int i = 0; i < 8; i++) {
    doc["servo" + String(i + 1) + "Moving"] = servoMoving[i];
  }
  String json;
  serializeJson(doc, json);
  //Serial.printf("[%.3f] Sending status via WebSocket: %s\n", millis() / 1000.0, json.c_str());
  ws.textAll(json);
}

void moveServo(int servoId, int targetAngle) {
  int channel = servoId - 1;
  if (servoMoving[channel]) {
    Serial.printf("[%.3f] Servo %d already moving, skipping\n", millis() / 1000.0, servoId);
    return;
  }

  servoMoving[channel] = true;

  if (servoId == 1) neopixelWrite(PIN_NEOPIXEL, 79, 70, 229);
  else if (servoId == 2) neopixelWrite(PIN_NEOPIXEL, 46, 204, 113);
  else if (servoId == 3) neopixelWrite(PIN_NEOPIXEL, 231, 76, 60);
  else if (servoId == 4) neopixelWrite(PIN_NEOPIXEL,255, 236, 69);
  else if (servoId == 5) neopixelWrite(PIN_NEOPIXEL,155, 89, 182);
  else if (servoId == 6) neopixelWrite(PIN_NEOPIXEL, 52, 152, 219);
  else if (servoId == 7) neopixelWrite(PIN_NEOPIXEL, 230, 126, 34);
  else if (servoId == 8) neopixelWrite(PIN_NEOPIXEL, 26, 188, 156);
  else neopixelWrite(PIN_NEOPIXEL, RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS);

  Serial.printf("[%.3f] Starting servo %d movement to angle: %d\n", millis() / 1000.0, servoId, targetAngle);
  sendStatus();

  for (int pos = 0; pos <= targetAngle; pos++) {
    ledcWrite(channel, angleToDuty(pos));
    delay(5);
    yield();
  }
  for (int pos = targetAngle; pos >= 0; pos--) {
    ledcWrite(channel, angleToDuty(pos));
    delay(5);
    yield();
  }

  servoMoving[channel] = false;
  neopixelWrite(PIN_NEOPIXEL, 0, 0, 0);
  sendStatus();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    sendStatus();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("[%.3f] Starting ESP32 initialization...\n", millis() / 1000.0);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(100);
  Serial.printf("[%.3f] LED initialized on GPIO %d\n", millis() / 1000.0, LED);

  esp_task_wdt_init(10, true);
  Serial.printf("[%.3f] Watchdog timer initialized with 10s timeout\n", millis() / 1000.0);

  // Khởi tạo 8 servo bằng LEDC
  for (int i = 0; i < 8; i++) {
    if (ledcSetup(i, freq, resolution) == 0) { // Kiểm tra lỗi
      Serial.printf("[%.3f] Error: Failed to setup LEDC channel %d\n", millis() / 1000.0, i);
    } else {
      ledcAttachPin(servoPins[i], i);
      ledcWrite(i, angleToDuty(0));
      Serial.printf("[%.3f] Servo %d initialized on GPIO %d\n", millis() / 1000.0, i + 1, servoPins[i]);
    }
  }
  Serial.printf("[%.3f] 8 servos initialization attempted\n", millis() / 1000.0);

  if (!SPIFFS.begin(true)) {
    Serial.printf("[%.3f] Error: Failed to initialize SPIFFS\n", millis() / 1000.0);
    return;
  }

  preferences.begin("credentials", false);
  if (preferences.getString("username", "") == "") preferences.putString("username", "admin");
  if (preferences.getString("password", "") == "") preferences.putString("password", "12345678");
  for (int i = 1; i <= 8; i++) {
    String key = "servo" + String(i) + "Angle";
    if (preferences.getInt(key.c_str(), -1) == -1) preferences.putInt(key.c_str(), 180);
  }
  if (preferences.getString("wifi_ssid", "") == "") preferences.putString("wifi_ssid", ssid);
  else ssid = preferences.getString("wifi_ssid", ssid);
  if (preferences.getString("wifi_password", "") == "") preferences.putString("wifi_password", password);
  else password = preferences.getString("wifi_password", password);
  preferences.end();

  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[%.3f] WiFi connected: %s\n", millis() / 1000.0, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("[%.3f] WiFi connection failed\n", millis() / 1000.0);
  }

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(SPIFFS, "/login.html", "text/html");
    } else {
      request->send(SPIFFS, "/index.html", "text/html");
    }
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/script.js", "application/javascript");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Favicon not found");
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();
      preferences.begin("credentials", true);
      String storedUsername = preferences.getString("username", "");
      String storedPassword = preferences.getString("password", "");
      preferences.end();
      if (username == storedUsername && password == storedPassword) {
        sessionToken = generateSessionToken();
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/");
        response->addHeader("Set-Cookie", "session=" + sessionToken + "; Path=/; HttpOnly");
        request->send(response);
      } else {
        request->send(401, "text/plain", "Invalid credentials");
      }
    } else {
      request->send(400, "text/plain", "Missing credentials");
    }
  });

  server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request){
    sessionToken = "";
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader("Location", "/");
    response->addHeader("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    request->send(response);
  });

  for (int i = 1; i <= 8; i++) {
    String path = "/servo" + String(i);
    server.on(path.c_str(), HTTP_GET, [i](AsyncWebServerRequest *request){
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
      } else if (servoMoving[i - 1]) {
        request->send(409, "text/plain", "Servo " + String(i) + " is already moving");
      } else {
        preferences.begin("credentials", true);
        int angle = preferences.getInt(("servo" + String(i) + "Angle").c_str(), 180);
        preferences.end();
        moveServo(i, angle);
        request->send(200, "text/plain", "Servo " + String(i) + " moved");
      }
    });
  }

  server.on("/change-password", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        return;
      }
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error || !doc["currentPassword"].is<String>() || !doc["newPassword"].is<String>()) {
        request->send(400, "text/plain", "Invalid JSON or missing parameters");
        return;
      }
      String currentPassword = doc["currentPassword"].as<String>();
      String newPassword = doc["newPassword"].as<String>();
      preferences.begin("credentials", false);
      String storedPassword = preferences.getString("password", "");
      if (currentPassword == storedPassword) {
        preferences.putString("password", newPassword);
        preferences.end();
        request->send(200, "text/plain", "Password changed successfully");
      } else {
        preferences.end();
        request->send(400, "text/plain", "Current password incorrect");
      }
    });

  server.on("/save-servo-settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        return;
      }
      StaticJsonDocument<400> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
      }
      for (int i = 1; i <= 8; i++) {
        String key = "servo" + String(i);
        if (!doc[key].is<int>()) {
          request->send(400, "text/plain", "Missing parameters for servo " + String(i));
          return;
        }
      }
      preferences.begin("credentials", false);
      for (int i = 1; i <= 8; i++) {
        String key = "servo" + String(i);
        int angle = doc[key].as<int>();
        preferences.putInt(("servo" + String(i) + "Angle").c_str(), angle);
      }
      preferences.end();
      request->send(200, "text/plain", "Servo settings saved");
    });

  server.on("/get-servo-settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      return;
    }
    preferences.begin("credentials", true);
    String json = "{";
    for (int i = 1; i <= 8; i++) {
      String key = "servo" + String(i);
      json += "\"" + key + "\":" + String(preferences.getInt((key + "Angle").c_str(), 180));
      if (i < 8) json += ",";
    }
    json += "}";
    preferences.end();
    request->send(200, "application/json", json);
  });

  server.on("/connect-wifi", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        return;
      }
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error || !doc["ssid"].is<String>() || !doc["password"].is<String>()) {
        request->send(400, "text/plain", "Invalid or missing parameters");
        return;
      }
      String newSSID = doc["ssid"].as<String>();
      String newPassword = doc["password"].as<String>();
      preferences.begin("credentials", false);
      preferences.putString("wifi_ssid", newSSID);
      preferences.putString("wifi_password", newPassword);
      preferences.end();
      request->send(200, "text/plain", "WiFi credentials updated, restarting ESP32...");
      delay(1000);
      ESP.restart();
    });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      return;
    }
    String json = "{\"wifiName\":\"" + String(ssid) + "\",";
    json += "\"wifiStrength\":" + String(WiFi.RSSI()) + ",";
    json += "\"localIp\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"publicIp\":\"" + getPublicIP() + "\",";
    json += "\"temperature\":" + String(temperatureRead()) + "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.printf("[%.3f] Web server started on port 80\n", millis() / 1000.0);
}

void loop() {
  ws.cleanupClients();
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 5000) {
    sendStatus();
    lastUpdate = millis();
    digitalWrite(LED, !digitalRead(LED));
    delay(100);
    digitalWrite(LED, !digitalRead(LED));
  }
}
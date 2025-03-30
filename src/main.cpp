/**
 * Dùng SPIFFS để lưu trữ các file giao diện
 * Thử nghiệm trên mạch ESP32-S3 16MB Flash và 8MB SPRAM
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h> // Thay SD bằng SPIFFS
#include <ESP32Servo.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h> // Thư viện cho Watchdog Timer

#define LED 34          // LED trên GPIO 34
#define PIN_NEOPIXEL 33  // NeoPixel trên GPIO 33

String ssid = "GauTruc";
String password = "0967062707";

Servo servo1, servo2, servo3;
const int servoPin1 = 1; // Servo1 trên GPIO 1
const int servoPin2 = 2; // Servo2 trên GPIO 2
const int servoPin3 = 3; // Servo3 trên GPIO 3

const int minUs = 500;
const int maxUs = 2400;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences preferences;

String sessionToken = "";

/**
 * @brief Tạo một session token ngẫu nhiên gồm 16 ký tự chữ cái in hoa.
 * @return Chuỗi token 16 ký tự (A-Z).
 */
String generateSessionToken() {
  String token = "";
  for (int i = 0; i < 16; i++) {
    token += (char)random(65, 90);
  }
  Serial.printf("[%.3f] Generated session token: %s\n", millis() / 1000.0, token.c_str());
  return token;
}

bool servo1Moving = false, servo2Moving = false, servo3Moving = false;

/**
 * @brief Lấy địa chỉ IP công cộng của thiết bị từ dịch vụ ipify.org.
 * @return Chuỗi chứa IP công cộng, hoặc "Unknown" nếu yêu cầu thất bại.
 */
String getPublicIP() {
  HTTPClient http;
  http.begin("http://api.ipify.org");
  int httpCode = http.GET();
  String ip = "Unknown";
  if (httpCode == HTTP_CODE_OK) {
    ip = http.getString();
    Serial.printf("[%.3f] Public IP retrieved: %s\n", millis() / 1000.0, ip.c_str());
  } else {
    Serial.printf("[%.3f] Failed to get public IP, HTTP code: %d\n", millis() / 1000.0, httpCode);
  }
  http.end();
  return ip;
}

/**
 * @brief Gửi trạng thái hệ thống (WiFi, IP, nhiệt độ, servo) qua WebSocket tới tất cả client.
 */
void sendStatus() {
  StaticJsonDocument<200> doc;
  doc["wifiName"] = ssid;
  doc["wifiStrength"] = WiFi.RSSI();
  doc["localIp"] = WiFi.localIP().toString();
  doc["publicIp"] = getPublicIP();
  doc["temperature"] = temperatureRead();
  doc["servo1Moving"] = servo1Moving;
  doc["servo2Moving"] = servo2Moving;
  doc["servo3Moving"] = servo3Moving;
  String json;
  serializeJson(doc, json);
  Serial.printf("[%.3f] Sending status via WebSocket: %s\n", millis() / 1000.0, json.c_str());
  ws.textAll(json);
}

/**
 * @brief Di chuyển servo đến góc mục tiêu và quay về 0 độ.
 */
void moveServo(Servo& servo, int targetAngle, int servoId) {
  if (servoId == 1 && servo1Moving) {
    Serial.printf("[%.3f] Servo %d already moving, skipping\n", millis() / 1000.0, servoId);
    return;
  }
  if (servoId == 2 && servo2Moving) {
    Serial.printf("[%.3f] Servo %d already moving, skipping\n", millis() / 1000.0, servoId);
    return;
  }
  if (servoId == 3 && servo3Moving) {
    Serial.printf("[%.3f] Servo %d already moving, skipping\n", millis() / 1000.0, servoId);
    return;
  }

  if (servoId == 1) servo1Moving = true;
  if (servoId == 2) servo2Moving = true;
  if (servoId == 3) servo3Moving = true;

  if (servoId == 1) neopixelWrite(PIN_NEOPIXEL, 0, 0, RGB_BRIGHTNESS);
  if (servoId == 2) neopixelWrite(PIN_NEOPIXEL, 0, RGB_BRIGHTNESS, 0);
  if (servoId == 3) neopixelWrite(PIN_NEOPIXEL, RGB_BRIGHTNESS, 0, 0);

  Serial.printf("[%.3f] Starting servo %d movement to angle: %d\n", millis() / 1000.0, servoId, targetAngle);
  sendStatus();

  for (int posDegrees = 0; posDegrees <= targetAngle; posDegrees++) {
    servo.write(posDegrees);
    delay(5);
    yield();
  }

  for (int posDegrees = targetAngle; posDegrees >= 0; posDegrees--) {
    servo.write(posDegrees);
    delay(5);
    yield();
  }

  Serial.printf("[%.3f] Completed servo %d movement to angle: %d\n", millis() / 1000.0, servoId, targetAngle);

  if (servoId == 1) servo1Moving = false;
  if (servoId == 2) servo2Moving = false;
  if (servoId == 3) servo3Moving = false;

  neopixelWrite(PIN_NEOPIXEL, 0, 0, 0);

  sendStatus();
}

/**
 * @brief Xử lý các sự kiện WebSocket (kết nối, ngắt kết nối).
 */
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[%.3f] WebSocket client connected: %d\n", millis() / 1000.0, client->id());
    sendStatus();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[%.3f] WebSocket client disconnected: %d\n", millis() / 1000.0, client->id());
  }
}

/**
 * @brief Thiết lập ban đầu cho ESP32: khởi tạo phần cứng, kết nối WiFi, và cấu hình server.
 */
void setup() {
  Serial.begin(115200);
  Serial.printf("[%.3f] Starting ESP32 initialization...\n", millis() / 1000.0);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(100); // Đợi khởi động USB
  Serial.printf("[%.3f] LED initialized on GPIO %d\n", millis() / 1000.0, LED);

  // Tăng timeout của Task Watchdog Timer lên 10 giây
  esp_task_wdt_init(10, true);
  Serial.printf("[%.3f] Watchdog timer initialized with 10s timeout\n", millis() / 1000.0);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);
  servo1.attach(servoPin1, minUs, maxUs);
  servo2.attach(servoPin2, minUs, maxUs);
  servo3.attach(servoPin3, minUs, maxUs);
  servo1.write(0);
  servo2.write(0);
  servo3.write(0);
  Serial.printf("[%.3f] Servos initialized on GPIO %d, %d, %d\n", millis() / 1000.0, servoPin1, servoPin2, servoPin3);

  // Khởi động SPIFFS
  if (!SPIFFS.begin(true)) { // true: format SPIFFS nếu lỗi
    Serial.printf("[%.3f] Error: Failed to initialize SPIFFS\n", millis() / 1000.0);
    return;
  }
  Serial.printf("[%.3f] SPIFFS initialized successfully\n", millis() / 1000.0);

  preferences.begin("credentials", false);
  if (preferences.getString("username", "") == "") {
    preferences.putString("username", "admin");
    Serial.printf("[%.3f] Set default username: admin\n", millis() / 1000.0);
  }
  if (preferences.getString("password", "") == "") {
    preferences.putString("password", "12345678");
    Serial.printf("[%.3f] Set default password: 12345678\n", millis() / 1000.0);
  }
  if (preferences.getInt("servo1Angle", -1) == -1) {
    preferences.putInt("servo1Angle", 180);
    Serial.printf("[%.3f] Set default servo1Angle: 180\n", millis() / 1000.0);
  }
  if (preferences.getInt("servo2Angle", -1) == -1) {
    preferences.putInt("servo2Angle", 180);
    Serial.printf("[%.3f] Set default servo2Angle: 180\n", millis() / 1000.0);
  }
  if (preferences.getInt("servo3Angle", -1) == -1) {
    preferences.putInt("servo3Angle", 180);
    Serial.printf("[%.3f] Set default servo3Angle: 180\n", millis() / 1000.0);
  }
  if (preferences.getString("wifi_ssid", "") == "") {
    preferences.putString("wifi_ssid", ssid);
    Serial.printf("[%.3f] Set default WiFi SSID: %s\n", millis() / 1000.0, ssid.c_str());
  } else {
    ssid = preferences.getString("wifi_ssid", ssid);
    Serial.printf("[%.3f] Loaded WiFi SSID from preferences: %s\n", millis() / 1000.0, ssid.c_str());
  }
  if (preferences.getString("wifi_password", "") == "") {
    preferences.putString("wifi_password", password);
    Serial.printf("[%.3f] Set default WiFi password: %s\n", millis() / 1000.0, password.c_str());
  } else {
    password = preferences.getString("wifi_password", password);
    Serial.printf("[%.3f] Loaded WiFi password from preferences\n", millis() / 1000.0);
  }
  preferences.end();

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("[%.3f] Attempting WiFi connection to SSID: %s with password: %s\n", 
                millis() / 1000.0, ssid.c_str(), password.c_str());
  
  int attempts = 0;
  const int maxAttempts = 20; // Giới hạn 20 lần thử (10 giây)
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    attempts++;
    Serial.printf("[%.3f] Connecting to WiFi - SSID: %s, Password: %s, Attempt #%d, Status: %d\n", 
                  millis() / 1000.0, ssid.c_str(), password.c_str(), attempts, WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[%.3f] WiFi connected successfully\n", millis() / 1000.0);
    Serial.printf("[%.3f] Local IP: %s\n", millis() / 1000.0, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("[%.3f] Failed to connect to WiFi - SSID: %s after %d attempts\n", 
                  millis() / 1000.0, ssid.c_str(), maxAttempts);
  }

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  Serial.printf("[%.3f] WebSocket handler registered\n", millis() / 1000.0);

  // Trang chính: phục vụ từ SPIFFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET / request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      Serial.printf("[%.3f] Serving login.html (not authenticated)\n", millis() / 1000.0);
      request->send(SPIFFS, "/login.html", "text/html");
    } else {
      Serial.printf("[%.3f] Serving index.html (authenticated)\n", millis() / 1000.0);
      request->send(SPIFFS, "/index.html", "text/html");
    }
  });

  // File CSS
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /style.css request received\n", millis() / 1000.0);
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // File JavaScript
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /script.js request received\n", millis() / 1000.0);
    request->send(SPIFFS, "/script.js", "application/javascript");
  });
  // Lấy favicon
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Favicon not found");
  });

  // Xử lý đăng nhập
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] POST /login request received\n", millis() / 1000.0);
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();
      Serial.printf("[%.3f] Login attempt - Username: %s\n", millis() / 1000.0, username.c_str());
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
        Serial.printf("[%.3f] Login successful, redirecting to /\n", millis() / 1000.0);
      } else {
        request->send(401, "text/plain", "Invalid credentials");
        Serial.printf("[%.3f] Login failed: Invalid credentials\n", millis() / 1000.0);
      }
    } else {
      request->send(400, "text/plain", "Missing credentials");
      Serial.printf("[%.3f] Login failed: Missing username or password\n", millis() / 1000.0);
    }
  });

  // Xử lý đăng xuất
  server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] POST /logout request received\n", millis() / 1000.0);
    sessionToken = "";
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader("Location", "/");
    response->addHeader("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    request->send(response);
    Serial.printf("[%.3f] Logout successful, redirecting to /\n", millis() / 1000.0);
  });

  // Điều khiển servo 1
  server.on("/servo1", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /servo1 request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      Serial.printf("[%.3f] Servo 1 control failed: Unauthorized\n", millis() / 1000.0);
    } else if (servo1Moving) {
      request->send(409, "text/plain", "Servo 1 is already moving");
      Serial.printf("[%.3f] Servo 1 control failed: Already moving\n", millis() / 1000.0);
    } else {
      preferences.begin("credentials", true);
      int angle = preferences.getInt("servo1Angle", 180);
      preferences.end();
      moveServo(servo1, angle, 1);
      request->send(200, "text/plain", "Servo 1 moved");
      Serial.printf("[%.3f] Servo 1 moved successfully\n", millis() / 1000.0);
    }
  });

  // Điều khiển servo 2
  server.on("/servo2", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /servo2 request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      Serial.printf("[%.3f] Servo 2 control failed: Unauthorized\n", millis() / 1000.0);
    } else if (servo2Moving) {
      request->send(409, "text/plain", "Servo 2 is already moving");
      Serial.printf("[%.3f] Servo 2 control failed: Already moving\n", millis() / 1000.0);
    } else {
      preferences.begin("credentials", true);
      int angle = preferences.getInt("servo2Angle", 180);
      preferences.end();
      moveServo(servo2, angle, 2);
      request->send(200, "text/plain", "Servo 2 moved");
      Serial.printf("[%.3f] Servo 2 moved successfully\n", millis() / 1000.0);
    }
  });

  // Điều khiển servo 3
  server.on("/servo3", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /servo3 request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      Serial.printf("[%.3f] Servo 3 control failed: Unauthorized\n", millis() / 1000.0);
    } else if (servo3Moving) {
      request->send(409, "text/plain", "Servo 3 is already moving");
      Serial.printf("[%.3f] Servo 3 control failed: Already moving\n", millis() / 1000.0);
    } else {
      preferences.begin("credentials", true);
      int angle = preferences.getInt("servo3Angle", 180);
      preferences.end();
      moveServo(servo3, angle, 3);
      request->send(200, "text/plain", "Servo 3 moved");
      Serial.printf("[%.3f] Servo 3 moved successfully\n", millis() / 1000.0);
    }
  });

  // Thay đổi mật khẩu
  server.on("/change-password", HTTP_POST, [](AsyncWebServerRequest *request){}, 
    NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      Serial.printf("[%.3f] POST /change-password request received\n", millis() / 1000.0);
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        Serial.printf("[%.3f] Change password failed: Unauthorized\n", millis() / 1000.0);
        return;
      }
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        Serial.printf("[%.3f] Change password failed: Invalid JSON (%s)\n", millis() / 1000.0, error.c_str());
        return;
      }
      if (!doc["currentPassword"].is<String>() || !doc["newPassword"].is<String>()) {
        request->send(400, "text/plain", "Missing parameters");
        Serial.printf("[%.3f] Change password failed: Missing parameters\n", millis() / 1000.0);
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
        Serial.printf("[%.3f] Password changed successfully to: %s\n", millis() / 1000.0, newPassword.c_str());
      } else {
        preferences.end();
        request->send(400, "text/plain", "Current password incorrect");
        Serial.printf("[%.3f] Change password failed: Current password incorrect\n", millis() / 1000.0);
      }
    });

  // Lưu cài đặt góc servo
  server.on("/save-servo-settings", HTTP_POST, [](AsyncWebServerRequest *request){}, 
    NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      Serial.printf("[%.3f] POST /save-servo-settings request received\n", millis() / 1000.0);
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        Serial.printf("[%.3f] Save servo settings failed: Unauthorized\n", millis() / 1000.0);
        return;
      }
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        Serial.printf("[%.3f] Save servo settings failed: Invalid JSON (%s)\n", millis() / 1000.0, error.c_str());
        return;
      }
      if (!doc["servo1"].is<int>() || !doc["servo2"].is<int>() || !doc["servo3"].is<int>()) {
        request->send(400, "text/plain", "Missing parameters");
        Serial.printf("[%.3f] Save servo settings failed: Missing parameters\n", millis() / 1000.0);
        return;
      }
      int servo1Angle = doc["servo1"].as<int>();
      int servo2Angle = doc["servo2"].as<int>();
      int servo3Angle = doc["servo3"].as<int>();
      preferences.begin("credentials", false);
      preferences.putInt("servo1Angle", servo1Angle);
      preferences.putInt("servo2Angle", servo2Angle);
      preferences.putInt("servo3Angle", servo3Angle);
      preferences.end();
      request->send(200, "text/plain", "Servo settings saved");
      Serial.printf("[%.3f] Servo settings saved: servo1=%d, servo2=%d, servo3=%d\n", millis() / 1000.0, servo1Angle, servo2Angle, servo3Angle);
    });

  // Lấy cài đặt góc servo
  server.on("/get-servo-settings", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /get-servo-settings request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      Serial.printf("[%.3f] Get servo settings failed: Unauthorized\n", millis() / 1000.0);
      return;
    }
    preferences.begin("credentials", true);
    String json = "{";
    json += "\"servo1\":" + String(preferences.getInt("servo1Angle", 180)) + ",";
    json += "\"servo2\":" + String(preferences.getInt("servo2Angle", 180)) + ",";
    json += "\"servo3\":" + String(preferences.getInt("servo3Angle", 180));
    json += "}";
    preferences.end();
    request->send(200, "application/json", json);
    Serial.printf("[%.3f] Servo settings sent: %s\n", millis() / 1000.0, json.c_str());
  });

  // Kết nối WiFi mới
  server.on("/connect-wifi", HTTP_POST, [](AsyncWebServerRequest *request){}, 
    NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      Serial.printf("[%.3f] POST /connect-wifi request received\n", millis() / 1000.0);
      String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
      if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
        request->send(401, "text/plain", "Unauthorized");
        Serial.printf("[%.3f] Connect WiFi failed: Unauthorized\n", millis() / 1000.0);
        return;
      }

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error || !doc["ssid"].is<String>() || !doc["password"].is<String>()) {
        request->send(400, "text/plain", "Invalid or missing parameters");
        Serial.printf("[%.3f] Connect WiFi failed: Invalid JSON or parameters\n", millis() / 1000.0);
        return;
      }

      String newSSID = doc["ssid"].as<String>();
      String newPassword = doc["password"].as<String>();
      Serial.printf("[%.3f] Saving new WiFi credentials - SSID: %s\n", millis() / 1000.0, newSSID.c_str());

      // Ghi đè thông tin WiFi mới vào Preferences
      preferences.begin("credentials", false);
      preferences.putString("wifi_ssid", newSSID);
      preferences.putString("wifi_password", newPassword);
      preferences.end();

      // Gửi phản hồi thành công và khởi động lại ESP32
      request->send(200, "text/plain", "WiFi credentials updated, restarting ESP32...");
      Serial.printf("[%.3f] WiFi credentials updated, restarting ESP32...\n", millis() / 1000.0);
      delay(1000); // Đợi 1 giây để phản hồi được gửi
      ESP.restart();
  });

  // Trả về trạng thái hệ thống
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[%.3f] GET /status request received\n", millis() / 1000.0);
    String clientSession = request->getHeader("Cookie") ? request->getHeader("Cookie")->value() : "";
    if (clientSession.indexOf("session=" + sessionToken) == -1 || sessionToken == "") {
      request->send(401, "text/plain", "Unauthorized");
      Serial.printf("[%.3f] Get status failed: Unauthorized\n", millis() / 1000.0);
      return;
    }
    String json = "{";
    json += "\"wifiName\":\"" + String(ssid) + "\",";
    json += "\"wifiStrength\":" + String(WiFi.RSSI()) + ",";
    json += "\"localIp\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"publicIp\":\"" + getPublicIP() + "\",";
    json += "\"temperature\":" + String(temperatureRead());
    json += "}";
    request->send(200, "application/json", json);
    Serial.printf("[%.3f] Status sent: %s\n", millis() / 1000.0, json.c_str());
  });

  server.begin();
  Serial.printf("[%.3f] Web server started on port 80\n", millis() / 1000.0);
}

/**
 * @brief Vòng lặp chính: dọn dẹp client WebSocket và cập nhật trạng thái định kỳ.
 */
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
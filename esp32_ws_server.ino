#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------
// Minimal ESP32-S3 WebSocket server for Margarita Machine
// ------------------------------------------------------------
// This sketch creates its own Wi-Fi access point and listens on
// ws://192.168.4.1/ws, matching the Python app's default URI.
//
// It accepts incoming JSON messages, prints the raw payload, parses it,
// then prints the decoded JSON structure to the Serial Monitor.
//
// Libraries to install in Arduino IDE:
//   - WebSockets by Markus Sattler
//   - ArduinoJson by Benoît Blanchon
// ------------------------------------------------------------

const char *AP_SSID = "MargaritaMachine";
const char *AP_PASSWORD = "12345678"; // Optional; can be left empty for open AP
const float MAX_DRINK_OUNCES = 16.0f;

// Pump IDs from ui/main_window.py mapped to GPIO pins.
// GPIO35-37 can be reserved for flash or PSRAM on ESP32-S3 variants,
// so use this generally safer test mapping until the board pinout is known.
const uint8_t PUMP_PINS[] = {0, 4, 5, 6, 7, 8, 9};
const uint8_t PUMP_COUNT = 6;
bool pumpRunning[PUMP_COUNT + 1] = {false};
unsigned long pumpStopAt[PUMP_COUNT + 1] = {0};

WebSocketsServer webSocket = WebSocketsServer(80);

float totalOuncesInServePayload(const JsonDocument &doc) {
  float total_oz = 0.0f;

  if (!doc.containsKey("action") || strcmp(doc["action"], "serve") != 0) {
    return total_oz;
  }

  if (!doc.containsKey("items") || !doc["items"].is<JsonArray>()) {
    return total_oz;
  }

  for (JsonVariant item : doc["items"].as<JsonArray>()) {
    if (!item.containsKey("oz")) {
      continue;
    }

    if (item["oz"].is<float>() || item["oz"].is<int>()) {
      total_oz += item["oz"].as<float>();
    }
  }

  return total_oz;
}

bool isValidPumpId(int pumpId) {
  return pumpId >= 1 && pumpId <= PUMP_COUNT;
}

void startPump(int pumpId, float ounces) {
  if (!isValidPumpId(pumpId) || ounces <= 0) {
    Serial.printf("[PUMP] Ignoring invalid request: pump=%d, oz=%.2f\n", pumpId, ounces);
    return;
  }

  unsigned long durationMs = (unsigned long)(ounces * 4000.0f);
  if (durationMs == 0) {
    durationMs = 1;
  }

  unsigned long now = millis();
  unsigned long startAt = pumpRunning[pumpId] ? pumpStopAt[pumpId] : now;
  pumpStopAt[pumpId] = startAt + durationMs;
  pumpRunning[pumpId] = true;
  digitalWrite(PUMP_PINS[pumpId], HIGH);

  Serial.printf("[PUMP] Pump %d GPIO %u ON for %.2f oz (%lu ms)\n",
                pumpId, PUMP_PINS[pumpId], ounces, durationMs);
}

void updatePumps() {
  unsigned long now = millis();

  for (uint8_t pumpId = 1; pumpId <= PUMP_COUNT; pumpId++) {
    if (pumpRunning[pumpId] && (long)(now - pumpStopAt[pumpId]) >= 0) {
      digitalWrite(PUMP_PINS[pumpId], LOW);
      pumpRunning[pumpId] = false;
      Serial.printf("[PUMP] Pump %d GPIO %u OFF\n", pumpId, PUMP_PINS[pumpId]);
    }
  }
}

void handleCommand(JsonDocument &doc) {
  const char *action = doc["action"] | "";

  if (strcmp(action, "pour") == 0) {
    int pumpId = doc["pump"] | 0;
    float ounces = doc["oz"] | 0.0f;
    startPump(pumpId, ounces);
    return;
  }

  if (strcmp(action, "serve") == 0) {
    JsonArrayConst items = doc["items"].as<JsonArrayConst>();
    if (items.isNull()) {
      Serial.println("[JSON] Serve command is missing its items array.");
      return;
    }

    for (JsonObjectConst item : items) {
      int pumpId = item["pump"] | 0;
      float ounces = item["oz"] | 0.0f;
      startPump(pumpId, ounces);
    }
    return;
  }

  Serial.printf("[JSON] Unsupported action: %s\n", action);
}

void printClientIP(IPAddress ip) {
  Serial.print("Client connected from: ");
  Serial.println(ip);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;

    case WStype_CONNECTED:
      printClientIP(webSocket.remoteIP(num));
      Serial.printf("[WS] Client #%u connected to WebSocket server\n", num);
      break;

    case WStype_TEXT: {
      Serial.println("--------------------------------------------------");
      Serial.println("[WS] Raw message received:");
      Serial.write(payload, length);
      Serial.println();
      Serial.println("--------------------------------------------------");

      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, payload, length);

      if (err) {
        Serial.print("[JSON] Failed to parse message: ");
        Serial.println(err.c_str());
        break;
      }

      Serial.println("[JSON] Parsed payload:");
      serializeJsonPretty(doc, Serial);
      Serial.println();

      if (strcmp(doc["action"] | "", "serve") == 0) {
        float total_oz = totalOuncesInServePayload(doc);

        if (total_oz > MAX_DRINK_OUNCES) {
          Serial.printf("[SAFE] Rejected oversized serve command: %.1f oz exceeds %.1f oz limit\n", total_oz, MAX_DRINK_OUNCES);
          webSocket.sendTXT(num, "{\"status\":\"error\",\"reason\":\"drink_overflow\"}");
          break;
        }
      }

      handleCommand(doc);

      // Optional: send a simple acknowledgment back to the sender.
      webSocket.sendTXT(num, "{\"status\":\"ok\",\"received\":true}");
      break;
    }

    case WStype_BIN:
      Serial.printf("[WS] Binary data received from client #%u (%u bytes)\n", num, (unsigned int)length);
      break;

    default:
      Serial.printf("[WS] Event type: %d\n", (int)type);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting ESP32 WebSocket server...");

  for (uint8_t pumpId = 1; pumpId <= PUMP_COUNT; pumpId++) {
    pinMode(PUMP_PINS[pumpId], OUTPUT);
    digitalWrite(PUMP_PINS[pumpId], LOW);
  }

  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (!apStarted) {
    Serial.println("[WiFi] Failed to start AP");
    return;
  }

  Serial.printf("[WiFi] AP SSID: %s\n", AP_SSID);
  Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("[WiFi] Waiting for client connection...");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("[WS] Server started on ws://192.168.4.1/ws");
}

void loop() {
  webSocket.loop();
  updatePumps();
}

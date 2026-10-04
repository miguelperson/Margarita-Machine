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
          Serial.printf("[SAFE] Rejected oversized serve command: %.1f oz exceeds %0.1f oz limit\n", total_oz, MAX_DRINK_OUNCES);
          webSocket.sendTXT(num, "{\"status\":\"error\",\"reason\":\"drink_overflow\"}");
          break;
        }
      }

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
}

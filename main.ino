#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include "landing_page.h"

const char *AP_SSID = "MargaritaMachine";
const float MAX_DRINK_OUNCES = 8.0f;

WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;

struct QueueItem {
  String id;
  String name;
  float totalOz = 0.0f;
  String payload;
};

std::vector<QueueItem> queueItems;
bool dispenseBusy = false;

String escapeHtml(const String &input) {
  String output;
  output.reserve(input.length());

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '&': output += "&amp;"; break;
      case '<': output += "&lt;"; break;
      case '>': output += "&gt;"; break;
      case '"': output += "&quot;"; break;
      default: output += c; break;
    }
  }

  return output;
}

bool isDrinkWithinLimit(float ounces) {
  return ounces > 0.0f && ounces <= MAX_DRINK_OUNCES;
}

String buildQueueStatusJson() {
  DynamicJsonDocument doc(2048);
  doc["status"] = "ok";
  doc["dispenseBusy"] = dispenseBusy;
  doc["maxOunces"] = MAX_DRINK_OUNCES;
  doc["queueLength"] = queueItems.size();

  JsonArray queue = doc.createNestedArray("queue");
  for (const auto &item : queueItems) {
    JsonObject queueItem = queue.createNestedObject();
    queueItem["id"] = item.id;
    queueItem["name"] = item.name;
    queueItem["totalOz"] = item.totalOz;
    queueItem["payload"] = item.payload;
  }

  String response;
  serializeJson(doc, response);
  return response;
}

bool queueHasItemWithId(const String &targetId) {
  for (const auto &item : queueItems) {
    if (item.id == targetId) {
      return true;
    }
  }
  return false;
}

QueueItem *findQueueItem(const String &targetId) {
  for (auto &item : queueItems) {
    if (item.id == targetId) {
      return &item;
    }
  }
  return nullptr;
}

void handleRoot() {
  server.send(200, "text/html", buildLandingPage());
}

void handleNotFound() {
  String requestPath = server.uri();

  if (requestPath.startsWith("/api/")) {
    server.send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
    return;
  }

  if (server.method() == HTTP_GET) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", buildLandingPage());
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

void handleStatus() {
  server.send(200, "application/json", buildQueueStatusJson());
}

void handleAddDrink() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"status\":\"error\",\"reason\":\"method_not_allowed\"}");
    return;
  }

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_body\"}");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
    return;
  }

  String name = doc["name"] | "Custom Drink";
  float totalOz = 0.0f;

  if (doc.containsKey("items") && doc["items"].is<JsonArray>()) {
    JsonArray items = doc["items"].as<JsonArray>();
    for (JsonVariant item : items) {
      if (item["oz"].is<float>() || item["oz"].is<int>()) {
        totalOz += item["oz"].as<float>();
      }
    }
  } else if (doc["oz"].is<float>() || doc["oz"].is<int>()) {
    totalOz = doc["oz"].as<float>();
  }

  if (!isDrinkWithinLimit(totalOz)) {
    server.send(400, "application/json", "{\"status\":\"error\",\"reason\":\"drink_overflow\"}");
    return;
  }

  QueueItem item;
  item.id = String(millis());
  item.name = name;
  item.totalOz = totalOz;
  item.payload = body;
  queueItems.push_back(item);

  server.send(200, "application/json", "{\"status\":\"ok\",\"queued\":true}");
}

void handleDispense() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"status\":\"error\",\"reason\":\"method_not_allowed\"}");
    return;
  }

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_body\"}");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
    return;
  }

  String targetId = doc["id"] | "";
  if (targetId.length() == 0 || !queueHasItemWithId(targetId)) {
    server.send(404, "application/json", "{\"status\":\"error\",\"reason\":\"item_not_found\"}");
    return;
  }

  if (dispenseBusy) {
    server.send(409, "application/json", "{\"status\":\"error\",\"reason\":\"dispense_busy\"}");
    return;
  }

  QueueItem *item = findQueueItem(targetId);
  if (item == nullptr) {
    server.send(404, "application/json", "{\"status\":\"error\",\"reason\":\"item_not_found\"}");
    return;
  }

  dispenseBusy = true;
  Serial.printf("[DISPENSE] Starting queue item %s (%s, %.2f oz)\n", item->id.c_str(), item->name.c_str(), item->totalOz);

  // Next step: this is where the pump sequence will be executed.
  // For Step 1, we maintain the queue state and lock the dispenser so that other
  // requests cannot race into the same dispensing window.

  // Remove the item after the active dispense operation is complete in a later step.
  queueItems.erase(std::remove_if(queueItems.begin(), queueItems.end(),
                                  [&](const QueueItem &queuedItem) {
                                    return queuedItem.id == targetId;
                                  }), queueItems.end());

  // Placeholder unlock: we intentionally keep the lock close to the real dispense flow,
  // which will be completed in the next step.
  dispenseBusy = false;

  server.send(200, "application/json", "{\"status\":\"ok\",\"dispensed\":true}");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[ESP32] Starting Margarita Machine access point...");

  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(AP_SSID, "", 1, false, 4);

  if (!apStarted) {
    Serial.println("[ESP32] Failed to start Wi-Fi AP");
    return;
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[ESP32] AP SSID: %s\n", AP_SSID);
  Serial.printf("[ESP32] IP: %s\n", apIP.toString().c_str());

  dnsServer.start(DNS_PORT, "*", apIP);

  if (MDNS.begin("margarita")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[ESP32] mDNS available at http://margarita.local");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/add", HTTP_POST, handleAddDrink);
  server.on("/api/dispense", HTTP_POST, handleDispense);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[ESP32] Web server started on port 80");
  Serial.println("[ESP32] Captive portal enabled; open http://margarita.com");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}


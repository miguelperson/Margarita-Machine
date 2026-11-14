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

String buildLandingPage() {
  String page = R"HTML(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Margarita Machine</title>
    <style>
      :root {
        --bg: #111827;
        --panel: #1f2937;
        --panel-alt: #374151;
        --accent: #f59e0b;
        --text: #f9fafb;
        --muted: #d1d5db;
        --danger: #ef4444;
        --success: #22c55e;
      }

      * { box-sizing: border-box; }
      body {
        margin: 0;
        font-family: Arial, sans-serif;
        background: var(--bg);
        color: var(--text);
      }
      .app {
        max-width: 760px;
        margin: 0 auto;
        padding: 24px 16px 48px;
      }
      .panel {
        background: var(--panel);
        border-radius: 16px;
        padding: 20px;
        margin-bottom: 18px;
        box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
      }
      h1, h2, h3 { margin-top: 0; }
      .drink-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
        gap: 12px;
      }
      .drink-btn, .queue-btn, .custom-btn, .builder-btn, .delete-btn {
        border: none;
        border-radius: 12px;
        padding: 16px 12px;
        font-size: 1rem;
        font-weight: bold;
        background: var(--panel-alt);
        color: var(--text);
        cursor: pointer;
      }
      .builder-btn {
        padding: 12px 10px;
        background: rgba(245, 158, 11, 0.2);
      }
      .delete-btn {
        background: rgba(239, 68, 68, 0.15);
        color: var(--text);
        padding: 10px 12px;
        margin-left: 8px;
      }
      .custom-btn {
        width: 100%;
        background: var(--accent);
        color: #111827;
      }
      .drink-btn:hover, .queue-btn:hover, .custom-btn:hover, .builder-btn:hover, .delete-btn:hover { filter: brightness(1.08); }
      .custom-builder {
        display: grid;
        gap: 12px;
      }
      .builder-row {
        display: grid;
        grid-template-columns: minmax(140px, 1.3fr) minmax(90px, 0.6fr) auto;
        gap: 10px;
        align-items: end;
      }
      .field {
        display: flex;
        flex-direction: column;
        gap: 6px;
      }
      .field label {
        font-size: 0.8rem;
        color: var(--muted);
      }
      select, input {
        width: 100%;
        padding: 10px 12px;
        border-radius: 10px;
        border: 1px solid rgba(255,255,255,0.08);
        background: rgba(17, 24, 39, 0.6);
        color: var(--text);
        font-size: 1rem;
      }
      .queue-item {
        display: flex;
        justify-content: space-between;
        align-items: flex-start;
        gap: 12px;
        background: rgba(255,255,255,0.03);
        border-radius: 12px;
        padding: 12px 14px;
        margin-top: 10px;
      }
      .queue-meta {
        display: flex;
        flex-direction: column;
        gap: 6px;
        flex: 1;
      }
      .ingredient-list {
        display: flex;
        flex-wrap: wrap;
        gap: 6px;
      }
      .ingredient-pill {
        display: inline-block;
        padding: 4px 8px;
        border-radius: 999px;
        background: rgba(245, 158, 11, 0.2);
        color: var(--text);
        font-size: 0.8rem;
      }
      .small { color: var(--muted); font-size: 0.9rem; }
      .status { color: var(--success); font-weight: bold; }
      .danger { color: var(--danger); }
      @media (max-width: 560px) {
        .builder-row { grid-template-columns: 1fr; }
      }
    </style>
  </head>
  <body>
    <div class="app">
      <div class="panel">
        <h1>Margarita Machine</h1>
        <p class="small">Open Wi-Fi access point. Queue drinks, then dispense one at a time.</p>
      </div>

      <div class="panel">
        <h2>Preset Drinks</h2>
        <div class="drink-grid">
          <button class="drink-btn" data-preset="Classic Margarita" data-oz="4">Classic Margarita</button>
          <button class="drink-btn" data-preset="Tequila Sunrise" data-oz="5">Tequila Sunrise</button>
          <button class="drink-btn" data-preset="Lime Cooler" data-oz="6">Lime Cooler</button>
          <button class="drink-btn" data-preset="Frozen Punch" data-oz="7.5">Frozen Punch</button>
        </div>
      </div>

      <div class="panel">
        <h2>Custom Drink</h2>
        <div class="custom-builder">
          <div class="builder-row">
            <div class="field">
              <label for="liquor-select">Liquor</label>
              <select id="liquor-select">
                <option value="Tequila">Tequila</option>
                <option value="Rum">Rum</option>
                <option value="Whisky">Whisky</option>
                <option value="Vodka">Vodka</option>
              </select>
            </div>
            <div class="field">
              <label for="liquor-oz">Ounces</label>
              <select id="liquor-oz">
                <option value="0.5">0.5</option>
                <option value="1.0">1.0</option>
                <option value="1.5" selected>1.5</option>
                <option value="2.0">2.0</option>
                <option value="2.5">2.5</option>
                <option value="3.0">3.0</option>
                <option value="3.5">3.5</option>
                <option value="4.0">4.0</option>
              </select>
            </div>
            <button id="add-liquor-btn" class="builder-btn" type="button">Add liquor</button>
          </div>

          <div class="builder-row">
            <div class="field">
              <label for="mixer-select">Mixer</label>
              <select id="mixer-select">
                <option value="Club soda">Club soda</option>
                <option value="Coke">Coke</option>
                <option value="Cranberry juice">Cranberry juice</option>
                <option value="Orange juice">Orange juice</option>
                <option value="Angry Orchard">Angry Orchard</option>
              </select>
            </div>
            <div class="field">
              <label for="mixer-oz">Ounces</label>
              <select id="mixer-oz">
                <option value="0.5">0.5</option>
                <option value="1.0">1.0</option>
                <option value="1.5">1.5</option>
                <option value="2.0" selected>2.0</option>
                <option value="2.5">2.5</option>
                <option value="3.0">3.0</option>
                <option value="3.5">3.5</option>
                <option value="4.0">4.0</option>
              </select>
            </div>
            <button id="add-mixer-btn" class="builder-btn" type="button">Add mixer</button>
          </div>

          <div id="custom-ingredient-list" class="ingredient-list"></div>
          <button id="queue-custom-drink-btn" class="custom-btn" type="button">Add custom drink to queue</button>
        </div>
      </div>

      <div class="panel">
        <h2>Queue</h2>
        <div id="queue"></div>
      </div>
    </div>

    <script>
      const MAX_OUNCES = 8.0;
      let queue = [];
      let customDrinkNumber = 1;
      let customDraftItems = [];

      function renderCustomDraft() {
        const listEl = document.getElementById('custom-ingredient-list');
        if (!customDraftItems.length) {
          listEl.innerHTML = '<div class="small">No ingredients selected yet.</div>';
          return;
        }

        listEl.innerHTML = customDraftItems
          .map((item) => `<span class="ingredient-pill">${item.name} · ${Number(item.oz).toFixed(1)} oz</span>`)
          .join('');
      }

      function parseIngredients(item) {
        if (!item || !item.payload) {
          return [{ name: item && item.name ? item.name : 'Custom blend', oz: Number(item && item.totalOz ? item.totalOz : 0) }];
        }

        try {
          const payload = JSON.parse(item.payload);
          const items = Array.isArray(payload.items) ? payload.items : [];
          if (items.length) {
            return items.map((entry) => ({
              name: entry.name || 'Ingredient',
              oz: Number(entry.oz) || 0
            })).filter((entry) => entry.oz > 0);
          }
        } catch (error) {
          // Fall back to a single label if the payload is not JSON.
        }

        return [{ name: item.name || 'Custom blend', oz: Number(item.totalOz) || 0 }];
      }

      function updateQueueDisplay() {
        const queueEl = document.getElementById('queue');
        if (!queue.length) {
          queueEl.innerHTML = '<div class="small">No drinks in the queue.</div>';
          return;
        }

        queueEl.innerHTML = queue.map((item) => {
          const ingredients = parseIngredients(item);
          const ingredientMarkup = ingredients.map((entry) => `
            <span class="ingredient-pill">${entry.name} · ${Number(entry.oz).toFixed(1)} oz</span>
          `).join('');

          return `
            <div class="queue-item">
              <div class="queue-meta">
                <strong>${item.name}</strong>
                <div class="ingredient-list">${ingredientMarkup}</div>
                <span class="small">${item.totalOz.toFixed(1)} oz total</span>
              </div>
              <div>
                <button class="queue-btn" data-id="${item.id}">Dispense</button>
                <button class="delete-btn" data-id="${item.id}">Delete</button>
              </div>
            </div>
          `;
        }).join('');
      }

      function apiFetch(path, options = {}) {
        return fetch(path, {
          headers: { 'Content-Type': 'application/json' },
          ...options
        }).then(async (response) => {
          const text = await response.text();
          try {
            return JSON.parse(text);
          } catch (error) {
            return { status: 'error', raw: text };
          }
        });
      }

      async function refreshQueue() {
        try {
          const payload = await apiFetch('/api/status');
          if (payload && payload.queue) {
            queue = payload.queue;
            updateQueueDisplay();
            return;
          }
        } catch (error) {
          // If the ESP32 endpoint is unavailable, leave the queue as-is.
        }
      }

      function addCustomIngredient(type, selectId, ounceId) {
        const ingredientName = document.getElementById(selectId).value;
        const ounceValue = Number(document.getElementById(ounceId).value);

        if (!ingredientName || !Number.isFinite(ounceValue) || ounceValue <= 0) {
          alert('Pick an ingredient and set a valid ounce amount.');
          return;
        }

        customDraftItems.push({ name: ingredientName, oz: ounceValue, type });
        renderCustomDraft();
        document.getElementById(selectId).selectedIndex = 0;
        document.getElementById(ounceId).value = type === 'liquor' ? '1.5' : '2.0';
      }

      document.querySelectorAll('.drink-btn').forEach((button) => {
        button.addEventListener('click', async () => {
          const payload = {
            name: button.dataset.preset,
            items: [{ name: button.dataset.preset, oz: Number(button.dataset.oz) }]
          };

          const result = await apiFetch('/api/add', {
            method: 'POST',
            body: JSON.stringify(payload)
          });

          if (result.status === 'ok') {
            await refreshQueue();
          } else {
            alert(result.reason || 'Unable to add drink to the queue.');
          }
        });
      });

      document.getElementById('add-liquor-btn').addEventListener('click', () => {
        addCustomIngredient('liquor', 'liquor-select', 'liquor-oz');
      });

      document.getElementById('add-mixer-btn').addEventListener('click', () => {
        addCustomIngredient('mixer', 'mixer-select', 'mixer-oz');
      });

      document.getElementById('queue-custom-drink-btn').addEventListener('click', async () => {
        if (!customDraftItems.length) {
          alert('Add at least one liquor or mixer before sending the custom drink to the queue.');
          return;
        }

        const name = `Custom drink #${customDrinkNumber}`;
        customDrinkNumber += 1;

        const result = await apiFetch('/api/add', {
          method: 'POST',
          body: JSON.stringify({
            name,
            items: customDraftItems.map(({ name, oz }) => ({ name, oz }))
          })
        });

        if (result.status === 'ok') {
          customDraftItems = [];
          renderCustomDraft();
          await refreshQueue();
        } else {
          alert(result.reason || 'Unable to add the custom drink.');
        }
      });

      document.addEventListener('click', async (event) => {
        const deleteButton = event.target.closest('.delete-btn');
        if (deleteButton) {
          const result = await apiFetch('/api/remove', {
            method: 'POST',
            body: JSON.stringify({ id: deleteButton.dataset.id })
          });

          if (result.status === 'ok') {
            await refreshQueue();
          } else {
            alert(result.reason || 'Unable to delete this item from the queue.');
          }
          return;
        }

        const button = event.target.closest('.queue-btn');
        if (!button) return;

        const result = await apiFetch('/api/dispense', {
          method: 'POST',
          body: JSON.stringify({ id: button.dataset.id })
        });

        if (result.status === 'ok') {
          await refreshQueue();
        } else {
          alert(result.reason || 'This drink cannot be dispensed right now.');
        }
      });

      renderCustomDraft();
      refreshQueue();
    </script>
  </body>
</html>
)HTML";

  return page;
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

void handleRemoveDrink() {
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

  queueItems.erase(std::remove_if(queueItems.begin(), queueItems.end(),
                                  [&](const QueueItem &queuedItem) {
                                    return queuedItem.id == targetId;
                                  }), queueItems.end());

  server.send(200, "application/json", "{\"status\":\"ok\",\"removed\":true}");
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
  server.on("/api/remove", HTTP_POST, handleRemoveDrink);
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


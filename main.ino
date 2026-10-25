#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

const char *AP_SSID = "MargaritaMachine";
const float MAX_DRINK_OUNCES = 8.0f;

WebServer server(80);

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
      .drink-btn, .queue-btn {
        border: none;
        border-radius: 12px;
        padding: 16px 12px;
        font-size: 1rem;
        font-weight: bold;
        background: var(--panel-alt);
        color: var(--text);
        cursor: pointer;
      }
      .drink-btn:hover, .queue-btn:hover { filter: brightness(1.08); }
      .custom-form {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 12px;
        margin-top: 12px;
      }
      .custom-form input, .custom-form button {
        width: 100%;
        padding: 12px;
        border: 1px solid rgba(255,255,255,0.1);
        border-radius: 10px;
        font-size: 1rem;
      }
      .custom-form button {
        background: var(--accent);
        color: #111827;
        font-weight: bold;
        border: none;
        cursor: pointer;
      }
      .queue-item {
        display: flex;
        justify-content: space-between;
        align-items: center;
        background: rgba(255,255,255,0.03);
        border-radius: 12px;
        padding: 12px 14px;
        margin-top: 10px;
      }
      .queue-meta { display: flex; flex-direction: column; gap: 4px; }
      .small { color: var(--muted); font-size: 0.9rem; }
      .status { color: var(--success); font-weight: bold; }
      .danger { color: var(--danger); }
      @media (max-width: 560px) {
        .custom-form { grid-template-columns: 1fr; }
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
        <form id="custom-form" class="custom-form">
          <input id="drink-name" type="text" placeholder="Drink name" maxlength="48" />
          <input id="drink-oz" type="number" step="0.5" min="0.5" max="8" value="4" />
          <button type="submit">Add to Queue</button>
        </form>
      </div>

      <div class="panel">
        <h2>Queue</h2>
        <div id="queue"></div>
      </div>
    </div>

    <script>
      const MAX_OUNCES = 8.0;
      let queue = [];

      function updateQueueDisplay() {
        const queueEl = document.getElementById('queue');
        if (!queue.length) {
          queueEl.innerHTML = '<div class="small">No drinks in the queue.</div>';
          return;
        }

        queueEl.innerHTML = queue.map(item => `
          <div class="queue-item">
            <div class="queue-meta">
              <strong>${item.name}</strong>
              <span class="small">${item.totalOz.toFixed(1)} oz</span>
            </div>
            <button class="queue-btn" data-id="${item.id}">Dispense</button>
          </div>
        `).join('');
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
        const payload = await apiFetch('/api/status');
        if (payload && payload.queue) {
          queue = payload.queue;
          updateQueueDisplay();
        }
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

      document.getElementById('custom-form').addEventListener('submit', async (event) => {
        event.preventDefault();
        const name = document.getElementById('drink-name').value.trim() || 'Custom Drink';
        const oz = Number(document.getElementById('drink-oz').value);

        if (!Number.isFinite(oz) || oz <= 0 || oz > MAX_OUNCES) {
          alert('Custom drink must be between 0.5 and 8.0 oz.');
          return;
        }

        const result = await apiFetch('/api/add', {
          method: 'POST',
          body: JSON.stringify({
            name,
            items: [{ name, oz }]
          })
        });

        if (result.status === 'ok') {
          document.getElementById('custom-form').reset();
          document.getElementById('drink-oz').value = '4';
          await refreshQueue();
        } else {
          alert(result.reason || 'Unable to add the custom drink.');
        }
      });

      document.addEventListener('click', async (event) => {
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

  Serial.printf("[ESP32] AP SSID: %s\n", AP_SSID);
  Serial.printf("[ESP32] IP: %s\n", WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/add", HTTP_POST, handleAddDrink);
  server.on("/api/dispense", HTTP_POST, handleDispense);

  server.begin();
  Serial.println("[ESP32] Web server started on port 80");
}

void loop() {
  server.handleClient();
}

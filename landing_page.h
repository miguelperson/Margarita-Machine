#ifndef LANDING_PAGE_H
#define LANDING_PAGE_H

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
    <div class="app" tabindex="-1">
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
      let queueSignature = '';

      function escapeHtml(value) {
        const element = document.createElement('div');
        element.textContent = String(value);
        return element.innerHTML;
      }

      function updateQueueDisplay() {
        const signature = JSON.stringify(queue.map(item => ({
          id: item.id,
          name: item.name,
          totalOz: item.totalOz
        })));

        if (signature === queueSignature) {
          return;
        }
        queueSignature = signature;

        const queueEl = document.getElementById('queue');
        if (!queue.length) {
          queueEl.innerHTML = '<div class="small">No drinks in the queue.</div>';
          return;
        }

        queueEl.innerHTML = queue.map(item => `
          <div class="queue-item">
            <div class="queue-meta">
              <strong>${escapeHtml(item.name)}</strong>
              <span class="small">${escapeHtml(Number(item.totalOz).toFixed(1))} oz</span>
            </div>
            <button class="queue-btn" data-id="${escapeHtml(item.id)}">Dispense</button>
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
        try {
          const payload = await apiFetch('/api/status');
          if (payload && payload.queue) {
            queue = payload.queue;
            updateQueueDisplay();
          }
        } catch (error) {
          console.error('Unable to refresh the queue:', error);
        }
      }

      // Helper to anchor focus away from inputs
      function anchorFocus() {
        if (document.activeElement && document.activeElement.tagName === 'INPUT') {
          return; // Don't steal focus if the user is actively typing in a custom box
        }
        const appEl = document.querySelector('.app');
        if (appEl) {
          appEl.focus({ preventScroll: true });
        }
      }

      // Preset drink buttons
      document.querySelectorAll('.drink-btn').forEach((button) => {
        button.addEventListener('click', async () => {
          button.blur();
          anchorFocus();

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
            anchorFocus();
          } else {
            alert(result.reason || 'Unable to add drink to the queue.');
          }
        });
      });

      // Custom form submission
      document.getElementById('custom-form').addEventListener('submit', async (event) => {
        event.preventDefault();
        document.activeElement.blur();
        anchorFocus();

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
          anchorFocus();
        } else {
          alert(result.reason || 'Unable to add the custom drink.');
        }
      });

      // Dispense button listener (delegated)
      document.addEventListener('click', async (event) => {
        const button = event.target.closest('.queue-btn');
        if (!button) return;

        button.blur();
        anchorFocus();

        const result = await apiFetch('/api/dispense', {
          method: 'POST',
          body: JSON.stringify({ id: button.dataset.id })
        });

        if (result.status === 'ok') {
          await refreshQueue();
          anchorFocus();
        } else {
          alert(result.reason || 'This drink cannot be dispensed right now.');
        }
      });

      refreshQueue();
      window.setInterval(refreshQueue, 1000);

      document.addEventListener('visibilitychange', () => {
        if (!document.hidden) {
          refreshQueue();
        }
      });
    </script>
  </body>
</html>
)HTML";

  return page;
}

#endif


// Generic model UI renderer shared by admin.html and model2.html.
// Expects certain DOM ids to exist (same as admin.html):
// - cardsContainer, graphsContainer, wsDebugLog, wsBadge
// Optional: Materialize toast if M is present.

(function () {
  const topicState = {};
  let ws = null;
  let wsDebugEnabled = false;

  const dirtyFields = new Set(); // Set of "topic\u0000field"
  let renderScheduled = false;

  const graphCharts = {}; // { graph_name: { chart, datasets: { label: dataset }, maxCount } }

  const byId = (id) => document.getElementById(id);
  const formatTopic = (t) => String(t || '').replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());

  const dirtyKey = (topic, field) => `${topic}\u0000${field}`;

  function escapeAttrValue(value) {
    return String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  }

  function markDirty(topic, field, isDirty) {
    const key = dirtyKey(topic, field);
    if (isDirty) dirtyFields.add(key);
    else dirtyFields.delete(key);
  }

  function isDirty(topic, field) {
    return dirtyFields.has(dirtyKey(topic, field));
  }

  function clearDirtyForTopic(topic) {
    Array.from(dirtyFields).forEach((k) => {
      if (k.startsWith(`${topic}\u0000`)) dirtyFields.delete(k);
    });
  }

  function scheduleRender() {
    if (renderScheduled) return;
    renderScheduled = true;

    const runner = () => {
      renderScheduled = false;
      renderTopics();
    };

    if (typeof window.requestAnimationFrame === 'function') window.requestAnimationFrame(runner);
    else setTimeout(runner, 16);
  }

  function toast(html, classes) {
    try {
      if (window.M && typeof window.M.toast === 'function') {
        window.M.toast({ html, classes });
      }
    } catch (_) {}
  }

  function unwrapField(raw) {
    if (raw && typeof raw === 'object' && !Array.isArray(raw)) {
      if ('items' in raw && Array.isArray(raw.items)) {
        return { kind: 'list', value: raw.items, raw };
      }
      if ('type' in raw && raw.type === 'button') {
        return { kind: 'button', value: raw.id, raw };
      }
      if ('type' in raw && raw.type === 'secret') {
        return { kind: 'string', value: '', raw };
      }
      if ('value' in raw) return unwrapField(raw.value);
    }
    if (Array.isArray(raw)) return { kind: 'list', value: raw, raw };
    if (typeof raw === 'boolean') return { kind: 'bool', value: raw, raw };
    if (typeof raw === 'number') return { kind: 'number', value: raw, raw };
    return { kind: 'string', value: raw == null ? '' : String(raw), raw };
  }

  function createCard(topic) {
    const card = document.createElement('div');
    card.className = 'card';
    card.dataset.topic = topic;

    const content = document.createElement('div');
    content.className = 'card-content';
    const title = document.createElement('span');
    title.className = 'card-title';
    title.textContent = formatTopic(topic);
    content.appendChild(title);

    const empty = document.createElement('p');
    empty.className = 'topic-empty';
    empty.style.display = 'none';
    empty.textContent = 'Keine Daten verfügbar';
    content.appendChild(empty);

    const actions = document.createElement('div');
    actions.className = 'card-action';
    const saveBtn = document.createElement('a');
    saveBtn.className = 'btn blue';
    saveBtn.textContent = 'Speichern';
    saveBtn.onclick = () => submitTopicUpdate(topic, card);
    actions.appendChild(saveBtn);

    card.appendChild(content);
    card.appendChild(actions);
    return card;
  }

  function ensureRow(cardContentEl, topic, key) {
    const selectorKey = escapeAttrValue(key);
    let row = cardContentEl.querySelector(`[data-row="${selectorKey}"]`);
    if (!row) {
      row = document.createElement('div');
      row.className = 'setting-row';
      row.dataset.row = key;
      cardContentEl.appendChild(row);
    }
    return row;
  }

  function updateRow(row, topic, key, info) {
    // Ensure row matches desired kind; if kind changes, rebuild this row.
    const isButtonKey = String(key).toLowerCase().includes('button');
    const desiredKind = info.kind === 'button' || isButtonKey ? 'button' : info.kind;
    const currentKind = row.dataset.rowKind;
    const mustRebuild = currentKind !== desiredKind;

    if (mustRebuild) {
      row.innerHTML = '';
      row.dataset.rowKind = desiredKind;
    }

    if (desiredKind === 'button') {
      let btn = row.querySelector('a.btn');
      if (!btn) {
        btn = document.createElement('a');
        btn.className = 'btn waves-effect waves-light orange';
        btn.style.margin = '0.5rem 0';
        row.appendChild(btn);
      }
      btn.textContent = '▶ ' + String(key).replace(/_/g, ' ');
      btn.onclick = () => triggerButton(topic, key);
      return;
    }

    if (desiredKind === 'list') {
      let label = row.querySelector('label');
      if (!label) {
        label = document.createElement('label');
        row.appendChild(label);
      }
      label.textContent = key;

      let ul = row.querySelector('ul.value-list');
      if (!ul) {
        ul = document.createElement('ul');
        ul.className = 'value-list';
        row.appendChild(ul);
      }

      ul.innerHTML = '';
      (info.value || []).forEach((item) => {
        const li = document.createElement('li');
        li.textContent = item;
        ul.appendChild(li);
      });
      return;
    }

    if (desiredKind === 'bool') {
      let label = row.querySelector('label');
      if (!label) {
        label = document.createElement('label');
        row.appendChild(label);
      }
      label.textContent = key;

      let input = row.querySelector('input[type="checkbox"]');
      if (!input) {
        const toggle = document.createElement('label');
        input = document.createElement('input');
        input.type = 'checkbox';
        input.dataset.field = key;
        input.dataset.kind = 'bool';
        input.dataset.topic = topic;
        input.addEventListener('change', () => markDirty(topic, key, true));
        const span = document.createElement('span');
        toggle.appendChild(input);
        toggle.appendChild(span);
        row.appendChild(toggle);
      }

      if (!isDirty(topic, key)) {
        input.checked = !!info.value;
      }
      return;
    }

    // string/number
    let label = row.querySelector('label');
    if (!label) {
      label = document.createElement('label');
      row.appendChild(label);
    }
    label.textContent = key;

    let input = row.querySelector('input');
    if (!input) {
      input = document.createElement('input');
      input.className = 'browser-default';
      input.dataset.field = key;
      input.dataset.kind = desiredKind;
      input.dataset.topic = topic;
      input.addEventListener('input', () => markDirty(topic, key, true));
      row.appendChild(input);
    }

    const newType = desiredKind === 'number' ? 'number' : 'text';
    if (input.type !== newType) input.type = newType;

    // Never overwrite while the user is editing that field.
    if (!isDirty(topic, key)) {
      const serverValue = info.value;
      if (input.value !== serverValue) input.value = serverValue;
    }
  }

  function updateCard(topic, card, data) {
    const content = card.querySelector('.card-content');
    if (!content) return;

    const title = content.querySelector('.card-title');
    if (title) title.textContent = formatTopic(topic);

    const keys = Object.keys(data || {});
    const emptyEl = content.querySelector('.topic-empty');
    if (emptyEl) emptyEl.style.display = keys.length === 0 ? '' : 'none';

    const desiredRows = new Set();
    keys.forEach((key) => {
      const info = unwrapField((data || {})[key]);

      // graph_xy_ring gets rendered inline in the owning topic card
      if (info.raw && info.raw.type === 'graph_xy_ring') {
        desiredRows.add(key);
        const row = ensureRow(content, topic, key);
        updateGraphRow(row, topic, key, info.raw);
        return;
      }

      desiredRows.add(key);
      const row = ensureRow(content, topic, key);
      updateRow(row, topic, key, info);
    });

    // Remove stale rows
    Array.from(content.querySelectorAll('.setting-row[data-row]')).forEach((row) => {
      const key = row.dataset.row;
      if (!desiredRows.has(key)) row.remove();
    });
  }

  function updateGraphRow(row, topic, key, graphData) {
    const currentKind = row.dataset.rowKind;
    if (currentKind !== 'graph') {
      row.innerHTML = '';
      row.dataset.rowKind = 'graph';
      row.style.flexDirection = 'column';
      row.style.alignItems = 'stretch';
    }

    let labelEl = row.querySelector('label');
    if (!labelEl) {
      labelEl = document.createElement('label');
      row.appendChild(labelEl);
    }
    labelEl.textContent = key;

    let host = row.querySelector('.graph-host');
    if (!host) {
      host = document.createElement('div');
      host.className = 'graph-host';
      host.style.marginTop = '0.35rem';
      host.style.position = 'relative';
      host.style.height = '220px';
      host.style.width = '100%';
      row.appendChild(host);
    }

    renderGraph(topic, key, graphData, host);
  }

  function renderTopics() {
    const container = byId('cardsContainer');
    if (!container) return;

    // Preserve the current focus so a re-order/rebuild doesn't break typing.
    let active = null;
    try {
      const el = document.activeElement;
      if (el && el.dataset && el.dataset.topic && el.dataset.field) {
        active = {
          topic: el.dataset.topic,
          field: el.dataset.field,
          selectionStart: typeof el.selectionStart === 'number' ? el.selectionStart : null,
          selectionEnd: typeof el.selectionEnd === 'number' ? el.selectionEnd : null
        };
      }
    } catch (_) {}

    const topics = Object.keys(topicState).sort();
    const keepTopics = new Set(topics);

    // Remove cards for topics that disappeared
    Array.from(container.querySelectorAll('.card[data-topic]')).forEach((card) => {
      const topic = card.dataset.topic;
      if (!keepTopics.has(topic)) card.remove();
    });

    topics.forEach((topic) => {
      const selectorTopic = escapeAttrValue(topic);
      let card = container.querySelector(`.card[data-topic="${selectorTopic}"]`);
      if (!card) card = createCard(topic);
      updateCard(topic, card, topicState[topic] || {});
      container.appendChild(card); // move into correct order
    });

    // Restore focus/caret if we had to rebuild any element.
    if (active) {
      const selectorTopic = escapeAttrValue(active.topic);
      const selectorField = escapeAttrValue(active.field);
      const el = container.querySelector(
        `.card[data-topic="${selectorTopic}"] [data-field="${selectorField}"]`
      );
      if (el && document.activeElement !== el) {
        try {
          el.focus({ preventScroll: true });
        } catch (_) {
          try {
            el.focus();
          } catch (_) {}
        }
        if (active.selectionStart != null && typeof el.setSelectionRange === 'function') {
          try {
            el.setSelectionRange(active.selectionStart, active.selectionEnd ?? active.selectionStart);
          } catch (_) {}
        }
      }
    }
  }

  function submitTopicUpdate(topic, cardEl) {
    const inputs = cardEl.querySelectorAll('[data-field]');
    const payload = {};
    inputs.forEach((el) => {
      const key = el.dataset.field;
      const kind = el.dataset.kind;
      if (kind === 'bool') {
        payload[key] = el.checked;
      } else if (kind === 'number') {
        const n = parseFloat(el.value);
        payload[key] = isNaN(n) ? 0 : n;
      } else {
        payload[key] = el.value;
      }
    });

    sendUpdate(topic, payload);
    clearDirtyForTopic(topic);
  }

  function sendUpdate(topic, data) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      toast('WebSocket nicht verbunden', 'red');
      return;
    }
    const msg = { topic, data };
    const payload = JSON.stringify(msg);
    logWsLine(payload, 'out');
    ws.send(payload);
    toast(`Sende Update für ${topic}...`, 'blue');
  }

  function triggerButton(topic, button) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      toast('WebSocket nicht verbunden', 'red');
      return;
    }
    const msg = { action: 'button_trigger', topic, button };
    const payload = JSON.stringify(msg);
    logWsLine(payload, 'out');
    ws.send(payload);
    toast(`Button "${button}" ausgelöst...`, 'blue');
  }

  function normalizeWsPayload(line) {
    if (typeof line !== 'string') return String(line);
    const s = line.trim();
    if (!s) return line;
    if (s[0] !== '{' && s[0] !== '[') return line;
    try {
      return JSON.stringify(JSON.parse(line));
    } catch (_) {
      return line;
    }
  }

  function logWsLine(line, dir) {
    if (!wsDebugEnabled) return;
    const debugLog = byId('wsDebugLog');
    if (!debugLog) return;
    const timestamp = new Date().toLocaleTimeString('de-DE');
    const arrow = dir === 'out' ? '→' : '←';
    const msg = normalizeWsPayload(line);
    debugLog.textContent += `[${timestamp}] ${arrow} ${msg}\n`;
    debugLog.scrollTop = debugLog.scrollHeight;
  }

  function handleWsMessageText(text) {
    logWsLine(text, 'in');

    try {
      const msg = JSON.parse(text);

      if (msg.topic === 'graph_point' && msg.data) {
        handleGraphPoint(msg.data);
        return;
      }

      if (!msg.topic) return;
      topicState[msg.topic] = msg.data || {};
      scheduleRender();
    } catch (e) {
      console.error('WS parse error', e);
    }
  }

  function connectWs(wsPath) {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const url = `${protocol}//${window.location.host}${wsPath}`;

    ws = new WebSocket(url);

    ws.onopen = () => {
      const badge = byId('wsBadge');
      if (badge) {
        badge.textContent = 'WS: verbunden';
        badge.className = 'chip connected';
      }
    };

    ws.onclose = () => {
      const badge = byId('wsBadge');
      if (badge) {
        badge.textContent = 'WS: getrennt';
        badge.className = 'chip disconnected';
      }
      // In demo mode, don't auto-reconnect.
      const isDemo = (() => {
        try {
          return new URLSearchParams(window.location.search).get('demo') === '1';
        } catch (_) {
          return false;
        }
      })();
      if (!isDemo) setTimeout(() => connectWs(wsPath), 2000);
    };

    ws.onerror = () => {
      const badge = byId('wsBadge');
      if (badge) {
        badge.textContent = 'WS: Fehler';
        badge.className = 'chip red lighten-4';
      }
    };

    ws.onmessage = (ev) => {
      const data = ev.data;
      if (typeof data === 'string') {
        handleWsMessageText(data);
        return;
      }

      // Some browsers deliver WS messages as Blob/ArrayBuffer.
      try {
        if (data instanceof Blob && typeof data.text === 'function') {
          data.text().then(handleWsMessageText).catch((e) => console.error('WS blob decode error', e));
          return;
        }
        if (data instanceof ArrayBuffer && typeof TextDecoder !== 'undefined') {
          handleWsMessageText(new TextDecoder().decode(data));
          return;
        }
      } catch (e) {
        console.error('WS decode error', e);
      }

      // Fallback
      logWsLine(String(data), 'in');
    };
  }

  function ensureGraph(graphName, maxCount, hostEl) {
    let graphContainer = byId(`graph-${graphName}`);
    if (!graphContainer) {
      graphContainer = document.createElement('div');
      graphContainer.id = `graph-${graphName}`;

      // Inline-first container (fits into the owning card).
      graphContainer.style.width = '100%';
      graphContainer.style.height = '100%';
      graphContainer.style.background = '#fff';
      graphContainer.style.border = '1px solid rgba(15, 23, 42, 0.08)';
      graphContainer.style.borderRadius = '12px';
      graphContainer.style.padding = '0.5rem';

      const canvas = document.createElement('canvas');
      canvas.id = `chart-${graphName}`;
      canvas.style.width = '100%';
      canvas.style.height = '100%';
      graphContainer.appendChild(canvas);

      const fallbackHost = byId('graphsContainer');
      (hostEl || fallbackHost || document.body).appendChild(graphContainer);

      if (!window.Chart) {
        console.warn('Chart.js not loaded; graph disabled');
        graphCharts[graphName] = { chart: null, datasets: {}, maxCount: maxCount || 60 };
        return graphCharts[graphName];
      }

      const ctx = canvas.getContext('2d');
      graphCharts[graphName] = {
        chart: new window.Chart(ctx, {
          type: 'line',
          data: { datasets: [] },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 200 },
            scales: {
              x: {
                type: 'linear',
                display: true,
                title: { display: true, text: 'Zeit' },
                ticks: {
                  callback: (value) => {
                    const n = typeof value === 'number' ? value : parseFloat(String(value));
                    if (!isFinite(n)) return String(value);

                    // Heuristic:
                    // - epoch-ms is ~1.7e12
                    // - epoch-s is ~1.7e9
                    // - uptime ms is usually < 1e9 for typical runtimes
                    let ms = n;
                    if (n >= 1e12) {
                      ms = n;
                      return new Date(ms).toLocaleTimeString('de-DE');
                    }
                    if (n >= 1e9) {
                      ms = n * 1000;
                      return new Date(ms).toLocaleTimeString('de-DE');
                    }

                    // uptime ms -> mm:ss
                    const totalSeconds = Math.floor(ms / 1000);
                    const minutes = Math.floor(totalSeconds / 60);
                    const seconds = totalSeconds % 60;
                    return `${minutes}:${String(seconds).padStart(2, '0')}`;
                  }
                }
              },
              y: { display: true, title: { display: true, text: 'Wert' }, beginAtZero: false }
            },
            plugins: { legend: { display: true, position: 'top' } }
          }
        }),
        datasets: {},
        maxCount: maxCount || 60
      };
    }

    // Keep chart DOM positioned where it belongs.
    if (hostEl && graphContainer.parentElement !== hostEl) {
      hostEl.appendChild(graphContainer);
    }

    return graphCharts[graphName];
  }

  function renderGraph(_topic, key, graphData, hostEl) {
    const graphName = graphData.graph || 'unknown';
    const label = graphData.label || key;
    const maxCount = graphData.max_count || 60;

    const graphInfo = ensureGraph(graphName, maxCount, hostEl);
    if (!graphInfo) return;
    if (graphData.max_count) graphInfo.maxCount = graphData.max_count;

    if (!graphInfo.datasets[label]) {
      const colors = ['#4caf50', '#2196f3', '#ff9800', '#e91e63', '#9c27b0', '#00bcd4'];
      const colorIndex = Object.keys(graphInfo.datasets).length % colors.length;

      graphInfo.datasets[label] = {
        label,
        data: [],
        borderColor: colors[colorIndex],
        backgroundColor: colors[colorIndex] + '20',
        borderWidth: 2,
        tension: 0.3,
        fill: true
      };

      if (graphInfo.chart) {
        graphInfo.chart.data.datasets.push(graphInfo.datasets[label]);
      }
    }

    // Initial values from ring-buffer snapshots.
    // Important: avoid resetting the chart on every topic refresh, otherwise
    // live points (graph_point) appear briefly then disappear.
    if (graphData.values && Array.isArray(graphData.values) && graphInfo.chart) {
      const dataset = graphInfo.datasets[label];
      const incomingCount = graphData.values.length;
      const currentCount = Array.isArray(dataset.data) ? dataset.data.length : 0;

      // Only apply snapshot if we have no data yet or server has more history than us.
      if (currentCount === 0 || incomingCount > currentCount) {
        dataset.data = [];

        graphData.values.forEach((point, idx) => {
          const x = point.x != null ? Number(point.x) : idx;
          dataset.data.push({ x, y: point.y });
        });

        graphInfo.chart.update();
      }
    }
  }

  function handleGraphPoint(data) {
    if (!data || !data.graph || !data.label) return;
    const graphName = data.graph;
    const label = data.label;

    const graphInfo = graphCharts[graphName];
    if (!graphInfo || !graphInfo.chart) return;

    const x = data.x != null ? Number(data.x) : Date.now();
    const y = data.y;

    if (!graphInfo.datasets[label]) return;

    graphInfo.datasets[label].data.push({ x, y });

    const maxCount = graphInfo.maxCount || 60;
    const oneDs = graphInfo.datasets[label];
    if (oneDs && Array.isArray(oneDs.data) && oneDs.data.length > maxCount) {
      oneDs.data.shift();
      Object.values(graphInfo.datasets).forEach((ds) => {
        if (ds !== oneDs && ds.data.length > maxCount) ds.data.shift();
      });
    }

    graphInfo.chart.update();
  }

  function init(config) {
    const wsPath = (config && config.wsPath) || '/ws';
    wsDebugEnabled = !!((config && config.debug) || window.__ESPWEBUTILS_DEBUG_WS__);
    if (wsDebugEnabled) {
      logWsLine(`debug_enabled wsPath=${wsPath}`, 'in');
    }
    connectWs(wsPath);
  }

  window.ModelGeneric = { init };
})();

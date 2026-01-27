// Generic model UI renderer shared by admin.html and model2.html.
// Expects certain DOM ids to exist (same as admin.html):
// - cardsContainer, graphsContainer, wsDebugLog, wsBadge
// Optional: Materialize toast if M is present.

(function () {
  const topicState = {};
  let ws = null;
  let wsDebugEnabled = false;

  const graphCharts = {}; // { graph_name: { chart, datasets: { label: dataset }, maxCount } }

  const byId = (id) => document.getElementById(id);
  const formatTopic = (t) => String(t || '').replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());

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

  function renderTopics() {
    const container = byId('cardsContainer');
    if (!container) return;
    container.innerHTML = '';

    Object.keys(topicState)
      .sort()
      .forEach((topic) => {
        const data = topicState[topic] || {};
        const card = document.createElement('div');
        card.className = 'card';
        card.dataset.topic = topic;

        const content = document.createElement('div');
        content.className = 'card-content';
        const title = document.createElement('span');
        title.className = 'card-title';
        title.textContent = formatTopic(topic);
        content.appendChild(title);

        const fields = Object.keys(data);
        if (fields.length === 0) {
          const p = document.createElement('p');
          p.textContent = 'Keine Daten verfügbar';
          content.appendChild(p);
        } else {
          fields.forEach((key) => {
            const info = unwrapField(data[key]);

            // graph_xy_ring gets rendered into graphsContainer
            if (info.raw && info.raw.type === 'graph_xy_ring') {
              renderGraph(topic, key, info.raw);
              return;
            }

            const row = document.createElement('div');
            row.className = 'setting-row';

            const isButtonKey = String(key).toLowerCase().includes('button');

            if (info.kind === 'button' || isButtonKey) {
              const btn = document.createElement('a');
              btn.className = 'btn waves-effect waves-light orange';
              btn.style.margin = '0.5rem 0';
              btn.textContent = '▶ ' + String(key).replace(/_/g, ' ');
              btn.onclick = () => triggerButton(topic, key);
              row.appendChild(btn);
            } else if (info.kind === 'list') {
              const label = document.createElement('label');
              label.textContent = key;
              row.appendChild(label);

              const ul = document.createElement('ul');
              ul.className = 'value-list';
              (info.value || []).forEach((item) => {
                const li = document.createElement('li');
                li.textContent = item;
                ul.appendChild(li);
              });
              row.appendChild(ul);
            } else if (info.kind === 'bool') {
              const label = document.createElement('label');
              label.textContent = key;
              row.appendChild(label);

              const toggle = document.createElement('label');
              const input = document.createElement('input');
              input.type = 'checkbox';
              input.checked = !!info.value;
              input.dataset.field = key;
              input.dataset.kind = 'bool';
              const span = document.createElement('span');
              toggle.appendChild(input);
              toggle.appendChild(span);
              row.appendChild(toggle);
            } else {
              const label = document.createElement('label');
              label.textContent = key;
              row.appendChild(label);

              const input = document.createElement('input');
              input.type = info.kind === 'number' ? 'number' : 'text';
              input.value = info.value;
              input.className = 'browser-default';
              input.dataset.field = key;
              input.dataset.kind = info.kind;
              row.appendChild(input);
            }

            content.appendChild(row);
          });
        }

        const actions = document.createElement('div');
        actions.className = 'card-action';
        const saveBtn = document.createElement('a');
        saveBtn.className = 'btn blue';
        saveBtn.textContent = 'Speichern';
        saveBtn.onclick = () => submitTopicUpdate(topic, card);
        actions.appendChild(saveBtn);

        card.appendChild(content);
        card.appendChild(actions);
        container.appendChild(card);
      });
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
  }

  function sendUpdate(topic, data) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      toast('WebSocket nicht verbunden', 'red');
      return;
    }
    const msg = { topic, data };
    ws.send(JSON.stringify(msg));
    toast(`Sende Update für ${topic}...`, 'blue');
  }

  function triggerButton(topic, button) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      toast('WebSocket nicht verbunden', 'red');
      return;
    }
    const msg = { action: 'button_trigger', topic, button };
    ws.send(JSON.stringify(msg));
    toast(`Button "${button}" ausgelöst...`, 'blue');
  }

  function logWsLine(line) {
    if (!wsDebugEnabled) return;
    const debugLog = byId('wsDebugLog');
    if (!debugLog) return;
    const timestamp = new Date().toLocaleTimeString('de-DE');
    debugLog.textContent += `[${timestamp}] ← ${line}\n`;
    debugLog.scrollTop = debugLog.scrollHeight;
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
      logWsLine(ev.data);

      try {
        const msg = JSON.parse(ev.data);

        if (msg.topic === 'graph_point' && msg.data) {
          handleGraphPoint(msg.data);
          return;
        }

        if (!msg.topic) return;
        topicState[msg.topic] = msg.data || {};
        renderTopics();
      } catch (e) {
        console.error('WS parse error', e);
      }
    };
  }

  function ensureGraph(graphName, maxCount) {
    let graphContainer = byId(`graph-${graphName}`);
    if (!graphContainer) {
      graphContainer = document.createElement('div');
      graphContainer.id = `graph-${graphName}`;
      graphContainer.className = 'card';
      graphContainer.style.marginTop = '1rem';

      const content = document.createElement('div');
      content.className = 'card-content';

      const title = document.createElement('span');
      title.className = 'card-title';
      title.textContent = `📊 ${formatTopic(graphName)}`;
      content.appendChild(title);

      const chartWrapper = document.createElement('div');
      chartWrapper.style.position = 'relative';
      chartWrapper.style.height = '200px';
      chartWrapper.style.width = '100%';

      const canvas = document.createElement('canvas');
      canvas.id = `chart-${graphName}`;
      chartWrapper.appendChild(canvas);
      content.appendChild(chartWrapper);

      graphContainer.appendChild(content);
      const host = byId('graphsContainer');
      if (host) host.appendChild(graphContainer);

      if (!window.Chart) {
        console.warn('Chart.js not loaded; graph disabled');
        graphCharts[graphName] = { chart: null, datasets: {}, maxCount: maxCount || 60 };
        return graphCharts[graphName];
      }

      const ctx = canvas.getContext('2d');
      graphCharts[graphName] = {
        chart: new window.Chart(ctx, {
          type: 'line',
          data: { labels: [], datasets: [] },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 200 },
            scales: {
              x: { display: true, title: { display: true, text: 'Zeit' } },
              y: { display: true, title: { display: true, text: 'Wert' }, beginAtZero: false }
            },
            plugins: { legend: { display: true, position: 'top' } }
          }
        }),
        datasets: {},
        maxCount: maxCount || 60
      };
    }

    return graphCharts[graphName];
  }

  function renderGraph(_topic, key, graphData) {
    const graphName = graphData.graph || 'unknown';
    const label = graphData.label || key;
    const maxCount = graphData.max_count || 60;

    const graphInfo = ensureGraph(graphName, maxCount);
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

    // initial values
    if (graphData.values && Array.isArray(graphData.values) && graphInfo.chart) {
      const dataset = graphInfo.datasets[label];
      dataset.data = [];
      graphInfo.chart.data.labels = [];

      graphData.values.forEach((point, idx) => {
        const timestamp = point.x != null ? String(point.x) : String(idx);
        if (graphInfo.chart.data.labels.length < graphData.values.length) {
          graphInfo.chart.data.labels.push(timestamp);
        }
        dataset.data.push(point.y);
      });

      graphInfo.chart.update();
    }
  }

  function handleGraphPoint(data) {
    if (!data || !data.graph || !data.label) return;
    const graphName = data.graph;
    const label = data.label;

    const graphInfo = graphCharts[graphName];
    if (!graphInfo || !graphInfo.chart) return;

    const timestamp = data.x != null ? String(data.x) : String(Date.now());
    const value = data.y;

    if (!graphInfo.datasets[label]) return;

    graphInfo.chart.data.labels.push(timestamp);
    graphInfo.datasets[label].data.push(value);

    const maxCount = graphInfo.maxCount || 60;
    if (graphInfo.chart.data.labels.length > maxCount) {
      graphInfo.chart.data.labels.shift();
      Object.values(graphInfo.datasets).forEach((ds) => {
        if (ds.data.length > maxCount) ds.data.shift();
      });
    }

    graphInfo.chart.update();
  }

  function init(config) {
    const wsPath = (config && config.wsPath) || '/ws';
    wsDebugEnabled = !!((config && config.debug) || window.__ESPWEBUTILS_DEBUG_WS__);
    connectWs(wsPath);
  }

  window.ModelGeneric = { init };
})();

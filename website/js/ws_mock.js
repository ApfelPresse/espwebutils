(function () {
  function hasDemoFlag() {
    try {
      return new URLSearchParams(window.location.search).get('demo') === '1';
    } catch (_) {
      return false;
    }
  }

  if (!hasDemoFlag()) return;

  const OriginalWebSocket = window.WebSocket;

  function pickDemoMessages(url) {
    try {
      const parsed = new URL(url, window.location.href);
      if (parsed.pathname === '/ws2') {
        return window.ESPWEBUTILS_DEMO_WS2_MESSAGES || [];
      }
      return window.ESPWEBUTILS_DEMO_WS_MESSAGES || [];
    } catch (_) {
      return window.ESPWEBUTILS_DEMO_WS_MESSAGES || [];
    }
  }

  class MockWebSocket {
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;

    constructor(url) {
      this.url = url;
      this.readyState = MockWebSocket.CONNECTING;
      this.onopen = null;
      this.onclose = null;
      this.onerror = null;
      this.onmessage = null;

      this._timers = [];
      this._messages = pickDemoMessages(url);

      // Deterministic-ish RNG for stable demos
      this._rngState = 0x12345678;
      this._graphStreams = this._discoverGraphStreams(this._messages);
      this._tempStreams = this._initTempStreams(this._graphStreams);

      // async open
      this._setTimer(() => {
        this.readyState = MockWebSocket.OPEN;
        if (typeof this.onopen === 'function') this.onopen({ type: 'open' });

        // replay messages
        const baseDelayMs = 150;
        this._messages.forEach((payload, idx) => {
          this._setTimer(() => {
            if (this.readyState !== MockWebSocket.OPEN) return;
            if (typeof this.onmessage === 'function') {
              this.onmessage({ data: payload });
            }
          }, baseDelayMs * (idx + 1));
        });

        // Start a lightweight live stream after the initial replay.
        // This makes graphs feel "alive" in demo mode without any ESP.
        const streamDelayMs = baseDelayMs * (this._messages.length + 2);
        this._setTimer(() => {
          this._startTempStreaming();
          this._startGraphStreaming();
        }, streamDelayMs);
      }, 0);
    }

    _initTempStreams(allStreams) {
      // Prefer the explicit demo series if present; otherwise create sane defaults.
      const fromDemo = (allStreams || []).filter((s) => s && s.graph === 'temp');
      if (fromDemo.length) {
        // Re-center around ~25°C for demo readability, but keep x continuous.
        return fromDemo.map((s, idx) => ({
          graph: 'temp',
          label: s.label,
          x: (typeof s.x === 'number' && isFinite(s.x)) ? s.x : 0,
          y: 25.0 + (idx - 1) * 0.2
        }));
      }

      const labels = ['Ds18b20 °C', 'Sht3x °C', 'Scd41 °C'];
      return labels.map((label, idx) => ({
        graph: 'temp',
        label,
        y: 25.0 + (idx - 1) * 0.2,
        x: 0
      }));
    }

    send(data) {
      // For demo, just log what the UI would send.
      try {
        console.log('[WS demo] send →', data);
      } catch (_) {}
    }

    close() {
      if (this.readyState === MockWebSocket.CLOSED) return;
      this.readyState = MockWebSocket.CLOSED;
      this._clearTimers();
      if (typeof this.onclose === 'function') this.onclose({ type: 'close' });
    }

    _rand01() {
      // xorshift32
      let x = this._rngState | 0;
      x ^= x << 13;
      x ^= x >>> 17;
      x ^= x << 5;
      this._rngState = x | 0;
      // unsigned -> [0,1)
      return ((x >>> 0) / 4294967296);
    }

    _discoverGraphStreams(messages) {
      const streamsByKey = {};
      (messages || []).forEach((payload) => {
        try {
          const msg = JSON.parse(payload);
          if (!msg || !msg.data || typeof msg.data !== 'object') return;

          // Find embedded graph_xy_ring fields in topic snapshots
          Object.values(msg.data).forEach((v) => {
            if (!v || typeof v !== 'object') return;
            if (v.type !== 'graph_xy_ring') return;
            if (!v.graph || !v.label) return;

            const key = `${v.graph}\u0000${v.label}`;
            if (streamsByKey[key]) return;

            let y = 0;
            let x = 0;
            try {
              if (Array.isArray(v.values) && v.values.length) {
                const last = v.values[v.values.length - 1];
                if (last && typeof last.y === 'number') y = last.y;
                if (last && typeof last.x === 'number') x = last.x;
              }
            } catch (_) {}

            streamsByKey[key] = {
              graph: String(v.graph),
              label: String(v.label),
              y,
              // Keep x as "uptime-like" ms so we don't mix epoch-ms with small snapshot x-values.
              x
            };
          });
        } catch (_) {}
      });
      return Object.values(streamsByKey);
    }

    _startGraphStreaming() {
      if (this.readyState !== MockWebSocket.OPEN) return;
      if (!this._graphStreams || this._graphStreams.length === 0) return;

      // Allow turning off streaming via ?stream=0
      try {
        const p = new URLSearchParams(window.location.search);
        if (p.get('stream') === '0') return;
      } catch (_) {}

      const periodMs = 900;
      const id = window.setInterval(() => {
        if (this.readyState !== MockWebSocket.OPEN) return;
        if (typeof this.onmessage !== 'function') return;

        this._graphStreams.forEach((s) => {
          // Temperature is streamed separately (every 5s) to make it easier to observe.
          if (s && s.graph === 'temp') return;

          // Random walk around current value
          const base = (typeof s.y === 'number' && isFinite(s.y)) ? s.y : 0;
          const scale = Math.max(0.05, Math.min(1.0, Math.abs(base) * 0.01));
          const delta = (this._rand01() - 0.5) * 2 * scale;
          s.y = base + delta;
          s.x = (typeof s.x === 'number' && isFinite(s.x)) ? (s.x + periodMs) : periodMs;

          const payload = JSON.stringify({
            topic: 'graph_point',
            data: {
              graph: s.graph,
              label: s.label,
              x: s.x,
              y: s.y,
              synced: false
            }
          });
          this.onmessage({ data: payload });
        });
      }, periodMs);

      this._timers.push(id);
    }

    _startTempStreaming() {
      if (this.readyState !== MockWebSocket.OPEN) return;
      if (!this._tempStreams || this._tempStreams.length === 0) return;

      // Allow turning off streaming via ?stream=0
      try {
        const p = new URLSearchParams(window.location.search);
        if (p.get('stream') === '0') return;
      } catch (_) {}

      const periodMs = 5000;
      const id = window.setInterval(() => {
        if (this.readyState !== MockWebSocket.OPEN) return;
        if (typeof this.onmessage !== 'function') return;

        this._tempStreams.forEach((s, idx) => {
          const base = (typeof s.y === 'number' && isFinite(s.y)) ? s.y : (25.0 + (idx - 1) * 0.2);

          // Random walk around ~25°C
          const delta = (this._rand01() - 0.5) * 0.6; // ±0.3°C
          s.y = base + delta;
          s.x = (typeof s.x === 'number' && isFinite(s.x)) ? (s.x + periodMs) : periodMs;

          const payload = JSON.stringify({
            topic: 'graph_point',
            data: {
              graph: 'temp',
              label: s.label,
              x: s.x,
              y: s.y,
              synced: false
            }
          });
          this.onmessage({ data: payload });
        });
      }, periodMs);

      this._timers.push(id);
    }

    _setTimer(fn, delayMs) {
      const id = window.setTimeout(fn, delayMs);
      this._timers.push(id);
    }

    _clearTimers() {
      this._timers.forEach((id) => {
        try { window.clearTimeout(id); } catch (_) {}
        try { window.clearInterval(id); } catch (_) {}
      });
      this._timers = [];
    }
  }

  // Mirror readyState constants
  MockWebSocket.OPEN = 1;
  MockWebSocket.CONNECTING = 0;
  MockWebSocket.CLOSING = 2;
  MockWebSocket.CLOSED = 3;

  // Monkeypatch
  window.WebSocket = MockWebSocket;
  window.__ESPWEBUTILS_DEMO__ = { enabled: true, OriginalWebSocket };

  try {
    console.log('[WS demo] enabled (?demo=1)');
  } catch (_) {}
})();

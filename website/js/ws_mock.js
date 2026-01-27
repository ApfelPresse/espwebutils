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
      }, 0);
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

    _setTimer(fn, delayMs) {
      const id = window.setTimeout(fn, delayMs);
      this._timers.push(id);
    }

    _clearTimers() {
      this._timers.forEach((id) => window.clearTimeout(id));
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

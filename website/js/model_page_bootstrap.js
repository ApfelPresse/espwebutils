// Bootstrap shared model page.
// Reads config from query params and/or window.MODEL_UI_CONFIG.

(function () {
  function readQueryConfig() {
    try {
      const qs = new URLSearchParams(window.location.search);
      const ws = qs.get('ws');
      const title = qs.get('title');
      const alias = qs.get('alias');
      const demo = qs.get('demo') === '1';
      const debug = qs.get('debug') === '1';
      return {
        wsPath: ws || undefined,
        title: title || undefined,
        alias: alias || undefined,
        demo,
        debug
      };
    } catch (_) {
      return {};
    }
  }

  document.addEventListener('DOMContentLoaded', () => {
    const queryCfg = readQueryConfig();
    const cfg = Object.assign({ wsPath: '/ws' }, window.MODEL_UI_CONFIG || {}, queryCfg);

    // If we were redirected to /model.html but want to present a friendly endpoint
    // (e.g. /admin or /model2), replace the address bar entry.
    if (cfg.alias) {
      try {
        window.history.replaceState({}, '', cfg.alias);
      } catch (_) {}
    }

    // Expose debug flag for other scripts.
    window.__ESPWEBUTILS_DEBUG_WS__ = !!cfg.debug;

    if (cfg.title) {
      document.title = cfg.title;
      const h = document.getElementById('pageTitle');
      if (h) h.textContent = cfg.title;
    }

    // Always show main content; auth (if enabled) is handled by HTTP Basic Auth.
    const main = document.getElementById('mainContent');
    if (main) main.style.display = 'block';

    // WS debug tile: only visible when debug=1.
    const debugCard = document.getElementById('wsDebugCard');
    if (debugCard) {
      debugCard.style.display = cfg.debug ? 'block' : 'none';
    }

    if (window.ModelGeneric) {
      window.ModelGeneric.init({ wsPath: cfg.wsPath || '/ws', debug: !!cfg.debug });
    }
  });
})();

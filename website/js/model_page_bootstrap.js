// Bootstrap shared model page.
// Reads config from query params and/or window.MODEL_UI_CONFIG.

(function () {
  function readQueryConfig() {
    try {
      const qs = new URLSearchParams(window.location.search);
      const cfg = {};

      if (qs.has('ws')) cfg.wsPath = qs.get('ws') || undefined;
      if (qs.has('title')) cfg.title = qs.get('title') || undefined;
      if (qs.has('alias')) cfg.alias = qs.get('alias') || undefined;
      if (qs.has('graphMode') || qs.has('graph')) cfg.graphMode = (qs.get('graphMode') || qs.get('graph')) || undefined;

      // Only override defaults if the flag is explicitly present.
      if (qs.has('demo')) cfg.demo = qs.get('demo') === '1';
      if (qs.has('debug')) cfg.debug = qs.get('debug') === '1';

      return cfg;
    } catch (_) {
      return {};
    }
  }

  document.addEventListener('DOMContentLoaded', () => {
    const queryCfg = readQueryConfig();
    console.log('[model_page_bootstrap] queryCfg:', queryCfg);
    
    // Auto-enable debug if demo=1
    if (queryCfg.demo && !queryCfg.debug) {
      queryCfg.debug = true;
    }
    
    const cfg = Object.assign({ wsPath: '/ws' }, window.MODEL_UI_CONFIG || {}, queryCfg);
    console.log('[model_page_bootstrap] final cfg:', cfg);

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
      window.ModelGeneric.init({
        wsPath: cfg.wsPath || '/ws',
        debug: !!cfg.debug,
        graphMode: cfg.graphMode
      });
    }
  });
})();

// Demo WebSocket payloads for local UI testing (no ESP required).
// Usage: open any page with ?demo=1 and include ws_mock.js.

// Admin/WiFi stream (initial full topics + a couple of graph_point updates)
window.ESPWEBUTILS_DEMO_WS_MESSAGES = [
  '{"topic":"wifi","data":{"ssid":{"value":"Wifi1"},"pass":{"type":"secret","initialized":true},"available_networks":{"type":"list","count":0,"capacity":20,"items":[]},"log_level":{"value":0}}}',
  '{"topic":"wifi","data":{"ssid":{"value":""},"pass":{"type":"secret","initialized":false},"available_networks":{"type":"list","count":9,"capacity":20,"items":["Wifi1","Wifi2","Wifi3","Wif4","Wifi5","Wifi6","Wifi7","Wifi8","Wifi9"]},"log_level":{"value":0}}}',
  '{"topic":"ota","data":{"ota_pass":{"value":"ota_pass_123"},"generate_new_pass_button":{"type":"button","id":0}}}',
  '{"topic":"mdns","data":{"mdns_domain":"meinesp"}}',
  '{"topic":"admin","data":{"pass":{"value":"admin_pass_123"},"session":{"value":""},"generate_new_admin_ui_pass":{"type":"button","id":0},"admin_log":{"type":"graph_xy_ring","graph":"admin_events","label":"auth","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":30037,"y":222296},{"x":35037,"y":222296},{"x":40037,"y":222308},{"x":45037,"y":220740},{"x":50037,"y":222628}]}}}',
  '{"topic":"admin_ui","data":{"config":{"value":""}}}',
  '{"topic":"graph_point","data":{"graph":"admin_events","label":"auth","x":55037,"y":220820,"synced":true}}',
  '{"topic":"graph_point","data":{"graph":"admin_events","label":"auth","x":60037,"y":220820,"synced":true}}'
];

// Minimal stream for Model2 (ws2)
window.ESPWEBUTILS_DEMO_WS2_MESSAGES = [
  '{"topic":"test","data":{"value":{"value":1}}}',
  '{"topic":"test","data":{"value":{"value":123}}}'
];

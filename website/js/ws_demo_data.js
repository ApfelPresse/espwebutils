// Demo WebSocket payloads for local UI testing (no ESP required).
// Usage: open any page with ?demo=1 and include ws_mock.js.

// Admin/WiFi stream (initial full topics + a couple of graph_point updates)
window.ESPWEBUTILS_DEMO_WS_MESSAGES = [
  '{"topic":"wifi","data":{"ssid":{"value":"Wifi1"},"pass":{"type":"secret","initialized":true},"available_networks":{"type":"list","count":0,"capacity":20,"items":[]},"log_level":{"value":0}}}',
  '{"topic":"wifi","data":{"ssid":{"value":""},"pass":{"type":"secret","initialized":false},"available_networks":{"type":"list","count":9,"capacity":20,"items":["Wifi1","Wifi2","Wifi3","Wif4","Wifi5","Wifi6","Wifi7","Wifi8","Wifi9"]},"log_level":{"value":0}}}',
  '{"topic":"ota","data":{"ota_pass":{"value":"ota_pass_123"},"generate_new_pass_button":{"type":"button","id":0}}}',
  '{"topic":"mdns","data":{"mdns_domain":"meinesp"}}',
  '{"topic":"admin","data":{"pass":{"value":"admin_pass_123"},"session":{"value":""},"generate_new_admin_ui_pass":{"type":"button","id":0},"admin_log":{"type":"graph_xy_ring","graph":"admin_events","label":"auth","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":30037,"y":222296},{"x":35037,"y":222296},{"x":40037,"y":222308},{"x":45037,"y":220740},{"x":50037,"y":222628}]}}}',
  // Multi-series graph demo: 3 temperature sensors in the same graph (graph="temp") distinguished by label.
  '{"topic":"temperatur","data":{"Ds18b20_temp":{"type":"graph_xy_ring","graph":"temp","label":"Ds18b20 °C","size":10,"count":10,"max_count":60,"synced":false,"values":[{"x":1000,"y":21.1},{"x":2000,"y":21.2},{"x":3000,"y":21.4},{"x":4000,"y":21.5},{"x":5000,"y":21.6},{"x":6000,"y":21.7},{"x":7000,"y":21.7},{"x":8000,"y":21.8},{"x":9000,"y":21.9},{"x":10000,"y":22.0}]},"Sht3x_temp":{"type":"graph_xy_ring","graph":"temp","label":"Sht3x °C","size":10,"count":10,"max_count":60,"synced":false,"values":[{"x":1000,"y":20.6},{"x":2000,"y":20.7},{"x":3000,"y":20.7},{"x":4000,"y":20.8},{"x":5000,"y":20.9},{"x":6000,"y":21.0},{"x":7000,"y":21.0},{"x":8000,"y":21.1},{"x":9000,"y":21.2},{"x":10000,"y":21.3}]},"Scd41_temp":{"type":"graph_xy_ring","graph":"temp","label":"Scd41 °C","size":10,"count":10,"max_count":60,"synced":false,"values":[{"x":1000,"y":22.4},{"x":2000,"y":22.4},{"x":3000,"y":22.3},{"x":4000,"y":22.3},{"x":5000,"y":22.2},{"x":6000,"y":22.2},{"x":7000,"y":22.1},{"x":8000,"y":22.1},{"x":9000,"y":22.0},{"x":10000,"y":22.0}]}}}',
  '{"topic":"admin_ui","data":{"config":{"value":""}}}',
  '{"topic":"graph_point","data":{"graph":"admin_events","label":"auth","x":55037,"y":220820,"synced":true}}',
  '{"topic":"graph_point","data":{"graph":"admin_events","label":"auth","x":60037,"y":220820,"synced":true}}'
  ,
  // Live updates for the shared temp graph (three labels => three datasets)
  '{"topic":"graph_point","data":{"graph":"temp","label":"Ds18b20 °C","x":11000,"y":22.1,"synced":false}}',
  '{"topic":"graph_point","data":{"graph":"temp","label":"Sht3x °C","x":11000,"y":21.4,"synced":false}}',
  '{"topic":"graph_point","data":{"graph":"temp","label":"Scd41 °C","x":11000,"y":21.9,"synced":false}}',
  '{"topic":"graph_point","data":{"graph":"temp","label":"Ds18b20 °C","x":12000,"y":22.2,"synced":false}}',
  '{"topic":"graph_point","data":{"graph":"temp","label":"Sht3x °C","x":12000,"y":21.5,"synced":false}}',
  '{"topic":"graph_point","data":{"graph":"temp","label":"Scd41 °C","x":12000,"y":21.9,"synced":false}}'
];

// Minimal stream for Model2 (ws2)
window.ESPWEBUTILS_DEMO_WS2_MESSAGES = [
  '{"topic":"test","data":{"value":{"value":1}}}',
  '{"topic":"test","data":{"value":{"value":123}}}'
];

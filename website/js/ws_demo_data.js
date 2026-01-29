// Demo WebSocket payloads for local UI testing (no ESP required).
// Based on realistic sensor/control data with proper timing.
// Usage: open any page with ?demo=1 and include ws_mock.js.

// Initial full state - all topics sent at startup
window.ESPWEBUTILS_DEMO_WS_MESSAGES = [
  // Control settings
  '{"topic":"control","data":{"refresh_sensor_ms":{"value":1000},"desired_temperature":{"value":30},"current_temperature":{"value":28},"enable_control":{"value":true}}}',
  
  // Temperature data with 5 historical points (shared graph "temp" with 2 series)
  '{"topic":"temperatur","data":{"ds18b20":{"type":"graph_xy_ring","graph":"temp","label":"Ds18b20 °C","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":100000,"y":28.0},{"x":101000,"y":28.1},{"x":102000,"y":28.15},{"x":103000,"y":28.2},{"x":104000,"y":28.25}]},"sht3x":{"type":"graph_xy_ring","graph":"temp","label":"Sht3x °C","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":100000,"y":30.5},{"x":101000,"y":30.6},{"x":102000,"y":30.55},{"x":103000,"y":30.6},{"x":104000,"y":30.65}]}}}',
  
  // Humidity data
  '{"topic":"humidity","data":{"sht3x":{"type":"graph_xy_ring","graph":"humidity","label":"Sht3x %RH","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":100500,"y":62.5},{"x":101500,"y":62.6},{"x":102500,"y":62.7},{"x":103500,"y":62.8},{"x":104500,"y":62.9}]}}}',
  
  // CO2/PPM data
  '{"topic":"ppm","data":{"scd41":{"type":"graph_xy_ring","graph":"ppm","label":"Scd41 ppm","size":5,"count":5,"max_count":5,"synced":false,"values":[{"x":90000,"y":1150},{"x":95000,"y":1151},{"x":100000,"y":1151},{"x":105000,"y":1150},{"x":110000,"y":1149}]}}}',
  
  // Ventilation control
  '{"topic":"ventilation","data":{"pwm_value":{"value":100},"0%":{"type":"button","id":0},"100%":{"type":"button","id":0}}}',
  
  // Air intake control
  '{"topic":"air_intake","data":{"pwm_value":{"value":0},"0%":{"type":"button","id":0},"100%":{"type":"button","id":0}}}',
  
  // Cooling control
  '{"topic":"cooling","data":{"pwm_value":{"value":0},"0%":{"type":"button","id":0},"100%":{"type":"button","id":0}}}',
  
  // Heating control
  '{"topic":"heating","data":{"pwm_value":{"value":100},"0%":{"type":"button","id":0},"100%":{"type":"button","id":0}}}'
];

// Minimal stream for Model2 (ws2)
window.ESPWEBUTILS_DEMO_WS2_MESSAGES = [
  '{"topic":"test","data":{"value":{"value":1}}}',
  '{"topic":"test","data":{"value":{"value":123}}}'
];

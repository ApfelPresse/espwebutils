#pragma once

// Included by src/model/ModelBase.h

inline void ModelBase::sendGraphPointXY(const char* graph, const char* label, uint64_t x, float y, bool synced) {
  LOG_DEBUG_F("[WS] Sending graph_point: graph=%s, label=%s, x=%llu, y=%.2f", graph, label, x, y);
  StaticJsonDocument<256> doc;
  doc["topic"] = "graph_point";
  JsonObject d = doc.createNestedObject("data");
  d["graph"] = graph;
  d["label"] = label;
  d["x"] = (uint64_t)x;
  d["y"] = y;
  d["synced"] = synced;

  String out;
  serializeJson(doc, out);
  LOG_TRACE_F("[WS] Graph point JSON: %s", out.c_str());
  ws_.textAll(out);
}

inline void ModelBase::graphPushCbXY(const char* graph, const char* label, uint64_t x, float y, void* ctx) {
  LOG_TRACE_F("[CALLBACK] graphPushCbXY called: graph=%s, label=%s, x=%llu, y=%.2f, ctx=%p",
              graph, label, x, y, ctx);
  if (!ctx) return;
  ModelBase* self = (ModelBase*)ctx;
  self->sendGraphPointXY(graph, label, x, y, true);
}

inline void ModelBase::sendGraphPoint(const char* graph, const char* label, int value) {
  StaticJsonDocument<256> doc;
  doc["topic"] = "graph_point";
  JsonObject d = doc.createNestedObject("data");
  d["graph"] = graph;
  d["label"] = label;
  d["value"] = value;

  String out;
  serializeJson(doc, out);
  ws_.textAll(out);
}

inline void ModelBase::graphPushCb(const char* graph, const char* label, int value, void* ctx) {
  if (!ctx) return;
  ((ModelBase*)ctx)->sendGraphPoint(graph, label, value);
}

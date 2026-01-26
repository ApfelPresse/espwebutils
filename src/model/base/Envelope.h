#pragma once

// Included by src/model/ModelBase.h

inline String ModelBase::makeEnvelope(Entry& e) {
  static StaticJsonDocument<JSON_CAPACITY> doc;  // reuse to keep allocations off stack
  doc.clear();
  doc["topic"] = e.topic;

  JsonObject data = doc.createNestedObject("data");
  e.makeWsJson(e.objPtr, data);

  String out;
  out.reserve(measureJson(doc) + 1);
  serializeJson(doc, out);
  return out;
}

inline String ModelBase::makeDataOnlyJson(Entry& e) {
  LOG_TRACE_F("[ModelBase] makeDataOnlyJson starting for topic '%s'", e.topic);
  static StaticJsonDocument<JSON_CAPACITY> doc;  // reuse to keep allocations off stack
  doc.clear();
  JsonObject data = doc.to<JsonObject>();

  LOG_TRACE_F("[ModelBase] Calling makePrefsJson for topic '%s'", e.topic);
  e.makePrefsJson(e.objPtr, data);
  LOG_TRACE_F("[ModelBase] makePrefsJson completed");

  String out;
  out.reserve(measureJson(doc) + 1);
  serializeJson(doc, out);
  LOG_TRACE_F("[ModelBase] makeDataOnlyJson result: %s", out.c_str());
  return out;
}

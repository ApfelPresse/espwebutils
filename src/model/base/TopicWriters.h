#pragma once

// Included by src/model/ModelBase.h

template <typename T>
inline void ModelBase::makeWsJsonImpl(void* objPtr, JsonObject out) {
  T& obj = *(T*)objPtr;
  fj::write_ws(obj, out);
}

template <typename T>
inline void ModelBase::makePrefsJsonImpl(void* objPtr, JsonObject out) {
  T& obj = *(T*)objPtr;
  fj::write_prefs(obj, out);
}

template <typename T>
inline bool ModelBase::applyUpdateImpl(void* objPtr, const String& dataJson, bool strict) {
  LOG_TRACE_F("[ModelBase::applyUpdateImpl] Parsing JSON: %s", dataJson.c_str());
  T& obj = *(T*)objPtr;

  static StaticJsonDocument<JSON_CAPACITY> doc;  // reuse to avoid stack bloat
  doc.clear();
  if (deserializeJson(doc, dataJson)) {
    LOG_WARN_F("[ModelBase::applyUpdateImpl] JSON parse failed");
    return false;
  }

  LOG_TRACE_F("[ModelBase::applyUpdateImpl] JSON parsed successfully, calling TypeAdapter<T>::read");
  JsonObject root = doc.as<JsonObject>();
  bool result = fj::TypeAdapter<T>::read(obj, root, strict);
  LOG_TRACE_F("[ModelBase::applyUpdateImpl] TypeAdapter<T>::read returned: %s", result ? "true" : "false");
  return result;
}

template <typename T>
inline bool ModelBase::applyUpdateJsonImpl(void* objPtr, JsonObject data, bool strict) {
  T& obj = *(T*)objPtr;
  bool result = fj::TypeAdapter<T>::read(obj, data, strict);
  LOG_TRACE_F("[ModelBase::applyUpdateJsonImpl] TypeAdapter<T>::read returned: %s", result ? "true" : "false");
  return result;
}

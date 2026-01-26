#pragma once

// Included by src/model/ModelBase.h

inline bool ModelBase::saveTopic(const char* topic) {
  Entry* e = find(topic);
  if (!e) return false;
  return saveEntry(*e);
}

inline bool ModelBase::saveEntry(Entry& e) {
  if (!e.persist) {
    LOG_TRACE_F("[Prefs] Topic '%s' not persisted (persist=false)", e.topic);
    return true;
  }
  LOG_TRACE_F("[Prefs] saveEntry starting for topic '%s'", e.topic);
  String dataJson = makeDataOnlyJson(e);
  LOG_INFO_F("[Prefs] Saving topic '%s': %s", e.topic, dataJson.c_str());
  size_t written = prefs_.putString(e.topic, dataJson);
  LOG_INFO_F("[Prefs] Written %u bytes for topic '%s'", written, e.topic);
  if (written == 0) {
    LOG_WARN_F("[Prefs] FAILED to write topic '%s' - putString returned 0", e.topic);
  }
  LOG_TRACE_F("[Prefs] saveEntry completed for topic '%s', success=%s", e.topic, written > 0 ? "true" : "false");
  return written > 0;
}

inline bool ModelBase::loadEntry(Entry& e) {
  if (!e.persist) {
    LOG_TRACE_F("[Prefs] Topic '%s' not persisted (persist=false)", e.topic);
    return true;
  }

  if (!prefs_.isKey(e.topic)) {
    LOG_TRACE_F("[Prefs] Topic '%s' not found in Preferences, initializing", e.topic);
    return saveEntry(e);
  }

  String dataJson = prefs_.getString(e.topic, "");
  if (!dataJson.length()) {
    LOG_TRACE_F("[Prefs] Topic '%s' exists but empty", e.topic);
    return false;
  }

  LOG_TRACE_F("[Prefs] Loading topic '%s': %s", e.topic, dataJson.c_str());
  LOG_TRACE_F("[ModelBase] About to call e.applyUpdate for topic '%s'", e.topic);
  bool result = e.applyUpdate(e.objPtr, dataJson, false);
  LOG_TRACE_F("[ModelBase] applyUpdate completed for topic '%s', result=%s", e.topic, result ? "true" : "false");
  return result;
}

inline void ModelBase::loadOrInitAll() {
  for (size_t i = 0; i < entryCount_; ++i) {
    (void)loadEntry(entries_[i]);
  }
}

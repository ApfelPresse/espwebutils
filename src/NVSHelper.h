#pragma once
#include <Preferences.h>

inline String readPref(const char* ns, const char* key, const String& defaultValue = "") {
  Preferences prefs;
  prefs.begin(ns, true);
  String value = prefs.getString(key, defaultValue);
  prefs.end();
  return value;
}

inline void writePref(const char* ns, const char* key, const String& value) {
  Preferences prefs;
  prefs.begin(ns, false);
  prefs.putString(key, value);
  prefs.end();
}

inline void removePref(const char* ns, const char* key) {
  Preferences prefs;
  prefs.begin(ns, false);
  prefs.remove(key);
  prefs.end();
}

inline void clearNamespace(const char* ns) {
  Preferences prefs;
  prefs.begin(ns, false);
  prefs.clear();
  prefs.end();
}

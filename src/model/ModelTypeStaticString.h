#pragma once
#include <cstring>
#include "ModelSerializer.h"
#include "ModelTypeTraits.h"
#include <functional>

template <size_t N>
struct StaticString {
  char value[N];
  std::function<void()> on_change;
  StaticString() { value[0] = '\0'; }
  StaticString(const char* s) { set(s); }

  void set(const char* s) {
    if (!s) { value[0] = '\0'; return; }
    std::strncpy(value, s, N - 1);
    value[N - 1] = '\0';
    if (on_change) on_change();
  }
  const char* c_str() const { return value; }

  void setOnChange(std::function<void()> cb) { on_change = cb; }

  // Convenient assignment operators
  StaticString& operator=(const char* s) { set(s); return *this; }
  StaticString& operator=(const String& s) { set(s.c_str()); return *this; }
  // Conversions and comparisons
  operator const char*() const { return c_str(); }
  operator String() const { return String(c_str()); }

  bool operator==(const char* s) const { return std::strcmp(c_str(), s) == 0; }
  bool operator!=(const char* s) const { return !(*this == s); }
  bool operator==(const String& s) const { return String(c_str()) == s; }
  bool operator!=(const String& s) const { return !(*this == s); }
};

namespace fj {

// Direct field serialization support (for fields not wrapped in Var<>)
template <typename T, size_t N>
inline void writeOne(const T& obj, const Field<T, StaticString<N>>& f, JsonObject out) {
  out[f.key] = (obj.*(f.member)).c_str();
}

template <typename T, size_t N>
inline bool readOne(T& obj, const Field<T, StaticString<N>>& f, JsonObject in) {
  JsonVariant v = in[f.key];
  if (v.isNull()) return false;
  const char* s = v.template as<const char*>();
  if (!s) return false;
  (obj.*(f.member)).set(s);
  return true;
}

// TypeAdapter used by Var<StaticString<N>> for wrapped values
template <size_t N>
struct TypeAdapter<StaticString<N>> {
  static void write_ws(const StaticString<N>& s, JsonObject out) {
    out["value"] = s.c_str();
  }

  static void write_prefs(const StaticString<N>& s, JsonObject out) {
    out["value"] = s.c_str();
  }

  static bool read(StaticString<N>& s, JsonObject in, bool /*strict*/) {
    JsonVariant v = in["value"];
    if (v.isNull()) return true; // tolerant
    const char* str = v.as<const char*>();
    if (!str) return false;
    s.set(str);
    return true;
  }
};

} // namespace fj

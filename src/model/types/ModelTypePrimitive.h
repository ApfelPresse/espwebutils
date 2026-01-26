#pragma once
#include <cstring>
#include "../ModelSerializer.h"
#include "ModelTypeTraits.h"
#include <functional>

// ============================================================================
// StringBuffer<N> - Minimales String-Wrapper für Template-Parameter
// Ersatz für StaticString mit vereinfachter API
// Note: No on_change callback - use Var<StringBuffer<N>> wrapper for callbacks
// ============================================================================

template <size_t N>
struct StringBuffer {
  char buf[N];

  StringBuffer() { 
    buf[0] = '\0'; 
    LOG_TRACE_F("[StringBuffer] Constructor: initialized empty buffer (size=%zu)", N);
  }
  StringBuffer(const char* s) { 
    LOG_TRACE_F("[StringBuffer] Constructor(const char*): input='%s'", s ? s : "nullptr");
    set(s); 
  }

  void set(const char* s) {
    if (!s) {
      buf[0] = '\0';
      LOG_TRACE_F("[StringBuffer] set(nullptr): cleared buffer");
      return;
    }
    std::strncpy(buf, s, N - 1);
    buf[N - 1] = '\0';
    LOG_TRACE_F("[StringBuffer] set('%s'): stored in buffer (size=%zu, strlen=%zu)", s, N, strlen(buf));
  }

  const char* c_str() const { return buf; }
  char* data() { return buf; }

  // Vereinfachte Operatoren
  StringBuffer& operator=(const char* s) { 
    LOG_TRACE_F("[StringBuffer] operator=: assigning '%s'", s ? s : "nullptr");
    set(s); 
    return *this; 
  }
  StringBuffer& operator=(const StringBuffer& s) { 
    LOG_TRACE_F("[StringBuffer] operator=(StringBuffer): copying '%s'", s.c_str());
    set(s.c_str()); 
    return *this; 
  }

  operator const char*() const { return c_str(); }
  bool operator==(const char* s) const { return std::strcmp(c_str(), s) == 0; }
  bool operator!=(const char* s) const { return !(*this == s); }
};

namespace fj {

// ============================================================================
// TypeAdapter specializations for primitive types (int, float, bool)
// ============================================================================

// INT
template <>
struct TypeAdapter<int> {
  static bool defaultPersist() { return true; }
  static bool defaultWsSend()  { return true; }

  static void write_ws(const int& val, JsonObject out) {
    out["value"] = val;
  }

  static void write_prefs(const int& val, JsonObject out) {
    out["value"] = val;
  }

  static bool read(int& val, JsonObject in, bool /*strict*/) {
    JsonVariant v = in["value"];
    if (v.isNull()) return false;
    val = v.as<int>();
    return true;
  }
};

// FLOAT
template <>
struct TypeAdapter<float> {
  static bool defaultPersist() { return true; }
  static bool defaultWsSend()  { return true; }

  static void write_ws(const float& val, JsonObject out) {
    out["value"] = val;
  }

  static void write_prefs(const float& val, JsonObject out) {
    out["value"] = val;
  }

  static bool read(float& val, JsonObject in, bool /*strict*/) {
    JsonVariant v = in["value"];
    if (v.isNull()) return false;
    val = v.as<float>();
    return true;
  }
};

// BOOL
template <>
struct TypeAdapter<bool> {
  static bool defaultPersist() { return true; }
  static bool defaultWsSend()  { return true; }

  static void write_ws(const bool& val, JsonObject out) {
    out["value"] = val;
  }

  static void write_prefs(const bool& val, JsonObject out) {
    out["value"] = val;
  }

  static bool read(bool& val, JsonObject in, bool /*strict*/) {
    JsonVariant v = in["value"];
    if (v.isNull()) return false;
    val = v.as<bool>();
    return true;
  }
};

// ============================================================================
// StringBuffer<N> TypeAdapter (replacement for StaticString)
// ============================================================================

template <size_t N>
struct TypeAdapter<StringBuffer<N>> {
  static void write_ws(const StringBuffer<N>& s, JsonObject out) {
    out["value"] = s.c_str();
  }

  static void write_prefs(const StringBuffer<N>& s, JsonObject out) {
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] write_prefs called, value='%s'", s.c_str());
    out["value"] = s.c_str();
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] write_prefs completed");
  }

  static bool read(StringBuffer<N>& s, JsonObject in, bool /*strict*/) {
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: starting, inspecting input JSON");
    JsonVariant v = in["value"];
    bool isNull = v.isNull();
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: 'value' field exists: %s", isNull ? "NO" : "YES");
    
    if (isNull) {
      LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: 'value' is null, returning true (tolerant)");
      return true; // tolerant
    }
    
    const char* str = v.as<const char*>();
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: extracted string='%s'", str ? str : "nullptr");
    
    if (!str) {
      LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: string is nullptr, returning false");
      return false;
    }
    
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: calling s.set('%s')", str);
    s.set(str);
    LOG_TRACE_F("[TypeAdapter<StringBuffer>] read: s.c_str()='%s' after set, returning true", s.c_str());
    return true;
  }
};

// ============================================================================
// Direct field serialization for StringBuffer<N>
// ============================================================================

template <typename T, size_t N>
inline void writeOne(const T& obj, const Field<T, StringBuffer<N>>& f, JsonObject out) {
  const char* val = (obj.*(f.member)).c_str();
  LOG_TRACE_F("[writeOne<StringBuffer>] key='%s', value='%s'", f.key, val);
  out[f.key] = val;
}

template <typename T, size_t N>
inline bool readOne(T& obj, const Field<T, StringBuffer<N>>& f, JsonObject in) {
  LOG_TRACE_F("[readOne<StringBuffer>] Reading key='%s'", f.key);
  JsonVariant v = in[f.key];
  if (v.isNull()) {
    LOG_TRACE_F("[readOne<StringBuffer>] Variant is NULL for key='%s'", f.key);
    return false;
  }
  const char* s = v.as<const char*>();
  if (!s) {
    LOG_TRACE_F("[readOne<StringBuffer>] Extracted string is NULL for key='%s'", f.key);
    return false;
  }
  LOG_TRACE_F("[readOne<StringBuffer>] Setting value='%s' for key='%s'", s, f.key);
  (obj.*(f.member)).set(s);
  LOG_TRACE_F("[readOne<StringBuffer>] Value set successfully");
  return true;
}

} // namespace fj

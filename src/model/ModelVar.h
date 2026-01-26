#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <type_traits>
#include <functional>
#include <cstring>
#include <utility>

#include "../Logger.h"
#include "ModelSerializer.h"
#include "types/ModelTypeTraits.h"

// Policy-based field wrapper.
//
// Per-field controls:
//   - WebSocket output: value vs meta (Variant A) vs none
//   - Preferences persistence: on/off
//   - Remote writability: on/off
//
// Variant A (WsMode::Meta): emits {"type":"secret","initialized":...} and NEVER emits the value.
//
// C++11 compatible (no if constexpr). Additive: does not change existing types/adapters.

namespace fj {

enum class WsMode    : uint8_t { Value, Meta, None };
enum class PrefsMode : uint8_t { On, Off };
enum class WriteMode : uint8_t { On, Off };

namespace detail {

// ---- SFINAE traits ----

template <typename T>
struct has_isInitialized {
  template <typename U>
  static auto test(int) -> decltype(std::declval<const U&>().isInitialized(), std::true_type());
  template <typename>
  static std::false_type test(...);
  static const bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct has_c_str {
  template <typename U>
  static auto test(int) -> decltype(std::declval<const U&>().c_str(), std::true_type());
  template <typename>
  static std::false_type test(...);
  static const bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct has_set_cstr {
  template <typename U>
  static auto test(int) -> decltype(std::declval<U&>().set((const char*)0), std::true_type());
  template <typename>
  static std::false_type test(...);
  static const bool value = decltype(test<T>(0))::value;
};

// Trait to check if T can be assigned from const char* (without set/c_str methods - i.e., NOT List)
template <typename T>
struct is_string_like {
  static const bool value = has_set_cstr<T>::value && has_c_str<T>::value;
};

// ---- initialized_of(T) ----

template <typename T>
inline bool initialized_of_impl(const T& v, std::true_type /*has isInitialized*/) {
  return v.isInitialized();
}

// Helper to check c_str-based initialization (has c_str)
template <typename T>
inline typename std::enable_if<has_c_str<T>::value, bool>::type
initialized_of_check_cstr(const T& v) {
  const char* s = v.c_str();
  return s && s[0] != '\0';
}

// Helper to check c_str-based initialization (no c_str)
template <typename T>
inline typename std::enable_if<!has_c_str<T>::value, bool>::type
initialized_of_check_cstr(const T&) {
  return true; // Not applicable
}

template <typename T>
inline bool initialized_of_impl(const T& v, std::false_type /*no isInitialized*/) {
  // Try c_str() if present
  return initialized_of_check_cstr(v);
}

template <typename T>
inline bool initialized_of(const T& v) {
  return initialized_of_impl(v, std::integral_constant<bool, has_isInitialized<T>::value>{});
}



// ---- TypeAdapter detection ----
template <typename T>
struct has_typeadapter_read {
  template <typename U>
  static auto test(int) -> decltype(
    fj::TypeAdapter<U>::read(std::declval<U&>(), std::declval<JsonObject>(), false),
    std::true_type{}
  );
  template <typename>
  static std::false_type test(...);
  static const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
};

template <typename T>
struct has_typeadapter_write_ws {
  template <typename U>
  static auto test(int) -> decltype(
    fj::TypeAdapter<U>::write_ws(std::declval<const U&>(), std::declval<JsonObject>()),
    std::true_type{}
  );
  template <typename>
  static std::false_type test(...);
  static const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
};

template <typename T>
struct is_scalar {
  static const bool value = std::is_arithmetic<T>::value || std::is_same<T, bool>::value;
};

template <typename T>
inline bool assign_from_variant_scalar(T& dst, JsonVariant v, std::true_type) {
  dst = v.as<T>();
  return true;
}
template <typename T>
inline bool assign_from_variant_scalar(T&, JsonVariant, std::false_type) { return false; }

template <typename T>
inline bool assign_from_variant_string(T& dst, JsonVariant v, std::true_type) {
  dst = v.as<String>();
  return true;
}
template <typename T>
inline bool assign_from_variant_string(T&, JsonVariant, std::false_type) { return false; }

template <typename T>
inline bool try_typeadapter_read(T& dst, JsonObject o, std::true_type) {
  return fj::TypeAdapter<T>::read(dst, o, false);
}
template <typename T>
inline bool try_typeadapter_read(T&, JsonObject, std::false_type) { return false; }

// ---- assignment helpers ----

template <typename T>
inline void assign_from_cstr_impl(T& dst, const char* s, std::true_type /*has set*/) { dst.set(s); }

template <typename T>
inline void assign_from_cstr_impl(T& dst, const char* s, std::false_type /*no set*/) { dst = s; }

template <typename T>
inline void assign_from_cstr(T& dst, const char* s) {
  assign_from_cstr_impl(dst, s, std::integral_constant<bool, has_set_cstr<T>::value>{});
}

// Small helper: log value if it exposes c_str(); otherwise no-op
template <typename T>
inline void log_value_if_cstr(const char* key, const T& val, std::true_type) {
  LOG_TRACE_F("[ModelVar] String-like value for key='%s': '%s' (length=%zu)", key, val.c_str(), strlen(val.c_str()));
}

template <typename T>
inline void log_value_if_cstr(const char*, const T&, std::false_type) {
  // Non-string types: skip detailed value logging
}

template <typename T>
inline void write_value_impl(JsonObject out, const char* key, const T& v, std::true_type /*has typeadapter*/) {
  // Use TypeAdapter::write_ws
  LOG_TRACE_F("[write_value_impl] TypeAdapter path for key='%s'", key);
  JsonObject nested = out.createNestedObject(key);
  fj::TypeAdapter<T>::write_ws(v, nested);
  LOG_TRACE_F("[write_value_impl] TypeAdapter write_ws completed for key='%s'", key);
}

template <typename T>
inline void write_value_impl_no_adapter(JsonObject out, const char* key, const T& v, std::true_type /*has c_str*/) {
  LOG_TRACE_F("[write_value_impl_no_adapter] c_str path for key='%s', value='%s'", key, v.c_str());
  out[key] = v.c_str();
}

template <typename T>
inline void write_value_impl_no_adapter(JsonObject out, const char* key, const T& v, std::false_type /*no c_str*/) {
  LOG_TRACE_F("[write_value_impl_no_adapter] Direct assignment path for key='%s'", key);
  out[key] = v;
}

template <typename T>
inline void write_value_impl(JsonObject out, const char* key, const T& v, std::false_type /*no typeadapter*/) {
  LOG_TRACE_F("[write_value_impl] Non-TypeAdapter path for key='%s'", key);
  write_value_impl_no_adapter(out, key, v, std::integral_constant<bool, has_c_str<T>::value>{});
}

template <typename T>
inline void write_value(JsonObject out, const char* key, const T& v) {
  LOG_TRACE_F("[write_value] Dispatching for key='%s', has_typeadapter=%s", key, has_typeadapter_write_ws<T>::value ? "YES" : "NO");
  write_value_impl(out, key, v, std::integral_constant<bool, has_typeadapter_write_ws<T>::value>{});
  LOG_TRACE_F("[write_value] Dispatch completed for key='%s'", key);
}


// Forward declarations for string fallback helpers
template <typename T>
inline bool read_value_from_variant_string_fallback(T& dst, JsonVariant v, std::true_type);

template <typename T>
inline bool read_value_from_variant_string_fallback(T&, JsonVariant, std::false_type);

// Helper: string fallback enabled (for StaticString)
template <typename T>
inline bool read_value_from_variant_string_fallback(T& dst, JsonVariant v, std::true_type /*is_string_like*/) {
  if (!v.is<const char*>()) return false;
  const char* s = v.as<const char*>();
  if (!s) return false;
  dst.set(s);
  return true;
}

// Helper: string fallback disabled (for List and other non-string types)
template <typename T>
inline bool read_value_from_variant_string_fallback(T&, JsonVariant, std::false_type /*not_string_like*/) {
  return false;
}

// Overload for types that have a TypeAdapter::read -> avoid compiling string assignment paths
// This overload handles TypeAdapter types
template <typename T>
inline typename std::enable_if<has_typeadapter_read<T>::value, bool>::type
read_value_from_variant(T& dst, JsonVariant v) {
  LOG_TRACE_F("[ModelVar::read_value_from_variant] TypeAdapter path, v.isNull()=%s", v.isNull() ? "true" : "false");
  if (v.isNull()) {
    LOG_TRACE("[ModelVar::read_value_from_variant] Variant is NULL, returning false");
    return false;
  }
  
  // Object form (TypeAdapter's expected format)
  if (v.is<JsonObject>()) {
    LOG_TRACE("[ModelVar::read_value_from_variant] Variant is JsonObject, calling TypeAdapter::read");
    JsonObject o = v.as<JsonObject>();
    bool result = fj::TypeAdapter<T>::read(dst, o, false);
    LOG_TRACE_F("[ModelVar::read_value_from_variant] TypeAdapter::read returned: %s", result ? "true" : "false");
    return result;
  }
  
  LOG_TRACE("[ModelVar::read_value_from_variant] Variant is not JsonObject, trying string fallback");
  // Fallback: plain string ONLY for string-like types (StaticString), NOT List
  // Use SFINAE-friendly check at compile time
  return read_value_from_variant_string_fallback(dst, v, std::integral_constant<bool, is_string_like<T>::value>{});
}


// Fallback for non-TypeAdapter types (strings, scalars, custom with set())
template <typename T>
inline typename std::enable_if<!has_typeadapter_read<T>::value, bool>::type
read_value_from_variant(T& dst, JsonVariant v) {
  if (v.isNull()) return false;

  // Object form: {"value": ...}
  if (v.is<JsonObject>()) {
    JsonObject o = v.as<JsonObject>();

    if (!o["value"].isNull()) {
      JsonVariant vv = o["value"];

      if (vv.is<const char*>()) {
        const char* s = vv.as<const char*>();
        if (!s) return false;
        assign_from_cstr(dst, s);
        return true;
      }

      // Scalars / String can be read directly.
      if (assign_from_variant_scalar(dst, vv, std::integral_constant<bool, is_scalar<T>::value>{}))
        return true;

      if (assign_from_variant_string(dst, vv, std::integral_constant<bool, std::is_same<T, String>::value>{}))
        return true;
    }

    return false;
  }

  // Plain scalar
  if (assign_from_variant_scalar(dst, v, std::integral_constant<bool, is_scalar<T>::value>{}))
    return true;

  // Plain string
  if (assign_from_variant_string(dst, v, std::integral_constant<bool, std::is_same<T, String>::value>{}))
    return true;

  return false;
}

} // namespace detail

template <typename T,
          WsMode WS = WsMode::Value,
          PrefsMode PREFS = PrefsMode::On,
          WriteMode WRITE = WriteMode::On>
class Var {
public:
  typedef T ValueType;

  Var() : value_(), on_change_() {}
  Var(const T& v) : value_(v), on_change_() {}

  const T& get() const { return value_; }
  T&       get()       { return value_; }

  void setOnChange(std::function<void()> cb) { 
    LOG_TRACE_F("[Var] setOnChange callback registered");
    on_change_ = cb; 
  }

  void touch() { 
    LOG_TRACE_F("[Var] touch() called, notifying");
    notify_(); 
  }

  // Generic set (always notifies; keep it simple and predictable)
  template <typename U>
  void set(U&& v) {
    LOG_TRACE_F("[Var::set] Called with new value");
    assign_(std::forward<U>(v));
    LOG_TRACE_F("[Var::set] Assignment completed, calling notify");
    notify_();
    LOG_TRACE_F("[Var::set] Notify completed");
  }

  // Implicit conversions (so existing code keeps working)
  operator const T&() const { return value_; }
  operator T&() { return value_; }

  // Implicit conversion to const char* if underlying type supports it
  template <typename U = T>
  operator typename std::enable_if<detail::has_c_str<U>::value, const char*>::type() const { return value_.c_str(); }

  // c_str passthrough if underlying supports it
  template <typename U = T>
  typename std::enable_if<detail::has_c_str<U>::value, const char*>::type c_str() const { return value_.c_str(); }

  // operator[] passthrough if underlying supports it
  template <typename U = T>
  auto operator[](size_t i) -> decltype(std::declval<U&>()[i]) { return value_[i]; }
  
  template <typename U = T>
  auto operator[](size_t i) const -> decltype(std::declval<const U&>()[i]) { return value_[i]; }

  // Assignment operators
  Var& operator=(const T& v) { set(v); return *this; }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, Var&>::type
  operator=(const String& s) { set(s); return *this; }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, Var&>::type
  operator=(const char* s) { set(s); return *this; }

  // Arithmetic-like ops (only participate if underlying supports them)
  template <typename U>
  auto operator+=(const U& rhs) -> decltype(std::declval<T&>() += rhs, *this) {
    value_ += rhs; notify_(); return *this;
  }

  template <typename U>
  auto operator-=(const U& rhs) -> decltype(std::declval<T&>() -= rhs, *this) {
    value_ -= rhs; notify_(); return *this;
  }

private:
  T value_;
  std::function<void()> on_change_;

  void notify_() { 
    if (on_change_) {
      LOG_TRACE("[Var] Calling on_change callback");
      on_change_(); 
    }
  }

  void assign_(const T& v) { 
    LOG_TRACE_F("[Var::assign_] Direct assignment");
    value_ = v; 
  }
  
  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, void>::type
  assign_(const String& s) { 
    LOG_TRACE_F("[Var::assign_] From String: '%s'", s.c_str());
    detail::assign_from_cstr(value_, s.c_str()); 
  }
  
  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, void>::type
  assign_(const char* s) { 
    LOG_TRACE_F("[Var::assign_] From const char*: '%s'", s ? s : "nullptr");
    detail::assign_from_cstr(value_, s); 
  }

  template <typename U>
  void assign_(U&& v) { 
    LOG_TRACE("[Var::assign_] From rvalue");
    value_ = std::forward<U>(v); 
  }
};

// Convenience aliases
// Convention: Var + WsMode (Ws/Meta) + PrefsMode (Prefs/nothing) + WriteMode (Rw/Ro)
// If PrefsMode::Off, omit "Prefs" from name
template <typename T> using VarWsPrefsRw   = Var<T, WsMode::Value, PrefsMode::On,  WriteMode::On>;
template <typename T> using VarWsRw        = Var<T, WsMode::Value, PrefsMode::Off, WriteMode::On>;
template <typename T> using VarWsPrefsRo   = Var<T, WsMode::Value, PrefsMode::On,  WriteMode::Off>;
template <typename T> using VarWsRo        = Var<T, WsMode::Value, PrefsMode::Off, WriteMode::Off>;
template <typename T> using VarMetaPrefsRw = Var<T, WsMode::Meta,  PrefsMode::On,  WriteMode::On>;
template <typename T> using VarMetaRw      = Var<T, WsMode::Meta,  PrefsMode::Off, WriteMode::On>;
template <typename T> using VarMetaPrefsRo = Var<T, WsMode::Meta,  PrefsMode::On,  WriteMode::Off>;
template <typename T> using VarMetaRo      = Var<T, WsMode::Meta,  PrefsMode::Off, WriteMode::Off>;

// ---- Serializer integration (field-level) ----

template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline void writeOne(const ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject out) {
  LOG_TRACE_F("[writeOne] Var key='%s', WsMode=%d", f.key, (int)WS);
  const Var<T, WS, PREFS, WRITE>& v = (obj.*(f.member));
  if (WS == WsMode::None) {
    LOG_TRACE_F("[writeOne] WsMode=None, skipping");
    return;
  }

  if (WS == WsMode::Meta) {
    LOG_TRACE_F("[writeOne] WsMode=Meta, writing metadata only");
    JsonObject nested = out.createNestedObject(f.key);
    nested["type"] = "secret";
    nested["initialized"] = detail::initialized_of(v.get());
    LOG_TRACE_F("[writeOne] Meta write completed for key='%s'", f.key);
    return;
  }

  // WS == Value
  LOG_TRACE_F("[writeOne] WsMode=Value, writing actual value");
  detail::write_value(out, f.key, v.get());
  LOG_TRACE_F("[writeOne] Value write completed for key='%s'", f.key);
}

template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline void writeOnePrefs(const ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject out) {
  LOG_TRACE_F("[ModelVar] writeOnePrefs called for Var key='%s', WS=%d, PREFS=%d", 
              f.key, (int)WS, (int)PREFS);
  if (PREFS == PrefsMode::Off) {
    LOG_TRACE_F("[ModelVar] Skipping, PREFS=Off");
    return;
  }
  const Var<T, WS, PREFS, WRITE>& v = (obj.*(f.member));
  // For Prefs, ALWAYS write the actual value, not metadata (unlike WebSocket)
  LOG_TRACE_F("[ModelVar] About to write key='%s', getting value from Var", f.key);
  const T& val = v.get();
  // Log the value being written (only for c_str capable types)
  detail::log_value_if_cstr(f.key, val, std::integral_constant<bool, detail::has_c_str<T>::value>{});
  
  LOG_TRACE_F("[ModelVar] Got value, calling write_prefs_value for key='%s'", f.key);
  fj::detail::write_prefs_value(out, f.key, val);
  LOG_TRACE_F("[ModelVar] write_prefs_value completed for key='%s'", f.key);
}

// TypeAdapter-backed vars: avoid string assignment path entirely
template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline typename std::enable_if<detail::has_typeadapter_read<T>::value, bool>::type
readOne(ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject in) {
  LOG_TRACE_F("readOne (Var/TypeAdapter) key='%s', WRITE=%s", f.key, WRITE == WriteMode::Off ? "Off" : "On");
  bool keyExists = in.containsKey(f.key);
  LOG_TRACE_F("[ModelVar] Key '%s' exists in JSON: %s", f.key, keyExists ? "YES" : "NO");

  if (WRITE == WriteMode::Off) {
    JsonVariant probe = in[f.key];
    bool result = probe.isNull();
    LOG_TRACE_F("  -> Read-only, probe.isNull()=%s", result ? "true" : "false");
    return result;
  }

  JsonVariant v = in[f.key];
  if (v.isNull()) {
    LOG_TRACE_F("[ModelVar] Variant for key '%s' is NULL", f.key);
    return false;
  }

  Var<T, WS, PREFS, WRITE>& dst = (obj.*(f.member));

  LOG_TRACE_F("[ModelVar] Variant for key '%s' is JsonObject: %s", f.key, v.is<JsonObject>() ? "YES" : "NO");
  
  if (v.is<JsonObject>()) {
    JsonObject o = v.as<JsonObject>();
    bool hasValue = !o["value"].isNull();
    LOG_TRACE_F("[ModelVar] JsonObject has 'value' field: %s", hasValue ? "YES" : "NO");
    if (hasValue) {
      LOG_TRACE_F("[ModelVar] Reading from nested 'value' field");
      bool result = detail::read_value_from_variant(dst.get(), o["value"]);
      LOG_TRACE_F("[ModelVar] read_value_from_variant returned: %s", result ? "true" : "false");
      return result;
    }
  }

  LOG_TRACE_F("[ModelVar] Reading directly from variant");
  bool result = detail::read_value_from_variant(dst.get(), v);
  LOG_TRACE_F("[ModelVar] read_value_from_variant returned: %s", result ? "true" : "false");
  return result;
}

// Fallback for scalar/string vars (legacy string assignment allowed)
template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline typename std::enable_if<!detail::has_typeadapter_read<T>::value, bool>::type
readOne(ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject in) {
  LOG_TRACE_F("readOne (Var) called for key='%s', WRITE=%s", f.key, WRITE == WriteMode::Off ? "Off" : "On");

  if (WRITE == WriteMode::Off) {
    JsonVariant probe = in[f.key];
    bool result = probe.isNull();
    LOG_TRACE_F("  -> Read-only, probe.isNull()=%s", result ? "true" : "false");
    return result;
  }

  JsonVariant v = in[f.key];

  if (v.isNull()) {
    LOG_TRACE("  -> Variant is NULL");
    return false;
  }

  if (v.is<const char*>()) {
    LOG_TRACE("  -> variant type: string");
  } else if (v.is<JsonObject>()) {
    LOG_TRACE("  -> variant type: object");
  } else {
    LOG_TRACE("  -> variant type: other");
  }

  Var<T, WS, PREFS, WRITE>& dst = (obj.*(f.member));

  if (v.is<JsonObject>()) {
    JsonObject o = v.as<JsonObject>();
    // Accept both {value:...} and (for convenience) raw-like objects where "value" is the actual payload.
    if (!o["value"].isNull()) {
      LOG_TRACE("  -> Object with 'value' key");
      JsonVariant val = o["value"];
      
      // Extract and assign via operator=
      if (val.is<const char*>()) {
        const char* s = val.as<const char*>();
        if (s) {
          LOG_TRACE_F("  -> Assigning from object value: '%s'", s);
          dst = s;
          return true;
        }
      }
      
      // Fallback to old method for non-string types
      bool result = detail::read_value_from_variant(dst.get(), val);
      LOG_TRACE_F("  -> Result: %s", result ? "true" : "false");
      return result;
    }
    // If someone sends the meta object without "value" -> ignore
    LOG_TRACE("  -> No 'value' key, ignoring");
    return true;
  }

  LOG_TRACE("  -> Calling read_value_from_variant with direct variant");
  
  // NEW approach - extract value and assign via operator=
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s) {
      LOG_TRACE("  -> String is NULL");
      return false;
    }
    LOG_TRACE_F("  -> Assigning string: '%s'", s);
    dst = s;  // Use assignment operator
    LOG_TRACE("  -> Value assigned");
    return true;
  }
  
  // For non-string types, try to use dst.get() reference directly
  bool result = detail::read_value_from_variant(dst.get(), v);
  
  LOG_TRACE_F("  -> Result: %s", result ? "true" : "false");
  return result;
}

} // namespace fj

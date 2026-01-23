#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <type_traits>
#include <functional>
#include <cstring>
#include <utility>

#include "../Logger.h"
#include "ModelSerializer.h"
#include "ModelTypeTraits.h"

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

template <typename T>
inline void write_value_impl(JsonObject out, const char* key, const T& v, std::true_type /*has typeadapter*/) {
  // Use TypeAdapter::write_ws
  JsonObject nested = out.createNestedObject(key);
  fj::TypeAdapter<T>::write_ws(v, nested);
}

template <typename T>
inline void write_value_impl_no_adapter(JsonObject out, const char* key, const T& v, std::true_type /*has c_str*/) {
  out[key] = v.c_str();
}

template <typename T>
inline void write_value_impl_no_adapter(JsonObject out, const char* key, const T& v, std::false_type /*no c_str*/) {
  out[key] = v;
}

template <typename T>
inline void write_value_impl(JsonObject out, const char* key, const T& v, std::false_type /*no typeadapter*/) {
  write_value_impl_no_adapter(out, key, v, std::integral_constant<bool, has_c_str<T>::value>{});
}

template <typename T>
inline void write_value(JsonObject out, const char* key, const T& v) {
  write_value_impl(out, key, v, std::integral_constant<bool, has_typeadapter_write_ws<T>::value>{});
}


template <typename T>
inline bool read_value_from_variant(T& dst, JsonVariant v) {
  if (v.isNull()) return false;

  // Object form: {"value": ...} or adapter-specific.
  if (v.is<JsonObject>()) {
    JsonObject o = v.as<JsonObject>();

    // Try project TypeAdapter<T>::read(dst, o, false) if available (priority).
    if (try_typeadapter_read(dst, o, std::integral_constant<bool, has_typeadapter_read<T>::value>{}))
      return true;

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

  // For types with TypeAdapter, don't try string assignment
  if (has_typeadapter_read<T>::value) {
    // Wrap value into object and use adapter
    StaticJsonDocument<512> tmp;
    JsonObject o = tmp.to<JsonObject>();
    o["items"] = v; // For List, expect array in "items"
    return fj::TypeAdapter<T>::read(dst, o, false);
  }

  // Plain string
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s) return false;
    assign_from_cstr(dst, s);
    return true;
  }

  // Scalars / String
  if (assign_from_variant_scalar(dst, v, std::integral_constant<bool, is_scalar<T>::value>{}))
    return true;

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

  void setOnChange(std::function<void()> cb) { on_change_ = cb; }

  void touch() { notify_(); }

  // Generic set (always notifies; keep it simple and predictable)
  template <typename U>
  void set(U&& v) {
    assign_(std::forward<U>(v));
    notify_();
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
  Var& operator=(const String& s) { set(s); return *this; }
  Var& operator=(const char* s) { set(s); return *this; }

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

  void notify_() { if (on_change_) on_change_(); }

  void assign_(const T& v) { value_ = v; }
  
  void assign_(const String& s) { 
    detail::assign_from_cstr(value_, s.c_str()); 
  }
  
  void assign_(const char* s) { 
    detail::assign_from_cstr(value_, s); 
  }

  template <typename U>
  void assign_(U&& v) { 
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
  const Var<T, WS, PREFS, WRITE>& v = (obj.*(f.member));
  if (WS == WsMode::None) return;

  if (WS == WsMode::Meta) {
    JsonObject nested = out.createNestedObject(f.key);
    nested["type"] = "secret";
    nested["initialized"] = detail::initialized_of(v.get());
    return;
  }

  // WS == Value
  detail::write_value(out, f.key, v.get());
}

template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline void writeOnePrefs(const ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject out) {
  if (PREFS == PrefsMode::Off) return;
  const Var<T, WS, PREFS, WRITE>& v = (obj.*(f.member));
  detail::write_value(out, f.key, v.get());
}

template <typename ObjT, typename T, WsMode WS, PrefsMode PREFS, WriteMode WRITE>
inline bool readOne(ObjT& obj, const Field<ObjT, Var<T, WS, PREFS, WRITE>>& f, JsonObject in) {
  LOG_TRACE_F("readOne for Var, key='%s', WRITE=%s", f.key, WRITE == WriteMode::Off ? "Off" : "On");
  
  if (WRITE == WriteMode::Off) {
    // Tolerant ignore (lets strict mode still pass? strict uses return value,
    // but strict here should still reject a write attempt)
    JsonVariant probe = in[f.key];
    bool result = probe.isNull(); // missing => ok, present => reject
    LOG_TRACE_F("  -> Read-only, probe.isNull()=%s", result ? "true" : "false");
    return result;
  }

  JsonVariant v = in[f.key];
  if (v.isNull()) {
    LOG_TRACE("  -> variant is NULL");
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
  
  // OLD approach - doesn't work because dst.get() might not properly modify dst
  // bool result = detail::read_value_from_variant(dst.get(), v);
  
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

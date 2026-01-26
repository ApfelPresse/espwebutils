#pragma once

#include <ArduinoJson.h>
#include <type_traits>
#include <cstring>

#include "../Logger.h"
#include "VarTraits.h"

namespace fj {
namespace detail {

// ---- Value assignment and dispatch helpers ----
// Determine how to assign a value (scalar, string, or complex type).
// Route based on whether type has TypeAdapter, c_str method, or plain assignment.

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

// Path for String type assignment

template <typename T>
inline bool assign_from_variant_string(T& dst, JsonVariant v, std::true_type) {
  dst = v.as<String>();
  return true;
}

template <typename T>
inline bool assign_from_variant_string(T&, JsonVariant, std::false_type) { return false; }

// Path for complex types with TypeAdapter

template <typename T>
inline bool try_typeadapter_read(T& dst, JsonObject o, std::true_type) {
  return fj::TypeAdapter<T>::read(dst, o, false);
}

template <typename T>
inline bool try_typeadapter_read(T&, JsonObject, std::false_type) { return false; }

// ---- Assignment implementation overloads ----
// Called by assign_() in Var class to convert incoming data to target type.

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

// ---- Reading values from JSON variants ----
// Determines whether to parse as TypeAdapter (complex), string fallback, or scalar.

template <typename T>
inline bool read_value_from_variant_string_fallback(T& dst, JsonVariant v, std::true_type /*is_string_like*/) {
  if (!v.is<const char*>()) return false;
  const char* s = v.as<const char*>();
  if (!s) return false;
  dst.set(s);
  return true;
}

template <typename T>
inline bool read_value_from_variant_string_fallback(T&, JsonVariant, std::false_type /*not_string_like*/) {
  return false;
}

// Overload for types that have a TypeAdapter::read -> avoid compiling string assignment paths

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
  return read_value_from_variant_string_fallback(dst, v, std::integral_constant<bool, is_string_like<T>::value>{});
}

// Fallback path for non-TypeAdapter types: handle scalars, strings, and custom types with set()

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
} // namespace fj

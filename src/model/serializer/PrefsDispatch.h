#pragma once

#include <ArduinoJson.h>
#include <type_traits>

#include "../Logger.h"
#include "../var/VarTraits.h"

namespace fj {
namespace detail {

// ---- Preferences write dispatch ----
// SFINAE: detect if TypeAdapter<T>::write_prefs exists

template <typename T>
struct has_typeadapter_write_prefs {
  template <typename U>
  static auto test(int) -> decltype(
    fj::TypeAdapter<U>::write_prefs(std::declval<const U&>(), std::declval<JsonObject>()),
    std::true_type{}
  );
  template <typename>
  static std::false_type test(...);
  static const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
};

// Route through TypeAdapter::write_prefs, then c_str(), then direct assignment

template <typename T>
inline typename std::enable_if<has_typeadapter_write_prefs<T>::value, void>::type
write_prefs_value(JsonObject out, const char* key, const T& val) {
  LOG_TRACE_F("[Serializer] write_prefs_value: Using TypeAdapter::write_prefs for key='%s'", key);
  JsonObject nested = out.createNestedObject(key);
  LOG_TRACE_F("[Serializer] Created nested object, about to call TypeAdapter::write_prefs");
  fj::TypeAdapter<T>::write_prefs(val, nested);
  LOG_TRACE_F("[Serializer] TypeAdapter::write_prefs completed");
}

template <typename T>
inline typename std::enable_if<!has_typeadapter_write_prefs<T>::value && has_c_str<T>::value, void>::type
write_prefs_value(JsonObject out, const char* key, const T& val) {
  LOG_TRACE_F("[Serializer] write_prefs_value: Using c_str for key='%s'", key);
  out[key] = val.c_str();
}

template <typename T>
inline typename std::enable_if<!has_typeadapter_write_prefs<T>::value && !has_c_str<T>::value, void>::type
write_prefs_value(JsonObject out, const char* key, const T& val) {
  LOG_TRACE_F("[Serializer] write_prefs_value: Using direct assignment for key='%s'", key);
  out[key] = val;
}

} // namespace detail
} // namespace fj

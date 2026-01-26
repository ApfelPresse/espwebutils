#pragma once

#include <ArduinoJson.h>
#include <type_traits>
#include <utility>

namespace fj {

// Forward declaration for SFINAE-based detection.
template <typename T>
struct TypeAdapter;

namespace detail {

// ---- SFINAE traits for compile-time type capability detection ----
// These traits allow us to specialize code paths based on whether a type
// has certain methods (c_str, set, isInitialized) without runtime overhead.

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

// Trait to check if T is a "string-like" type in the sense that it supports
// both .set(const char*) and .c_str().
template <typename T>
struct is_string_like {
  static const bool value = has_set_cstr<T>::value && has_c_str<T>::value;
};

// ---- Initialization state detection ----
// Used by WsMode::Meta to determine if a secret field has been set without leaking its value.

template <typename T>
inline bool initialized_of_impl(const T& v, std::true_type /*has isInitialized*/) {
  return v.isInitialized();
}

template <typename T>
inline typename std::enable_if<has_c_str<T>::value, bool>::type
initialized_of_check_cstr(const T& v) {
  const char* s = v.c_str();
  return s && s[0] != '\0';
}

template <typename T>
inline typename std::enable_if<!has_c_str<T>::value, bool>::type
initialized_of_check_cstr(const T&) {
  return true; // Not applicable
}

template <typename T>
inline bool initialized_of_impl(const T& v, std::false_type /*no isInitialized*/) {
  return initialized_of_check_cstr(v);
}

template <typename T>
inline bool initialized_of(const T& v) {
  return initialized_of_impl(v, std::integral_constant<bool, has_isInitialized<T>::value>{});
}

// ---- TypeAdapter capability detection for dispatch ----
// Used to route serialization through specialized adapters (List, PointRingBuffer, Button).

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

} // namespace detail
} // namespace fj

// ModelTypeTraits.h
#pragma once
#include "ModelSerializer.h"

namespace fj {

template <typename T>
struct TypeAdapter {
  static bool defaultPersist() { return true; }
  static bool defaultWsSend()  { return true; }

  static void write(const T& obj, JsonObject out) {
    writeFields(obj, T::schema(), out);
  }

  static bool read(T& obj, JsonObject in, bool strict) {
    return strict ? readFieldsStrict(obj, T::schema(), in)
                  : readFieldsTolerant(obj, T::schema(), in);
  }
};

// -------- SFINAE: detect write_ws / write_prefs --------

template <typename T>
struct HasWriteWs {
  typedef char Yes[1];
  typedef char No[2];

  template <typename U>
  static Yes& test(decltype(&TypeAdapter<U>::write_ws)*);

  template <typename U>
  static No& test(...);

  static const bool value = sizeof(test<T>(0)) == sizeof(Yes);
};

template <typename T>
struct HasWritePrefs {
  typedef char Yes[1];
  typedef char No[2];

  template <typename U>
  static Yes& test(decltype(&TypeAdapter<U>::write_prefs)*);

  template <typename U>
  static No& test(...);

  static const bool value = sizeof(test<T>(0)) == sizeof(Yes);
};

// -------- Dispatch helpers (C++11) --------

template <typename T>
inline void write_ws_impl(const T& obj, JsonObject out, const char(*)[1]) {
  // HasWriteWs == true
  TypeAdapter<T>::write_ws(obj, out);
}

template <typename T>
inline void write_ws_impl(const T& obj, JsonObject out, const char(*)[2]) {
  // HasWriteWs == false
  TypeAdapter<T>::write(obj, out);
}

template <typename T>
inline void write_prefs_impl(const T& obj, JsonObject out, const char(*)[1]) {
  // HasWritePrefs == true
  TypeAdapter<T>::write_prefs(obj, out);
}

template <typename T>
inline void write_prefs_impl(const T& obj, JsonObject out, const char(*)[2]) {
  // HasWritePrefs == false
  TypeAdapter<T>::write(obj, out);
}

// -------- Public API --------

template <typename T>
inline void write_ws(const T& obj, JsonObject out) {
  // Choose overload by type size trick
  write_ws_impl(obj, out, (const char(*)[HasWriteWs<T>::value ? 1 : 2])0);
}

template <typename T>
inline void write_prefs(const T& obj, JsonObject out) {
  write_prefs_impl(obj, out, (const char(*)[HasWritePrefs<T>::value ? 1 : 2])0);
}

} // namespace fj

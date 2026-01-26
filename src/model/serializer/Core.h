#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <type_traits>
#include <cstring>

#include "Logger.h"
#include "Schema.h"
#include "PrefsDispatch.h"
#include "ReadDispatch.h"

namespace fj {

// ============================================================================
// Serialization dispatcher functions: write / read / read tolerant
// ============================================================================

// SFINAE: detect if a type is a Var<> (has ValueType typedef)
namespace detail {
template <typename T>
struct is_var {
  typedef char Yes[1];
  typedef char No[2];

  template <typename U>
  static Yes& test(typename U::ValueType*);

  template <typename U>
  static No& test(...);

  static const bool value = sizeof(test<T>(0)) == sizeof(Yes);
};
} // namespace detail

// Write for non-Var types

template <typename T, typename MemberT>
inline typename std::enable_if<!detail::is_var<MemberT>::value, void>::type
writeOne(const T& obj, const Field<T, MemberT>& f, JsonObject out) {
  out[f.key] = obj.*(f.member);
}

// Write for Var types (generic fallback)
// Note: Var<T,...> is handled by a more specific overload in ModelVar.h.

template <typename T, typename VarT>
inline typename std::enable_if<detail::is_var<VarT>::value, void>::type
writeOne(const T& obj, const Field<T, VarT>& f, JsonObject out) {
  const VarT& var = obj.*(f.member);
  var.write_value(out, f.key);
}

template <typename T, size_t N>
inline void writeOne(const T& obj, const FieldStr<T, N>& f, JsonObject out) {
  out[f.key] = (const char*)(obj.*(f.member));
}

// WritePrefs for non-Var types

template <typename T, typename MemberT>
inline typename std::enable_if<!detail::is_var<MemberT>::value, void>::type
writeOnePrefs(const T& obj, const Field<T, MemberT>& f, JsonObject out) {
  writeOne(obj, f, out);
}

// WritePrefs for Var types (generic fallback)

template <typename T, typename VarT>
inline typename std::enable_if<detail::is_var<VarT>::value, void>::type
writeOnePrefs(const T& obj, const Field<T, VarT>& f, JsonObject out) {
  const VarT& var = obj.*(f.member);
  typedef typename VarT::ValueType ValueType;
  (void)sizeof(ValueType);
  LOG_TRACE_F("[Serializer] writeOnePrefs for Var key='%s', calling write_prefs_value", f.key);
  fj::detail::write_prefs_value(out, f.key, var.get());
}

template <typename T, size_t N>
inline void writeOnePrefs(const T& obj, const FieldStr<T, N>& f, JsonObject out) {
  writeOne(obj, f, out);
}

template <typename T>
struct WriterPrefs {
  const T& obj;
  JsonObject out;
  WriterPrefs(const T& o, JsonObject jo) : obj(o), out(jo) {
    LOG_TRACE_F("[WriterPrefs] Constructor");
  }
  template <typename F>
  void operator()(const F& f) {
    LOG_TRACE_F("[WriterPrefs::operator()] Writing Prefs field key='%s'", f.key);
    writeOnePrefs(obj, f, out);
    LOG_TRACE_F("[WriterPrefs::operator()] Prefs field write completed");
  }
};

template <typename T, typename... Fs>
inline void writeFieldsPrefs(const T& obj, const Schema<T, Fs...>& schema, JsonObject out) {
  LOG_TRACE_F("[ModelSerializer] writeFieldsPrefs starting");
  WriterPrefs<T> w(obj, out);
  tuple_for_each(schema.fields, w);
  LOG_TRACE_F("[ModelSerializer] writeFieldsPrefs completed");
}

// Internal dispatch helpers for readOne
namespace detail {

// Dispatch for non-Var types

template <typename T, typename MemberT>
inline bool readOne_impl(T& obj, const Field<T, MemberT>& f, JsonObject in, std::false_type /*is_var*/) {
  LOG_TRACE("  -> readOne_impl for NON-Var");
  JsonVariant v = in[f.key];
  if (v.isNull()) return false;
  obj.*(f.member) = v.template as<MemberT>();
  return true;
}

// Dispatch for Var types

template <typename T, typename VarT>
inline bool readOne_impl(T& obj, const Field<T, VarT>& f, JsonObject in, std::true_type /*is_var*/) {
  LOG_TRACE("  -> readOne_impl for VAR");
  JsonVariant v = in[f.key];
  if (v.isNull()) {
    LOG_TRACE("  -> variant is NULL, returning false");
    return false;
  }

  if (v.is<const char*>()) {
    LOG_TRACE_F("  -> variant value: '%s'", v.as<const char*>());
  }

  VarT& var = obj.*(f.member);
  bool result = read_var_value(var, v);

  LOG_TRACE_F("  -> read_var_value returned: %s", result ? "true" : "false");
  LOG_TRACE_F("  -> var value after: '%s'", var.get().c_str());

  return result;
}

} // namespace detail

// Public readOne - dispatches based on is_var trait

template <typename T, typename MemberT>
inline bool readOne(T& obj, const Field<T, MemberT>& f, JsonObject in) {
  LOG_TRACE_F("readOne called for key='%s', is_var=%s", f.key, detail::is_var<MemberT>::value ? "YES" : "NO");
  return detail::readOne_impl(obj, f, in, typename std::integral_constant<bool, detail::is_var<MemberT>::value>::type());
}

template <typename T, size_t N>
inline bool readOne(T& obj, const FieldStr<T, N>& f, JsonObject in) {
  LOG_TRACE_F("readOne for FieldStr, key='%s'", f.key);
  JsonVariant v = in[f.key];
  if (v.isNull()) return false;
  const char* s = v.template as<const char*>();
  if (!s) return false;
  std::strncpy((obj.*(f.member)), s, N - 1);
  (obj.*(f.member))[N - 1] = '\0';
  return true;
}

template <typename T>
struct Writer {
  const T& obj;
  JsonObject out;
  Writer(const T& o, JsonObject jo) : obj(o), out(jo) {
    LOG_TRACE_F("[Writer] Constructor");
  }
  template <typename F>
  void operator()(const F& f) {
    LOG_TRACE_F("[Writer::operator()] Writing field key='%s'", f.key);
    writeOne(obj, f, out);
    LOG_TRACE_F("[Writer::operator()] Field write completed");
  }
};

template <typename T>
struct ReaderTolerant {
  T& obj;
  JsonObject in;
  ReaderTolerant(T& o, JsonObject jo) : obj(o), in(jo) {
    LOG_TRACE("ReaderTolerant constructor");
  }
  template <typename F>
  void operator()(const F& f) {
    LOG_TRACE_F("ReaderTolerant::operator() called for key: %s", f.key);
    (void)readOne(obj, f, in);
  }
};

template <typename T>
struct ReaderStrict {
  T& obj;
  JsonObject in;
  bool ok;
  ReaderStrict(T& o, JsonObject jo) : obj(o), in(jo), ok(true) {
    LOG_TRACE_F("[ReaderStrict] Constructor");
  }
  template <typename F>
  void operator()(const F& f) {
    LOG_TRACE_F("[ReaderStrict::operator()] Processing key='%s'", f.key);
    bool result = readOne(obj, f, in);
    LOG_TRACE_F("[ReaderStrict::operator()] readOne returned=%s, ok before=%s", result ? "true" : "false", ok ? "true" : "false");
    ok = result && ok;
    LOG_TRACE_F("[ReaderStrict::operator()] ok after=%s", ok ? "true" : "false");
  }
};

template <typename T, typename... Fs>
inline void writeFields(const T& obj, const Schema<T, Fs...>& schema, JsonObject out) {
  LOG_TRACE_F("[ModelSerializer] writeFields starting");
  Writer<T> w(obj, out);
  tuple_for_each(schema.fields, w);
  LOG_TRACE_F("[ModelSerializer] writeFields completed");
}

template <typename T, typename... Fs>
inline bool readFieldsTolerant(T& obj, const Schema<T, Fs...>& schema, JsonObject in) {
  LOG_TRACE_F("[ModelSerializer] readFieldsTolerant starting");
  ReaderTolerant<T> r(obj, in);
  tuple_for_each(schema.fields, r);
  LOG_TRACE_F("[ModelSerializer] readFieldsTolerant completed");
  return true;
}

template <typename T, typename... Fs>
inline bool readFieldsStrict(T& obj, const Schema<T, Fs...>& schema, JsonObject in) {
  LOG_TRACE_F("[ModelSerializer] readFieldsStrict starting");
  ReaderStrict<T> r(obj, in);
  tuple_for_each(schema.fields, r);
  LOG_TRACE_F("[ModelSerializer] readFieldsStrict completed, ok=%s", r.ok ? "true" : "false");
  return r.ok;
}

template <typename T, size_t JSON_CAPACITY>
inline String to_json(const T& obj) {
  StaticJsonDocument<JSON_CAPACITY> doc;
  JsonObject root = doc.template to<JsonObject>();
  writeFields(obj, T::schema(), root);
  String out;
  serializeJson(doc, out);
  return out;
}

template <typename T, size_t JSON_CAPACITY>
inline bool from_json(const String& json, T& obj, bool strict = false) {
  StaticJsonDocument<JSON_CAPACITY> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  JsonObject root = doc.template as<JsonObject>();
  return strict ? readFieldsStrict(obj, T::schema(), root)
                : readFieldsTolerant(obj, T::schema(), root);
}

} // namespace fj

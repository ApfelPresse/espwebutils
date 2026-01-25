#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <tuple>
#include <cstring>

#include "../Logger.h"

namespace fj {

// Forward declaration of TypeAdapter to avoid cyclic include
template <typename T>
struct TypeAdapter;

// Forward declare SFINAE helpers from ModelVar
namespace detail {
  template <typename T> struct has_c_str;
  template <typename T> struct has_set_cstr;
  template <typename T> struct has_typeadapter_read;
  
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
  
  // SFINAE: detect if a type is a Var<> (has ValueType typedef)
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
  
  // Helper to write value for prefs - dispatch on has TypeAdapter or c_str
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
  
  // Helper to read value for Var - now TypeAdapter-aware (StaticString/List)
  template <typename VarT>
  inline typename std::enable_if<has_typeadapter_read<typename VarT::ValueType>::value, bool>::type
  read_var_value(VarT& var, JsonVariant v) {
    typedef typename VarT::ValueType ValueType;
    if (v.isNull()) return false;

    // If value is wrapped object (common TypeAdapter form)
    if (v.is<JsonObject>()) {
      JsonObject o = v.as<JsonObject>();
      if (!o["value"].isNull()) {
        JsonVariant val = o["value"];
        if (val.is<const char*>() && (has_set_cstr<ValueType>::value || has_c_str<ValueType>::value)) {
          const char* s = val.as<const char*>();
          if (!s) return false;
          var = s;
          return true;
        }
      }
      return fj::TypeAdapter<ValueType>::read(var.get(), o, false);
    }

    // Plain string fallback for adapter types that support c_str/set
    if (v.is<const char*>() && (has_set_cstr<ValueType>::value || has_c_str<ValueType>::value)) {
      const char* s = v.as<const char*>();
      if (!s) return false;
      var = s;
      return true;
    }

    // Allow array shortcut for list-like adapters: wrap into {items: [...]}
    if (v.is<JsonArray>()) {
      StaticJsonDocument<512> tmp;
      JsonObject o = tmp.to<JsonObject>();
      o["items"] = v;
      return fj::TypeAdapter<ValueType>::read(var.get(), o, false);
    }

    // Last resort: direct conversion
    var = v.template as<ValueType>();
    return true;
  }
  
  template <typename VarT>
  inline typename std::enable_if<!has_typeadapter_read<typename VarT::ValueType>::value, bool>::type
  read_var_value(VarT& var, JsonVariant v) {
    const char* s = v.template as<const char*>();
    if (has_c_str<typename VarT::ValueType>::value) {
      if (!s) return false;
      var = s;
      return true;
    }
    var = v.template as<typename VarT::ValueType>();
    return true;
  }
}

template <typename T, typename MemberT>
struct Field {
  const char* key;
  MemberT T::* member;
};

template <typename T, size_t N>
struct FieldStr {
  const char* key;
  char (T::* member)[N];
};

template <typename T, typename... Fs>
struct Schema {
  std::tuple<Fs...> fields;
};

template <typename T, typename... Fs>
inline Schema<T, Fs...> makeSchema(Fs... fs) { return { std::make_tuple(fs...) }; }

// Write for non-Var types
template <typename T, typename MemberT>
inline typename std::enable_if<!detail::is_var<MemberT>::value, void>::type
writeOne(const T& obj, const Field<T, MemberT>& f, JsonObject out) {
  out[f.key] = obj.*(f.member);
}

// Write for Var types - uses write_value which handles TypeAdapter/meta
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

// WritePrefs for Var types - extracts raw value without meta wrapper
template <typename T, typename VarT>
inline typename std::enable_if<detail::is_var<VarT>::value, void>::type
writeOnePrefs(const T& obj, const Field<T, VarT>& f, JsonObject out) {
  const VarT& var = obj.*(f.member);
  typedef typename VarT::ValueType ValueType;
  LOG_TRACE_F("[Serializer] writeOnePrefs for Var key='%s', calling write_prefs_value", f.key);
  detail::write_prefs_value(out, f.key, var.get());
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
  template <typename F> void operator()(const F& f) { 
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
}

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

template <size_t I, typename Tuple, typename Func>
struct TupleForEach {
  static void apply(const Tuple& t, Func& f) {
    TupleForEach<I - 1, Tuple, Func>::apply(t, f);
    f(std::get<I>(t));
  }
};
template <typename Tuple, typename Func>
struct TupleForEach<0, Tuple, Func> {
  static void apply(const Tuple& t, Func& f) { f(std::get<0>(t)); }
};
template <typename Tuple, typename Func>
inline void tuple_for_each(const Tuple& t, Func& f) {
  constexpr size_t N = std::tuple_size<Tuple>::value;
  TupleForEach<N - 1, Tuple, Func>::apply(t, f);
}

template <typename T>
struct Writer {
  const T& obj;
  JsonObject out;
  Writer(const T& o, JsonObject jo) : obj(o), out(jo) {
    LOG_TRACE_F("[Writer] Constructor");
  }
  template <typename F> void operator()(const F& f) { 
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
  template <typename F> void operator()(const F& f) { 
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
  template <typename F> void operator()(const F& f) { 
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

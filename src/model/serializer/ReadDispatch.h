#pragma once

#include <ArduinoJson.h>

#include "../var/VarTraits.h"

namespace fj {
namespace detail {

// ---- Var field value reading: TypeAdapter-aware dispatch ----
// For complex types (List, Button, PointRingBuffer), invokes custom TypeAdapter::read
// For scalars/strings, uses direct assignment

// Path 1: Type has custom TypeAdapter::read (complex types)
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

// Path 2: Type has NO TypeAdapter (scalars, plain strings)
template <typename VarT>
inline typename std::enable_if<!has_typeadapter_read<typename VarT::ValueType>::value, bool>::type
read_var_value(VarT& var, JsonVariant v) {
  typedef typename VarT::ValueType ValueType;
  if (v.isNull()) return false;

  if (v.is<const char*>() && (has_set_cstr<ValueType>::value || has_c_str<ValueType>::value)) {
    const char* s = v.as<const char*>();
    if (!s) return false;
    var = s;
    return true;
  }

  var = v.template as<ValueType>();
  return true;
}

} // namespace detail
} // namespace fj

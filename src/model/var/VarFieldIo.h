#pragma once

#include <ArduinoJson.h>
#include <type_traits>

#include "Var.h"
#include "VarAliases.h"
#include "../serializer/Schema.h"
#include "../serializer/PrefsDispatch.h"

namespace fj {

// ---- Serializer integration: per-field write/read dispatchers ----
// These functions are called by ModelSerializer for each field in a struct.
// They apply the Var's policy (WsMode, PrefsMode, WriteMode) during serialization.

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

// Write Var field to preferences JSON (only if PrefsMode::On)
// Always emits actual value, not metadata (unlike WsMode)

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

// Read Var field from incoming JSON (only if WriteMode::On)
// Routes through TypeAdapter for complex types, or direct assignment for scalars/strings

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

// Fallback path for scalar/string vars: direct assignment allowed (legacy support)
// Does NOT route through TypeAdapter (for compatibility with plain types)

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

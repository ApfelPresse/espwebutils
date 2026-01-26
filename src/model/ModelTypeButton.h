#pragma once
#include <Arduino.h>
#include <cstring>
#include <functional>
#include "ModelSerializer.h"
#include "ModelTypeTraits.h"

struct Button {
  int id;
  std::function<void()> callback;

  Button() : id(0), callback(nullptr) {}
  Button(int _id) : id(_id), callback(nullptr) {}
  Button(int _id, std::function<void()> cb) : id(_id), callback(cb) {}

  // Register a callback to be called when button is triggered
  void setCallback(std::function<void()> cb) {
    callback = cb;
  }

  // Trigger the button (calls registered callback if available)
  void on_trigger() {
    if (callback) {
      callback();
    } else {
      Serial.print("Button triggered (no callback), id=");
      Serial.println(id);
    }
  }

  // Convenient conversions and comparisons
  operator int() const { return id; }
  bool operator==(int v) const { return id == v; }
  bool operator!=(int v) const { return id != v; }
};

namespace fj {

// Direct field serialization support (for fields not wrapped in Var<>)
template <typename T>
inline void writeOne(const T& obj, const Field<T, Button>& f, JsonObject out) {
  JsonObject btnObj = out.createNestedObject(f.key);
  btnObj["type"] = "button";
  btnObj["id"] = (obj.*(f.member)).id;
}

template <typename T>
inline bool readOne(T& obj, const Field<T, Button>& f, JsonObject in) {
  JsonVariant v = in[f.key];
  if (v.isNull()) return false;
  (obj.*(f.member)).id = v.as<int>();
  return true;
}

// TypeAdapter used by Var<Button> for wrapped values
template <>
struct TypeAdapter<Button> {
  static void write_ws(const Button& b, JsonObject out) {
    out["type"] = "button";
    out["id"]   = b.id;
  }

  static void write_prefs(const Button& b, JsonObject out) {
    out["id"] = b.id;
  }

  static bool read(Button& b, JsonObject in, bool /*strict*/) {
    JsonVariant vid = in["id"];
    if (vid.isNull()) return true;
    b.id = vid.as<int>();
    return true;
  }
};

} // namespace fj

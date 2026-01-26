#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypePointRingBuffer.h"
#include "../../src/model/ModelVar.h"
#include <cmath>

namespace PointRingBufferTest {

struct RBWrapper {
  fj::VarWsPrefsRw<PointRingBuffer<3>> rb;

  typedef fj::Schema<RBWrapper,
                     fj::Field<RBWrapper, decltype(rb)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<RBWrapper>(
        fj::Field<RBWrapper, decltype(rb)>{"rb", &RBWrapper::rb});
    return s;
  }
};

void test_ws_serialization_order() {
  TEST_START("PointRingBuffer WS serialization order");

  RBWrapper w;
  w.rb.get().setGraph("g");
  w.rb.get().setLabel("l");

  w.rb.get().push(1, 1.0f);
  w.rb.get().push(2, 2.0f);
  w.rb.get().push(3, 3.0f);
  // Overwrite oldest (1,1.0)
  w.rb.get().push(4, 4.0f);

  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(w, RBWrapper::schema(), root);

  JsonArray values = root["rb"]["values"].as<JsonArray>();
  CUSTOM_ASSERT(values.size() == 3, "Buffer should keep last 3 points");

  uint64_t xs[3] = {0};
  float ys[3] = {0};
  size_t idx = 0;
  for (JsonObject p : values) {
    xs[idx] = p["x"].as<uint64_t>();
    ys[idx] = p["y"].as<float>();
    idx++;
  }

  CUSTOM_ASSERT(xs[0] == 2 && xs[1] == 3 && xs[2] == 4, "Order should be oldest->newest after wrap");
  CUSTOM_ASSERT(std::fabs(ys[0] - 2.0f) < 0.001f, "Y[0] matches");
  CUSTOM_ASSERT(std::fabs(ys[1] - 3.0f) < 0.001f, "Y[1] matches");
  CUSTOM_ASSERT(std::fabs(ys[2] - 4.0f) < 0.001f, "Y[2] matches");

  TEST_END();
}

void test_prefs_roundtrip() {
  TEST_START("PointRingBuffer Prefs roundtrip");

  RBWrapper w;
  w.rb.get().setGraph("g");
  w.rb.get().setLabel("l");
  w.rb.get().push(10, 1.5f);
  w.rb.get().push(20, 2.5f);

  StaticJsonDocument<512> docPrefs;
  JsonObject prefsObj = docPrefs.to<JsonObject>();
  fj::writeFieldsPrefs(w, RBWrapper::schema(), prefsObj);

  String jsonPrefs;
  serializeJson(prefsObj, jsonPrefs);

  StaticJsonDocument<512> docIn;
  DeserializationError err = deserializeJson(docIn, jsonPrefs);
  CUSTOM_ASSERT(!err, "Deserialization should succeed");

  RBWrapper w2;
  bool ok = fj::readFieldsTolerant(w2, RBWrapper::schema(), docIn.as<JsonObject>());
  CUSTOM_ASSERT(ok, "readFieldsTolerant should succeed");

  StaticJsonDocument<512> docWs;
  JsonObject wsObj = docWs.to<JsonObject>();
  fj::writeFields(w2, RBWrapper::schema(), wsObj);
  JsonArray values = wsObj["rb"]["values"].as<JsonArray>();

  CUSTOM_ASSERT(values.size() == 2, "Should restore two values");
  CUSTOM_ASSERT(values[0]["x"] == 10, "First x restored");
  CUSTOM_ASSERT(values[1]["x"] == 20, "Second x restored");
  CUSTOM_ASSERT(std::fabs(values[0]["y"].as<float>() - 1.5f) < 0.001f, "First y restored");
  CUSTOM_ASSERT(std::fabs(values[1]["y"].as<float>() - 2.5f) < 0.001f, "Second y restored");

  TEST_END();
}

void test_push_triggers_callback() {
  TEST_START("PointRingBuffer push triggers callback");

  PointRingBuffer<2> rb;
  struct Ctx { int count = 0; uint64_t x = 0; float y = 0; } ctx;

  rb.setCallback([](const char*, const char*, uint64_t x, float y, void* ctxPtr) {
    Ctx* c = (Ctx*)ctxPtr;
    c->count++;
    c->x = x;
    c->y = y;
  }, &ctx);

  rb.push(123, 4.2f);

  CUSTOM_ASSERT(ctx.count == 1, "Callback should fire once");
  CUSTOM_ASSERT(ctx.x == 123, "Callback x matches");
  CUSTOM_ASSERT(std::fabs(ctx.y - 4.2f) < 0.001f, "Callback y matches");

  TEST_END();
}

void runAllTests() {
  test_ws_serialization_order();
  test_prefs_roundtrip();
  test_push_triggers_callback();
}

} // namespace PointRingBufferTest

#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypePointRingBuffer.h"
#include "../../src/model/ModelVar.h"
#include <cmath>

namespace GraphVarSyncTest {

// Reusable JSON documents to avoid heap allocations in tests
// gTestDoc: general-purpose WS/prefs serializer for small payloads
// gPrefsDoc/gRestoreDoc/gWsDoc: used in reload simulation to keep all allocations on the stack/BSS
// gJsonBuffer: scratch buffer for serializeJson without heap-backed String
static StaticJsonDocument<768> gTestDoc;
static StaticJsonDocument<1024> gPrefsDoc;
static StaticJsonDocument<1024> gRestoreDoc;
static StaticJsonDocument<1024> gWsDoc;
static char gJsonBuffer[512];

struct GraphModel {
  fj::VarWsPrefsRw<PointRingBuffer<4>> graph_data;

  typedef fj::Schema<GraphModel,
                     fj::Field<GraphModel, decltype(graph_data)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<GraphModel>(
        fj::Field<GraphModel, decltype(graph_data)>{"graph_data", &GraphModel::graph_data});
    return s;
  }
};

void test_initial_sync_includes_pushed_data() {
  TEST_START("Initial sync includes pushed data points");

  gTestDoc.clear();  // Reuse global document
  GraphModel model;
  model.graph_data.get().setGraph("admin_events");
  model.graph_data.get().setLabel("auth");

  // Simulate: push some data points (like heap values)
  model.graph_data.get().push(1000, 246132.0f);
  model.graph_data.get().push(2000, 245500.0f);
  model.graph_data.get().push(3000, 244800.0f);

  // Simulate initial sync: serialize the Var as it would be sent to client
  JsonObject root = gTestDoc.to<JsonObject>();
  fj::writeFields(model, GraphModel::schema(), root);

  JsonObject graphObj = root["graph_data"];
  CUSTOM_ASSERT(graphObj.containsKey("values"), "Should have values array");

  JsonArray values = graphObj["values"].as<JsonArray>();
  CUSTOM_ASSERT(values.size() == 3, "Should have all 3 pushed points in initial sync");

  // Verify data order and correctness
  CUSTOM_ASSERT(values[0]["x"] == 1000, "First x point correct");
  CUSTOM_ASSERT(std::fabs(values[0]["y"].as<float>() - 246132.0f) < 1.0f, "First y point correct");

  CUSTOM_ASSERT(values[1]["x"] == 2000, "Second x point correct");
  CUSTOM_ASSERT(std::fabs(values[1]["y"].as<float>() - 245500.0f) < 1.0f, "Second y point correct");

  CUSTOM_ASSERT(values[2]["x"] == 3000, "Third x point correct");

  TEST_END();
}

void test_sync_after_buffer_wrap() {
  TEST_START("Initial sync after buffer wraps around");

  gTestDoc.clear();  // Reuse global document
  GraphModel model;
  model.graph_data.get().setGraph("admin_events");
  model.graph_data.get().setLabel("auth");

  // Push more data than buffer can hold (buffer size = 4)
  model.graph_data.get().push(1000, 100.0f);
  model.graph_data.get().push(2000, 200.0f);
  model.graph_data.get().push(3000, 300.0f);
  model.graph_data.get().push(4000, 400.0f);
  // This push should overwrite the first (1000, 100.0)
  model.graph_data.get().push(5000, 500.0f);

  // Simulate initial sync
  JsonObject root = gTestDoc.to<JsonObject>();
  fj::writeFields(model, GraphModel::schema(), root);

  JsonArray values = root["graph_data"]["values"].as<JsonArray>();
  CUSTOM_ASSERT(values.size() == 4, "Buffer should contain exactly 4 points (newest ones)");

  // Should have points 2,3,4,5 (oldest to newest)
  CUSTOM_ASSERT(values[0]["x"] == 2000, "First remaining x point (oldest)");
  CUSTOM_ASSERT(values[1]["x"] == 3000, "Second remaining x point");
  CUSTOM_ASSERT(values[2]["x"] == 4000, "Third remaining x point");
  CUSTOM_ASSERT(values[3]["x"] == 5000, "Fourth remaining x point (newest)");

  CUSTOM_ASSERT(std::fabs(values[3]["y"].as<float>() - 500.0f) < 0.1f, "Latest y value is correct");

  TEST_END();
}

void test_reload_simulation() {
  TEST_START("Reload simulation: persist then restore");

  // ===== FIRST SESSION: Push data =====
  {
    GraphModel model1;
    model1.graph_data.get().setGraph("admin_events");
    model1.graph_data.get().setLabel("auth");

    // Simulate 3 heap updates coming in every 5 seconds
    model1.graph_data.get().push(5000, 246132.0f);
    model1.graph_data.get().push(10000, 245500.0f);
    model1.graph_data.get().push(15000, 244800.0f);

    // Persist to JSON (would be saved to prefs/SD card)
    gPrefsDoc.clear();
    JsonObject prefsObj = gPrefsDoc.to<JsonObject>();
    fj::writeFieldsPrefs(model1, GraphModel::schema(), prefsObj);

    size_t jsonLen = serializeJson(prefsObj, gJsonBuffer, sizeof(gJsonBuffer));
    CUSTOM_ASSERT(jsonLen < sizeof(gJsonBuffer), "Persisted JSON should fit scratch buffer");

    // ===== PAGE RELOAD: Restore and check =====
    gRestoreDoc.clear();
    DeserializationError err = deserializeJson(gRestoreDoc, gJsonBuffer, jsonLen);
    CUSTOM_ASSERT(!err, "JSON deserialization should succeed");

    GraphModel model2;
    bool ok = fj::readFieldsTolerant(model2, GraphModel::schema(), gRestoreDoc.as<JsonObject>());
    CUSTOM_ASSERT(ok, "Restore should succeed");

    // Now serialize for WS (initial sync to client after reload)
    gWsDoc.clear();
    JsonObject wsObj = gWsDoc.to<JsonObject>();
    fj::writeFields(model2, GraphModel::schema(), wsObj);

    JsonArray values = wsObj["graph_data"]["values"].as<JsonArray>();
    CUSTOM_ASSERT(values.size() == 3, "Restored data should have all 3 points");
    CUSTOM_ASSERT(values[0]["x"] == 5000, "First x point after reload");
    CUSTOM_ASSERT(values[1]["x"] == 10000, "Second x point after reload");
    CUSTOM_ASSERT(values[2]["x"] == 15000, "Third x point after reload");

    CUSTOM_ASSERT(std::fabs(values[0]["y"].as<float>() - 246132.0f) < 1.0f, "First y correct after reload");
    CUSTOM_ASSERT(std::fabs(values[1]["y"].as<float>() - 245500.0f) < 1.0f, "Second y correct after reload");
    CUSTOM_ASSERT(std::fabs(values[2]["y"].as<float>() - 244800.0f) < 1.0f, "Third y correct after reload");
  }  // Cleanup: all local objects destroyed, heap freed

  TEST_END();
}

void test_callback_context_preservation() {
  TEST_START("Callback fires with correct context during push");

  gTestDoc.clear();  // Reuse global document
  GraphModel model;
  model.graph_data.get().setGraph("admin_events");
  model.graph_data.get().setLabel("auth");

  // Track callback invocations
  struct CallbackCtx {
    int call_count = 0;
    uint64_t last_x = 0;
    float last_y = 0.0f;
    const char* last_graph = nullptr;
    const char* last_label = nullptr;
  } cbCtx;

  model.graph_data.get().setCallback(
      [](const char* graph, const char* label, uint64_t x, float y, void* ctx) {
        CallbackCtx* c = (CallbackCtx*)ctx;
        c->call_count++;
        c->last_x = x;
        c->last_y = y;
        c->last_graph = graph;
        c->last_label = label;
      },
      &cbCtx);

  // Push data
  model.graph_data.get().push(50000, 241300.0f);

  CUSTOM_ASSERT(cbCtx.call_count == 1, "Callback should fire once");
  CUSTOM_ASSERT(cbCtx.last_x == 50000, "Callback x correct");
  CUSTOM_ASSERT(std::fabs(cbCtx.last_y - 241300.0f) < 1.0f, "Callback y correct");
  CUSTOM_ASSERT(strcmp(cbCtx.last_graph, "admin_events") == 0, "Graph name correct in callback");
  CUSTOM_ASSERT(strcmp(cbCtx.last_label, "auth") == 0, "Label correct in callback");

  TEST_END();
}

void runAllTests() {
  test_initial_sync_includes_pushed_data();
  test_sync_after_buffer_wrap();
  test_reload_simulation();
  test_callback_context_preservation();
}

} // namespace GraphVarSyncTest

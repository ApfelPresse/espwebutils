#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include "ModelTypeTraits.h"

template <size_t N>
struct PointRingBuffer {
  struct Point {
    uint64_t x;
    float y;
  };

  Point data[N];
  size_t head  = 0;
  size_t count = 0;
  size_t max_count = N;

  char graph_name[24];
  char label[24];

  // Live update callback: (graph, label, x, y, ctx)
  void (*on_push)(const char*, const char*, uint64_t, float, void*) = nullptr;
  void* on_push_ctx = nullptr;

  uint64_t (*now_ms)(void*) = nullptr;
  bool (*is_synced)(void*) = nullptr;
  void* time_ctx = nullptr;

  PointRingBuffer() {
    graph_name[0] = '\0';
    label[0] = '\0';
  }

  PointRingBuffer(const char* graph, const char* lbl) : PointRingBuffer() {
    setGraph(graph);
    setLabel(lbl);
  }

  void setGraph(const char* g) {
    if (!g) { graph_name[0] = '\0'; return; }
    std::strncpy(graph_name, g, sizeof(graph_name) - 1);
    graph_name[sizeof(graph_name) - 1] = '\0';
  }

  void setLabel(const char* l) {
    if (!l) { label[0] = '\0'; return; }
    std::strncpy(label, l, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
  }

  void setCallback(void (*cb)(const char*, const char*, uint64_t, float, void*),
                   void* ctx) {
    on_push = cb;
    on_push_ctx = ctx;
  }

  void setTimeProvider(uint64_t (*nowCb)(void*),
                       bool (*syncedCb)(void*),
                       void* ctx) {
    now_ms = nowCb;
    is_synced = syncedCb;
    time_ctx = ctx;
  }

  bool timeSynced() const {
    return is_synced ? is_synced(time_ctx) : false;
  }

  uint64_t currentX() const {
    if (now_ms) return now_ms(time_ctx);
    return (uint64_t)millis();
  }

  // push(y) → auto timestamp
  void push(float y) {
    push(currentX(), y);
  }

  void push(uint64_t x, float y) {
    data[head].x = x;
    data[head].y = y;

    head = (head + 1) % N;
    if (count < N) count++;

    if (on_push) {
      on_push(graph_name, label, x, y, on_push_ctx);
    }
  }

  // pop oldest
  bool pop(Point& out) {
    if (count == 0) return false;
    const size_t tail = (head + N - count) % N;
    out = data[tail];
    count--;
    return true;
  }
};

namespace fj {

template <size_t N>
struct TypeAdapter<PointRingBuffer<N>> {
  static void write(const PointRingBuffer<N>& rb, JsonObject out) {
    out["type"]   = "graph_xy_ring";
    out["graph"]  = rb.graph_name;
    out["label"]  = rb.label;
    out["size"]   = (int)N;
    out["count"]  = (int)rb.count;
    out["max_count"] = (int)rb.max_count;
    out["synced"] = rb.timeSynced();

    JsonArray values = out.createNestedArray("values");

    const size_t cnt  = rb.count;
    const size_t tail = (rb.head + N - cnt) % N;

    for (size_t i = 0; i < cnt; ++i) {
      const size_t idx = (tail + i) % N;
      JsonObject p = values.createNestedObject();
      p["x"] = (uint64_t)rb.data[idx].x;
      p["y"] = rb.data[idx].y;
    }
  }

  // WS output (same as write)
  static void write_ws(const PointRingBuffer<N>& rb, JsonObject out) {
    write(rb, out);
  }

  // Persist full buffer (same shape as WS output)
  static void write_prefs(const PointRingBuffer<N>& rb, JsonObject out) {
    write(rb, out);
  }

  static bool read(PointRingBuffer<N>& rb, JsonObject in, bool) {
    // Reset state
    rb.head = 0;
    rb.count = 0;

    const char* g = in["graph"] | "";
    const char* l = in["label"] | "";
    rb.setGraph(g);
    rb.setLabel(l);

    if (!in.containsKey("values")) return true;
    JsonArray values = in["values"].as<JsonArray>();
    if (values.isNull()) return true;

    for (JsonObject p : values) {
      if (rb.count >= N) break;
      uint64_t x = p["x"] | 0ULL;
      float    y = p["y"] | 0.0f;
      rb.data[rb.head].x = x;
      rb.data[rb.head].y = y;
      rb.head = (rb.head + 1) % N;
      rb.count++;
    }

    return true;
  }
};

} // namespace fj

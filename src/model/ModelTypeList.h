#pragma once
#include <Arduino.h>
#include <cstring>

#include "ModelSerializer.h"
#include "ModelTypeTraits.h"

template <typename T, size_t N>
struct List {
  T items[N];
  size_t count = 0;

  List() { count = 0; }

  void clear() { count = 0; }

  bool add(const T& item) {
    if (count >= N) return false;
    items[count++] = item;
    return true;
  }

  size_t size() const { return count; }
  size_t capacity() const { return N; }
  bool isFull() const { return count >= N; }

  T& operator[](size_t idx) { return items[idx]; }
  const T& operator[](size_t idx) const { return items[idx]; }

  // Iterator support
  T* begin() { return items; }
  T* end() { return items + count; }
  const T* begin() const { return items; }
  const T* end() const { return items + count; }
};

// TypeAdapter for List<T, N>
namespace fj {

// Forward declare detail namespace from ModelTypeTraits.h
namespace detail {
  template <typename T> struct has_c_str;
  template <typename T> struct has_set_cstr;
}

namespace list_detail {
  // Helper to write list item (has c_str)
  template <typename T>
  typename std::enable_if<detail::has_c_str<T>::value, void>::type
  writeListItem(JsonArray& arr, const T& item) {
    arr.add(item.c_str());
  }

  // Helper to write list item (no c_str)
  template <typename T>
  typename std::enable_if<!detail::has_c_str<T>::value, void>::type
  writeListItem(JsonArray& arr, const T& item) {
    arr.add(item);
  }

  // Helper to read list item (has set)
  template <typename T>
  typename std::enable_if<detail::has_set_cstr<T>::value, T>::type
  readListItem(JsonVariant v) {
    T item;
    item.set(v.as<const char*>());
    return item;
  }

  // Helper to read list item (no set)
  template <typename T>
  typename std::enable_if<!detail::has_set_cstr<T>::value, T>::type
  readListItem(JsonVariant v) {
    return v.as<T>();
  }
}

template <typename T, size_t N>
struct TypeAdapter<List<T, N>> {
  typedef List<T, N> type;
  static const char* type_name() { return "list"; }

  static void write_ws(const List<T, N>& list, JsonObject out) {
    LOG_TRACE_F("[List::write_ws] Writing list with count=%zu, capacity=%zu", list.count, N);
    out["type"] = "list";
    out["count"] = list.count;
    out["capacity"] = N;
    
    JsonArray arr = out.createNestedArray("items");
    for (size_t i = 0; i < list.count; ++i) {
      LOG_TRACE_F("[List::write_ws] Item[%zu]", i);
      list_detail::writeListItem(arr, list.items[i]);
    }
    LOG_TRACE_F("[List::write_ws] Completed");
  }

  static void write_prefs(const List<T, N>& list, JsonObject out) {
    // Same as write_ws for now (lists typically not persisted anyway)
    LOG_TRACE_F("[List::write_prefs] Called, delegating to write_ws");
    write_ws(list, out);
  }

  static bool read(List<T, N>& list, JsonObject in, bool /*strict*/) {
    LOG_TRACE_F("[List::read] Starting, checking for 'items' key");
    
    if (!in.containsKey("items")) {
      LOG_TRACE_F("[List::read] No 'items' key found, returning false");
      return false;
    }
    
    LOG_TRACE_F("[List::read] Found 'items' key, parsing array");
    JsonArray arr = in["items"].as<JsonArray>();
    list.clear();
    
    size_t idx = 0;
    for (JsonVariant v : arr) {
      if (list.isFull()) {
        LOG_TRACE_F("[List::read] List is full at idx=%zu, stopping", idx);
        break;
      }
      LOG_TRACE_F("[List::read] Reading item[%zu]", idx);
      T item = list_detail::readListItem<T>(v);
      list.add(item);
      idx++;
    }
    
    LOG_TRACE_F("[List::read] Completed, final count=%zu", list.count);
    return true;
  }
};

} // namespace fj

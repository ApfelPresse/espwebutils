#pragma once

#include <Arduino.h>
#include <type_traits>
#include <functional>
#include <utility>

#include "../Logger.h"
#include "VarPolicy.h"
#include "VarTraits.h"
#include "VarJsonDispatch.h"

namespace fj {

// ---- Core Var<T> template: policy-based wrapper with notification ----
// Template parameters control serialization behavior:
//   - WsMode:    Value (full data), Meta (only metadata like "initialized"), None (omit from WS)
//   - PrefsMode: On (persist to preferences), Off (transient only)
//   - WriteMode: On (accept remote updates), Off (read-only, reject updates)

template <typename T,
          WsMode WS = WsMode::Value,
          PrefsMode PREFS = PrefsMode::On,
          WriteMode WRITE = WriteMode::On>
class Var {
public:
  typedef T ValueType;

  Var() : value_(), on_change_() {}
  Var(const T& v) : value_(v), on_change_() {}

  const T& get() const { return value_; }
  T&       get()       { return value_; }

  void setOnChange(std::function<void()> cb) {
    LOG_TRACE_F("[Var] setOnChange callback registered");
    on_change_ = cb;
  }

  void touch() {
    LOG_TRACE_F("[Var] touch() called, notifying");
    notify_();
  }

  // Generic set (always notifies; keep it simple and predictable)
  template <typename U>
  void set(U&& v) {
    LOG_TRACE_F("[Var::set] Called with new value");
    assign_(std::forward<U>(v));
    LOG_TRACE_F("[Var::set] Assignment completed, calling notify");
    notify_();
    LOG_TRACE_F("[Var::set] Notify completed");
  }

  // Implicit conversions (so existing code keeps working)
  operator const T&() const { return value_; }
  operator T&() { return value_; }

  // Implicit conversion to const char* if underlying type supports it
  template <typename U = T>
  operator typename std::enable_if<detail::has_c_str<U>::value, const char*>::type() const { return value_.c_str(); }

  // c_str passthrough if underlying supports it
  template <typename U = T>
  typename std::enable_if<detail::has_c_str<U>::value, const char*>::type c_str() const { return value_.c_str(); }

  // operator[] passthrough if underlying supports it
  template <typename U = T>
  auto operator[](size_t i) -> decltype(std::declval<U&>()[i]) { return value_[i]; }

  template <typename U = T>
  auto operator[](size_t i) const -> decltype(std::declval<const U&>()[i]) { return value_[i]; }

  // Assignment operators
  Var& operator=(const T& v) { set(v); return *this; }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, Var&>::type
  operator=(const String& s) { set(s); return *this; }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, Var&>::type
  operator=(const char* s) { set(s); return *this; }

  // Arithmetic-like ops (only participate if underlying supports them)
  template <typename U>
  auto operator+=(const U& rhs) -> decltype(std::declval<T&>() += rhs, *this) {
    value_ += rhs; notify_(); return *this;
  }

  template <typename U>
  auto operator-=(const U& rhs) -> decltype(std::declval<T&>() -= rhs, *this) {
    value_ -= rhs; notify_(); return *this;
  }

private:
  T value_;
  std::function<void()> on_change_;

  void notify_() {
    if (on_change_) {
      LOG_TRACE("[Var] Calling on_change callback");
      on_change_();
    }
  }

  void assign_(const T& v) {
    LOG_TRACE_F("[Var::assign_] Direct assignment");
    value_ = v;
  }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, void>::type
  assign_(const String& s) {
    LOG_TRACE_F("[Var::assign_] From String: '%s'", s.c_str());
    detail::assign_from_cstr(value_, s.c_str());
  }

  template <typename U = T>
  typename std::enable_if<detail::has_set_cstr<U>::value || std::is_assignable<U&, const char*>::value, void>::type
  assign_(const char* s) {
    LOG_TRACE_F("[Var::assign_] From const char*: '%s'", s ? s : "nullptr");
    detail::assign_from_cstr(value_, s);
  }

  template <typename U>
  void assign_(U&& v) {
    LOG_TRACE("[Var::assign_] From rvalue");
    value_ = std::forward<U>(v);
  }
};

} // namespace fj

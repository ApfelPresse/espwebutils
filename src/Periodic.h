#pragma once

#include <functional>
#include <Arduino.h>

class Periodic {
public:
  Periodic(uint32_t intervalMs, std::function<void()> fn = nullptr)
    : _interval(intervalMs), _fn(fn), _last(0) {}

  uint32_t intervalMs() const { return _interval; }

  // Update interval at runtime. If reset==true, the timer restarts from now.
  void setIntervalMs(uint32_t intervalMs, bool reset = false)
  {
    _interval = intervalMs;
    if (reset) _last = millis();
  }
  bool ready()
  {
    uint32_t now = millis();
    if ((uint32_t)(now - _last) >= _interval) {
      _last = now;
      return true;
    }
    return false;
  }

  void run()
  {
    if (_fn && ready()) _fn();
  }

private:
  uint32_t _interval;
  std::function<void()> _fn;
  uint32_t _last;
};

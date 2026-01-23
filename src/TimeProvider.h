#pragma once
#include <Arduino.h>
#include <time.h>

// Returns true if system time is synced (NTP / RTC)
inline bool time_is_synced(void*) {
  time_t now = time(nullptr);
  // 2020-01-01 as sanity check
  return now > 1577836800;
}

// Returns timestamp in ms:
// - epoch ms if synced
// - millis() otherwise
inline uint64_t time_now_ms(void*) {
  if (time_is_synced(nullptr)) {
    time_t sec = time(nullptr);
    return (uint64_t)sec * 1000ULL;
  }
  return (uint64_t)millis();
}

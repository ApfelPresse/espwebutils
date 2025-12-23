#pragma once
#include <WiFi.h>
#include <time.h>

class TimeSync {
public:
  void begin(const char* tz,
             const char* ntp1 = "pool.ntp.org",
             const char* ntp2 = "time.nist.gov",
             const char* ntp3 = "time.google.com") {
    _tz = tz;
    setenv("TZ", tz, 1);
    tzset();
    configTime(0, 0, ntp1, ntp2, ntp3);
  }

  bool isValid() const {
    time_t now = time(nullptr);
    return now > 1700000000;
  }

  double nowEpochSeconds() const {
    return (double)time(nullptr);
  }

  double nowEpochMillis() const {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (double)tv.tv_sec * 1000.0 + (double)(tv.tv_usec / 1000);
  }

  String nowLocalString() const {
    struct tm t;
    if (!getLocalTime(&t)) return "-";
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return String(buf);
  }

private:
  const char* _tz = nullptr;
};

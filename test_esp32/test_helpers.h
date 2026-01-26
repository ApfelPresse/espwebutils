#pragma once
#include <Arduino.h>
#include <cstring>

#include "../src/Logger.h"

// Test statistics
struct TestStats {
  int passed = 0;
  int failed = 0;
  
  void reset() { passed = 0; failed = 0; }
};

extern TestStats globalTestStats;

struct SuiteScope {
  const char* name;
  int passedStart;
  int failedStart;

  explicit SuiteScope(const char* n)
      : name(n), passedStart(globalTestStats.passed), failedStart(globalTestStats.failed) {
    LOG_INFO_F("\n=== SUITE: %s ===", name);
  }

  ~SuiteScope() {
    const int passedDelta = globalTestStats.passed - passedStart;
    const int failedDelta = globalTestStats.failed - failedStart;
    LOG_INFO_F("=== SUITE DONE: %s (passed %d, failed %d) ===\n", name, passedDelta, failedDelta);
  }
};

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

inline const char* test_basename(const char* path) {
  if (!path) return "?";
  const char* lastSlash = std::strrchr(path, '/');
  if (lastSlash && *(lastSlash + 1) != '\0') return lastSlash + 1;
  return path;
}

// Suite-level markers: visible in INFO and above.
// Creates a scope object that prints start + end summary.
#define SUITE_START(name) \
  SuiteScope TEST_CONCAT(_suite_scope_, __LINE__)(name)

// Kept for compatibility with existing call sites; end summary is automatic.
#define SUITE_END(name) \
  do { \
    (void)(name); \
  } while (0)

// Test-level markers: visible in DEBUG and TRACE.
#define TEST_START(name) \
  do { \
    LOG_DEBUG_F("--- Test: %s ---", name); \
  } while (0)

#define TEST_END() \
  do { \
    LOG_TRACE("--- Test end ---"); \
  } while (0)

// Assertions:
// - FAIL is always ERROR (so it shows up even in INFO)
// - PASS is DEBUG (hidden at INFO for clean output)
#define CUSTOM_ASSERT(condition, message) \
  do { \
    if (!(condition)) { \
      LOG_ERROR_F("FAIL: %s (%s:%d)", message, test_basename(__FILE__), __LINE__); \
      globalTestStats.failed++; \
    } else { \
      LOG_DEBUG_F("PASS: %s", message); \
      globalTestStats.passed++; \
    } \
  } while(0)

// Optional helpers for verbose test diagnostics
#define TEST_DEBUG(msg) do { LOG_DEBUG(msg); } while (0)
#define TEST_DEBUG_F(fmt, ...) do { LOG_DEBUG_F(fmt, ##__VA_ARGS__); } while (0)
#define TEST_TRACE(msg) do { LOG_TRACE(msg); } while (0)
#define TEST_TRACE_F(fmt, ...) do { LOG_TRACE_F(fmt, ##__VA_ARGS__); } while (0)

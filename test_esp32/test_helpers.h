#pragma once
#include <Arduino.h>

// Test statistics
struct TestStats {
  int passed = 0;
  int failed = 0;
  
  void reset() { passed = 0; failed = 0; }
};

extern TestStats globalTestStats;

// Simple test macros (using custom names to avoid conflict with Unity framework)
#define CUSTOM_ASSERT(condition, message) \
  do { \
    if (!(condition)) { \
      Serial.print("❌ FAIL: "); \
      Serial.println(message); \
      Serial.print("   at line "); \
      Serial.println(__LINE__); \
      globalTestStats.failed++; \
    } else { \
      Serial.print("✓ PASS: "); \
      Serial.println(message); \
      globalTestStats.passed++; \
    } \
  } while(0)

#define TEST_START(name) \
  Serial.println("\n--- Test: " name " ---")

#define TEST_END() \
  Serial.println("")

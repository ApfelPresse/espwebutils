#pragma once
#include <Arduino.h>

// Log levels
enum class LogLevel {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
  NONE = 5
};

// Global log level - can be changed at runtime
#ifndef LOG_LEVEL
#define LOG_LEVEL LogLevel::INFO
#endif

namespace Logger {
  inline LogLevel& levelRef() {
    static LogLevel level = LOG_LEVEL;
    return level;
  }
  
  inline void setLevel(LogLevel level) {
    levelRef() = level;
  }
  
  inline LogLevel getLevel() {
    return levelRef();
  }
  
  inline const char* levelToString(LogLevel level) {
    switch(level) {
      case LogLevel::TRACE: return "TRACE";
      case LogLevel::DEBUG: return "DEBUG";
      case LogLevel::INFO:  return "INFO";
      case LogLevel::WARN:  return "WARN";
      case LogLevel::ERROR: return "ERROR";
      default: return "NONE";
    }
  }
  
  inline bool shouldLog(LogLevel level) {
    return level >= levelRef();
  }
}

// Logging macros
#define LOG_TRACE(msg) if(Logger::shouldLog(LogLevel::TRACE)) { Serial.print("[TRACE] "); Serial.println(msg); }
#define LOG_DEBUG(msg) if(Logger::shouldLog(LogLevel::DEBUG)) { Serial.print("[DEBUG] "); Serial.println(msg); }
#define LOG_INFO(msg)  if(Logger::shouldLog(LogLevel::INFO))  { Serial.print("[INFO]  "); Serial.println(msg); }
#define LOG_WARN(msg)  if(Logger::shouldLog(LogLevel::WARN))  { Serial.print("[WARN]  "); Serial.println(msg); }
#define LOG_ERROR(msg) if(Logger::shouldLog(LogLevel::ERROR)) { Serial.print("[ERROR] "); Serial.println(msg); }

// Convenience macros for formatted output
#define LOG_TRACE_F(fmt, ...) if(Logger::shouldLog(LogLevel::TRACE)) { Serial.print("[TRACE] "); Serial.printf(fmt, ##__VA_ARGS__); Serial.println(); }
#define LOG_DEBUG_F(fmt, ...) if(Logger::shouldLog(LogLevel::DEBUG)) { Serial.print("[DEBUG] "); Serial.printf(fmt, ##__VA_ARGS__); Serial.println(); }
#define LOG_INFO_F(fmt, ...)  if(Logger::shouldLog(LogLevel::INFO))  { Serial.print("[INFO]  "); Serial.printf(fmt, ##__VA_ARGS__); Serial.println(); }
#define LOG_WARN_F(fmt, ...)  if(Logger::shouldLog(LogLevel::WARN))  { Serial.print("[WARN]  "); Serial.printf(fmt, ##__VA_ARGS__); Serial.println(); }
#define LOG_ERROR_F(fmt, ...) if(Logger::shouldLog(LogLevel::ERROR)) { Serial.print("[ERROR] "); Serial.printf(fmt, ##__VA_ARGS__); Serial.println(); }

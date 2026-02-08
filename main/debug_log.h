#pragma once

// =============================================================================
// Debug Logging Module
// =============================================================================
// Provides severity-level logging over Serial.
// Uses Arduino Serial (Bluepad32 console is disabled in sdkconfig.defaults).
//
// Usage:
//   LOG_INFO("WiFi", "Connected to %s", ssid);
//   LOG_ERROR("CAN", "Transmit failed: %d", err);
// =============================================================================

#include <Arduino.h>
#include "config.h"

// Log level definitions
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

// Initialize the debug logging system (call once in setup)
void debugLogInit();

// Core logging function - prefer the macros below
void debugLog(int level, const char* tag, const char* format, ...);

// ---------------------------------------------------------------------------
// Log ring buffer for web streaming
// ---------------------------------------------------------------------------
#define LOG_RING_SIZE       30      // Number of entries in the ring buffer
#define LOG_ENTRY_MAX_LEN   140     // Max characters per log entry (truncated)

struct LogEntry {
    uint32_t seq;                           // Monotonic sequence number
    char text[LOG_ENTRY_MAX_LEN];           // Pre-formatted log line
};

// Returns the current sequence number (latest entry written)
uint32_t logRingGetHead();

// Copy entries with seq > afterSeq into outBuf (up to maxEntries).
// Returns number of entries copied. Entries are in chronological order.
int logRingGetSince(uint32_t afterSeq, LogEntry* outBuf, int maxEntries);

// Convenience macros with compile-time level filtering
#if LOG_LEVEL >= LOG_LEVEL_ERROR
  #define LOG_ERROR(tag, fmt, ...) debugLog(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_ERROR(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
  #define LOG_WARN(tag, fmt, ...)  debugLog(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_WARN(tag, fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
  #define LOG_INFO(tag, fmt, ...)  debugLog(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_INFO(tag, fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  #define LOG_DEBUG(tag, fmt, ...) debugLog(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

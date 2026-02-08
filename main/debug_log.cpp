// =============================================================================
// Debug Logging Module - Implementation
// =============================================================================

#include "debug_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* levelNames[] = {
    "NONE",   // 0
    "ERROR",  // 1
    "WARN",   // 2
    "INFO",   // 3
    "DEBUG"   // 4
};

// ---------------------------------------------------------------------------
// Ring buffer for web log streaming
// ---------------------------------------------------------------------------
static LogEntry s_ring[LOG_RING_SIZE];
static volatile uint32_t s_ringSeq = 0;     // Next sequence number to assign
static int s_ringWriteIdx = 0;              // Next slot to write into

void debugLogInit() {
    Serial.begin(LOG_SERIAL_BAUD);

    // Wait briefly for serial to be ready
    unsigned long start = millis();
    while (!Serial && (millis() - start < 2000)) {
        delay(10);
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("  JumpRopeStick - Robot Controller");
    Serial.println("  M5StickC Plus 2");
    Serial.println("========================================");
    Serial.printf("Log level: %s (%d)\n", levelNames[LOG_LEVEL], LOG_LEVEL);
    Serial.println();
}

void debugLog(int level, const char* tag, const char* format, ...) {
    if (level > LOG_LEVEL) {
        return;
    }

    // Format: [millis] LEVEL [TAG] message
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Build the full formatted line
    char fullLine[LOG_ENTRY_MAX_LEN];
    snprintf(fullLine, sizeof(fullLine), "[%8lu] %-5s [%-10s] %s",
             millis(), levelNames[level], tag, buffer);

    // Write to serial
    Serial.println(fullLine);

    // Write to ring buffer
    LogEntry& entry = s_ring[s_ringWriteIdx];
    entry.seq = s_ringSeq;
    strncpy(entry.text, fullLine, LOG_ENTRY_MAX_LEN - 1);
    entry.text[LOG_ENTRY_MAX_LEN - 1] = '\0';
    s_ringSeq = s_ringSeq + 1;
    s_ringWriteIdx = (s_ringWriteIdx + 1) % LOG_RING_SIZE;
}

uint32_t logRingGetHead() {
    return s_ringSeq;
}

int logRingGetSince(uint32_t afterSeq, LogEntry* outBuf, int maxEntries) {
    // If nothing new, return 0
    if (s_ringSeq <= afterSeq) {
        return 0;
    }

    // How many new entries exist?
    uint32_t available = s_ringSeq - afterSeq;
    // Can't return more than the ring holds
    if (available > LOG_RING_SIZE) {
        available = LOG_RING_SIZE;
        afterSeq = s_ringSeq - LOG_RING_SIZE;
    }
    // Can't return more than the caller's buffer
    if (available > (uint32_t)maxEntries) {
        // Skip oldest, return the most recent maxEntries
        afterSeq = s_ringSeq - maxEntries;
        available = maxEntries;
    }

    int count = 0;
    for (uint32_t seq = afterSeq; seq < s_ringSeq && count < maxEntries; seq++) {
        // Find the ring index for this sequence number
        // The entry with sequence `seq` was written at index:
        //   (s_ringWriteIdx - (s_ringSeq - seq)) mod LOG_RING_SIZE
        int idx = (int)(s_ringWriteIdx - (int)(s_ringSeq - seq));
        while (idx < 0) { idx += LOG_RING_SIZE; }
        idx = idx % LOG_RING_SIZE;

        if (s_ring[idx].seq == seq) {
            outBuf[count] = s_ring[idx];
            count++;
        }
    }

    return count;
}

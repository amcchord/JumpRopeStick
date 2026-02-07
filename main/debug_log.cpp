// =============================================================================
// Debug Logging Module - Implementation
// =============================================================================

#include "debug_log.h"
#include <stdarg.h>
#include <stdio.h>

static const char* levelNames[] = {
    "NONE",   // 0
    "ERROR",  // 1
    "WARN",   // 2
    "INFO",   // 3
    "DEBUG"   // 4
};

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

    Serial.printf("[%8lu] %-5s [%-10s] %s\n",
                  millis(),
                  levelNames[level],
                  tag,
                  buffer);
}

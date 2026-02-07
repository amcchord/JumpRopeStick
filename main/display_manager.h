#pragma once

// =============================================================================
// Display Manager Module
// =============================================================================
// Renders status information on the M5StickC Plus 2's 135x240 ST7789V2 LCD.
// Uses M5Unified's sprite-based double buffering for flicker-free updates.
//
// Usage:
//   DisplayManager display;
//   display.begin();        // Call once in setup() after M5.begin()
//   display.update();       // Call every loop iteration (rate-limited internally)
// =============================================================================

#include <Arduino.h>

class DisplayManager {
public:
    // Initialize the display and create the sprite buffer.
    // Must be called after M5.begin().
    void begin();

    // Update the display with current status.
    // Rate-limited internally by DISPLAY_UPDATE_MS from config.h.
    void update();

private:
    unsigned long _lastUpdateMs = 0;
    bool _initialized = false;

    // Drawing helpers
    void drawStatusBar(int y);
    void drawControllerInfo(int y);
    void drawOutputInfo(int y);
    void drawSystemInfo(int y);
};

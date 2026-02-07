// =============================================================================
// Display Manager Module - Implementation
// =============================================================================

#include "display_manager.h"
#include "config.h"
#include "debug_log.h"
#include "wifi_manager.h"
#include "controller_manager.h"
#include "drive_manager.h"

#include <M5Unified.h>

static const char* TAG = "Display";

// Colors (RGB565)
static const uint16_t COLOR_BG         = 0x0000;   // Black
static const uint16_t COLOR_HEADER_BG  = 0x1082;   // Dark blue-gray
static const uint16_t COLOR_TEXT       = 0xFFFF;   // White
static const uint16_t COLOR_TEXT_DIM   = 0x8410;   // Gray
static const uint16_t COLOR_ACCENT     = 0x5D9F;   // Soft blue
static const uint16_t COLOR_OK         = 0x07E0;   // Green
static const uint16_t COLOR_WARN       = 0xFD20;   // Orange
static const uint16_t COLOR_ERROR      = 0xF800;   // Red

// Sprite for double-buffered rendering
static M5Canvas sprite(&M5.Display);

// External references
extern WiFiManager g_wifiManager;
extern ControllerManager g_controllerManager;
extern DriveManager g_driveManager;

void DisplayManager::begin() {
    LOG_INFO(TAG, "Initializing display...");

    // Create sprite buffer matching display size
    // M5StickC Plus 2 display is 135x240 in portrait
    sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    sprite.setTextWrap(false);

    _initialized = true;

    // Draw initial screen immediately
    _lastUpdateMs = 0;
    update();

    LOG_INFO(TAG, "Display initialized (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void DisplayManager::update() {
    if (!_initialized) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastUpdateMs < DISPLAY_UPDATE_MS) {
        return;
    }
    _lastUpdateMs = now;

    // Clear the sprite
    sprite.fillSprite(COLOR_BG);

    // Draw sections top to bottom
    int y = 0;
    drawStatusBar(y);
    y += 28;
    drawControllerInfo(y);
    y += 120;
    drawOutputInfo(y);
    y += 48;
    drawSystemInfo(y);

    // Push sprite to display (flicker-free)
    sprite.pushSprite(0, 0);
}

void DisplayManager::drawStatusBar(int y) {
    // WiFi status bar at the top
    sprite.fillRect(0, y, DISPLAY_WIDTH, 26, COLOR_HEADER_BG);

    // WiFi indicator
    bool connected = g_wifiManager.isConnected();
    uint16_t wifiColor = connected ? COLOR_OK : COLOR_ERROR;
    sprite.fillCircle(10, y + 13, 4, wifiColor);

    // IP or status text
    sprite.setTextSize(1);
    sprite.setTextColor(COLOR_TEXT);
    if (connected) {
        sprite.setCursor(20, y + 6);
        sprite.print(g_wifiManager.getIP());

        // RSSI indicator
        int rssi = g_wifiManager.getRSSI();
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(20, y + 16);
        char rssiBuf[16];
        snprintf(rssiBuf, sizeof(rssiBuf), "%ddBm", rssi);
        sprite.print(rssiBuf);
    } else {
        sprite.setCursor(20, y + 9);
        sprite.setTextColor(COLOR_WARN);
        sprite.print("Connecting WiFi...");
    }

}

void DisplayManager::drawControllerInfo(int y) {
    sprite.setTextSize(1);
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setCursor(4, y + 2);
    sprite.print("CONTROLLER");

    int connCount = g_controllerManager.getConnectedCount();

    sprite.setTextColor(COLOR_TEXT_DIM);
    char countBuf[8];
    snprintf(countBuf, sizeof(countBuf), " (%d)", connCount);
    sprite.print(countBuf);

    y += 14;

    if (connCount == 0) {
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y + 20);
        sprite.print("No controller");
        sprite.setCursor(4, y + 32);
        sprite.print("Pair 8BitDo...");
        return;
    }

    // Show first connected controller's state
    for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
        const ControllerState& state = g_controllerManager.getState(i);
        if (!state.connected) {
            continue;
        }

        // Model name
        sprite.setTextColor(COLOR_TEXT);
        sprite.setCursor(4, y);
        sprite.print(state.modelName);
        y += 12;

        // Left stick
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y);
        sprite.print("L:");
        sprite.setTextColor(COLOR_TEXT);
        char stickBuf[24];
        snprintf(stickBuf, sizeof(stickBuf), "%4d,%4d", state.lx, state.ly);
        sprite.print(stickBuf);
        y += 11;

        // Right stick
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y);
        sprite.print("R:");
        sprite.setTextColor(COLOR_TEXT);
        snprintf(stickBuf, sizeof(stickBuf), "%4d,%4d", state.rx, state.ry);
        sprite.print(stickBuf);
        y += 11;

        // Triggers (L2/R2)
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y);
        sprite.print("T:");
        sprite.setTextColor(COLOR_TEXT);
        char trigBuf[24];
        snprintf(trigBuf, sizeof(trigBuf), "L2%4d R2%4d", state.l2, state.r2);
        sprite.print(trigBuf);
        y += 11;

        // Buttons - draw small indicators
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y);
        sprite.print("B:");

        const char* btnLabels[] = {"A","B","X","Y","L1","R1","L3","R3"};
        int bx = 20;
        for (int b = 0; b < 8; b++) {
            bool pressed = (state.buttons & (1 << b)) != 0;
            if (pressed) {
                sprite.setTextColor(COLOR_ACCENT);
            } else {
                sprite.setTextColor(0x3186);  // Very dim
            }
            sprite.setCursor(bx, y);
            sprite.print(btnLabels[b]);
            bx += (b < 4) ? 14 : 16;
        }
        y += 11;

        // D-pad
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setCursor(4, y);
        sprite.print("D:");
        int dx = 20;
        const char* dNames[] = {"U","D","R","L"};
        uint8_t dMasks[] = {1, 2, 4, 8};
        for (int d = 0; d < 4; d++) {
            bool active = (state.dpad & dMasks[d]) != 0;
            sprite.setTextColor(active ? COLOR_ACCENT : 0x3186);
            sprite.setCursor(dx, y);
            sprite.print(dNames[d]);
            dx += 14;
        }

        // Only show first connected controller on the small display
        break;
    }
}

void DisplayManager::drawOutputInfo(int y) {
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setCursor(4, y + 2);
    sprite.setTextSize(1);
    sprite.print("OUTPUTS");
    y += 14;

    // Servo pulse values from drive manager
    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.setCursor(4, y);
    sprite.print("SrvL:");
    sprite.setTextColor(COLOR_TEXT);
    char leftBuf[8];
    snprintf(leftBuf, sizeof(leftBuf), "%u", g_driveManager.getLeftPulse());
    sprite.print(leftBuf);

    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.setCursor(70, y);
    sprite.print("SrvR:");
    sprite.setTextColor(COLOR_TEXT);
    char rightBuf[8];
    snprintf(rightBuf, sizeof(rightBuf), "%u", g_driveManager.getRightPulse());
    sprite.print(rightBuf);
}

void DisplayManager::drawSystemInfo(int y) {
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setCursor(4, y + 2);
    sprite.setTextSize(1);
    sprite.print("SYSTEM");
    y += 14;

    // Uptime
    unsigned long uptimeSec = millis() / 1000;
    unsigned long hours = uptimeSec / 3600;
    unsigned long mins = (uptimeSec % 3600) / 60;
    unsigned long secs = uptimeSec % 60;

    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.setCursor(4, y);
    sprite.print("Up:");
    sprite.setTextColor(COLOR_TEXT);
    char uptimeBuf[16];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%luh%02lum%02lus", hours, mins, secs);
    sprite.print(uptimeBuf);
    y += 11;

    // Free heap
    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.setCursor(4, y);
    sprite.print("Heap:");
    sprite.setTextColor(COLOR_TEXT);
    char heapBuf[16];
    snprintf(heapBuf, sizeof(heapBuf), "%luK", (unsigned long)(ESP.getFreeHeap() / 1024));
    sprite.print(heapBuf);
}

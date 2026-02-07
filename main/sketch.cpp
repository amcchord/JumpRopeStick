// =============================================================================
// JumpRopeStick - Main Application (Arduino setup/loop)
// =============================================================================
// This runs on CPU1. Bluepad32/BTstack runs on CPU0.
// All application modules are initialized and updated here.
// =============================================================================

#include "sdkconfig.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <Bluepad32.h>

#include "config.h"
#include "debug_log.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "controller_manager.h"
#include "display_manager.h"

// ---------------------------------------------------------------------------
// Global module instances
// ---------------------------------------------------------------------------
WiFiManager g_wifiManager;
WebServerManager g_webServer;
// g_controllerManager is defined in controller_manager.cpp
DisplayManager g_displayManager;

// Track whether the web server has been started
static bool s_webServerStarted = false;

// ---------------------------------------------------------------------------
// Arduino setup - runs once on CPU1
// ---------------------------------------------------------------------------
void setup() {
    // -----------------------------------------------------------------------
    // CRITICAL: Set HOLD pin HIGH to keep the device powered on.
    // Without this, the M5StickC Plus 2 will shut down when the button
    // is released.
    // -----------------------------------------------------------------------
    pinMode(PIN_HOLD, OUTPUT);
    digitalWrite(PIN_HOLD, HIGH);

    // Initialize serial debug logging
    debugLogInit();

    // Initialize M5Unified (handles display, IMU, buttons, etc.)
    auto cfg = M5.config();
    M5.begin(cfg);

    LOG_INFO("Main", "M5Unified initialized");

    // Initialize display
    g_displayManager.begin();

    // Initialize WiFi
    g_wifiManager.begin();

    // Initialize Bluepad32 controller manager
    g_controllerManager.begin();

    LOG_INFO("Main", "Setup complete. Entering main loop.");
    LOG_INFO("Main", "Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
    LOG_INFO("Main", "Free PSRAM: %lu bytes", (unsigned long)ESP.getFreePsram());
}

// ---------------------------------------------------------------------------
// Arduino loop - runs repeatedly on CPU1
// ---------------------------------------------------------------------------
void loop() {
    // Update M5 button states
    M5.update();

    // 1. Poll Bluepad32 for controller input
    g_controllerManager.update();

    // 2. Maintain WiFi connection (handles reconnect)
    g_wifiManager.loop();

    // 3. Start web server once WiFi connects for the first time
    if (!s_webServerStarted && g_wifiManager.isConnected()) {
        g_webServer.begin();
        s_webServerStarted = true;
        LOG_INFO("Main", "Web dashboard: http://%s/", g_wifiManager.getIP().c_str());
    }

    // 4. Update the display (rate-limited internally)
    g_displayManager.update();

    // Periodic status log (every 10 seconds)
    static unsigned long lastStatusLog = 0;
    if (millis() - lastStatusLog > 10000) {
        lastStatusLog = millis();
        LOG_INFO("Main", "Status: WiFi=%s, IP=%s, Controllers=%d, WebSrv=%s, Heap=%lu",
                 g_wifiManager.isConnected() ? "yes" : "no",
                 g_wifiManager.getIP().c_str(),
                 g_controllerManager.getConnectedCount(),
                 g_webServer.isRunning() ? "yes" : "no",
                 (unsigned long)ESP.getFreeHeap());
    }

    // Yield to prevent watchdog trigger.
    // Bluepad32 needs the main loop to not block for too long.
    delay(1);
}

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
#include "drive_manager.h"
#include "display_manager.h"

// ---------------------------------------------------------------------------
// Global module instances
// ---------------------------------------------------------------------------
WiFiManager g_wifiManager;
WebServerManager g_webServer;
// g_controllerManager is defined in controller_manager.cpp
DriveManager g_driveManager;
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

    // Initialize servo drive (LEDC PWM on G25/G26)
    g_driveManager.begin();

    LOG_INFO("Main", "Setup complete. Entering main loop.");
    LOG_INFO("Main", "Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
    LOG_INFO("Main", "Free PSRAM: %lu bytes", (unsigned long)ESP.getFreePsram());
}

// ---------------------------------------------------------------------------
// Loop timing instrumentation
// ---------------------------------------------------------------------------
static unsigned long s_loopCount = 0;
static unsigned long s_lastTimingLog = 0;
static unsigned long s_totalLoopUs = 0;
static unsigned long s_maxLoopUs = 0;
static unsigned long s_totalM5Us = 0;
static unsigned long s_totalCtrlUs = 0;
static unsigned long s_totalDriveUs = 0;
static unsigned long s_totalWifiUs = 0;
static unsigned long s_totalDisplayUs = 0;

// ---------------------------------------------------------------------------
// Arduino loop - runs repeatedly on CPU1
// ---------------------------------------------------------------------------
void loop() {
    unsigned long loopStart = micros();
    unsigned long t0, t1;

    // Update M5 button states
    t0 = micros();
    M5.update();
    t1 = micros();
    s_totalM5Us += (t1 - t0);

    // 1. Poll Bluepad32 for controller input
    t0 = micros();
    g_controllerManager.update();
    t1 = micros();
    s_totalCtrlUs += (t1 - t0);

    // 2. Update servo drive (50Hz control loop, rate-limited internally)
    t0 = micros();
    g_driveManager.update();
    t1 = micros();
    s_totalDriveUs += (t1 - t0);

    // 3. Maintain WiFi connection (handles reconnect)
    t0 = micros();
    g_wifiManager.loop();
    t1 = micros();
    s_totalWifiUs += (t1 - t0);

    // 4. Start web server once WiFi connects for the first time
    if (!s_webServerStarted && g_wifiManager.isConnected()) {
        g_webServer.begin();
        s_webServerStarted = true;
        LOG_INFO("Main", "Web dashboard: http://%s/", g_wifiManager.getIP().c_str());
    }

    // 5. Update the display (rate-limited internally)
    t0 = micros();
    g_displayManager.update();
    t1 = micros();
    s_totalDisplayUs += (t1 - t0);

    // Measure total loop time
    unsigned long loopUs = micros() - loopStart;
    s_totalLoopUs += loopUs;
    if (loopUs > s_maxLoopUs) {
        s_maxLoopUs = loopUs;
    }
    s_loopCount++;

    // Log timing every 2 seconds
    unsigned long now = millis();
    if (now - s_lastTimingLog >= 2000) {
        unsigned long count = s_loopCount;
        if (count > 0) {
            unsigned long avgUs = s_totalLoopUs / count;
            unsigned long hz = (count * 1000) / (now - s_lastTimingLog);
            LOG_INFO("Main", "Loop: %lu Hz, avg=%lu us, max=%lu us, n=%lu",
                     hz, avgUs, s_maxLoopUs, count);
            LOG_INFO("Main", "  M5=%lu us, Ctrl=%lu us, Drive=%lu us, WiFi=%lu us, Disp=%lu us (avg per loop)",
                     s_totalM5Us / count, s_totalCtrlUs / count,
                     s_totalDriveUs / count, s_totalWifiUs / count,
                     s_totalDisplayUs / count);
            LOG_INFO("Main", "  Controllers=%d, Heap=%lu",
                     g_controllerManager.getConnectedCount(),
                     (unsigned long)ESP.getFreeHeap());
        }
        // Reset counters
        s_loopCount = 0;
        s_totalLoopUs = 0;
        s_maxLoopUs = 0;
        s_totalM5Us = 0;
        s_totalCtrlUs = 0;
        s_totalDriveUs = 0;
        s_totalWifiUs = 0;
        s_totalDisplayUs = 0;
        s_lastTimingLog = now;
    }

    // Yield to RTOS to prevent watchdog trigger.
    // Using yield() instead of delay(1) to avoid adding 1ms of unnecessary
    // latency per loop iteration. yield() lets other tasks run without blocking.
    yield();
}

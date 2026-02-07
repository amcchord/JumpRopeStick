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
#include "motor_manager.h"
#include "robstride_protocol.h"
#include "display_manager.h"

#include "esp_coexist.h"

// ---------------------------------------------------------------------------
// Global module instances
// ---------------------------------------------------------------------------
WiFiManager g_wifiManager;
WebServerManager g_webServer;
// g_controllerManager is defined in controller_manager.cpp
DriveManager g_driveManager;
MotorManager g_motorManager;
DisplayManager g_displayManager;

// Track whether the web server has been started
static bool s_webServerStarted = false;

// ---------------------------------------------------------------------------
// Motor trim state (extern-accessible for display_manager)
// ---------------------------------------------------------------------------
float g_trimTargetLeft = 0.0f;
float g_trimTargetRight = 0.0f;

// Edge detection for d-pad and misc buttons
static uint8_t s_prevDpad = 0;
static uint16_t s_prevMiscButtons = 0;

// Repeat rate: time between trim steps while d-pad is held (ms)
static const unsigned long TRIM_REPEAT_MS = 150;
static unsigned long s_lastTrimStepMs = 0;

// Debounce for Sys button (ms)
static const unsigned long SYS_DEBOUNCE_MS = 500;
static unsigned long s_lastSysMs = 0;

// Track which motor CAN IDs have been initialized for trim.
// Stored as CAN IDs (not bools) so we can detect when a motor changes.
static uint8_t s_trimInitLeftId = 0;
static uint8_t s_trimInitRightId = 0;

// Track which motors have been auto-zeroed at startup.
// On first connect we assume arms are in front and set mechanical zero.
// These are NEVER reset (except on reboot) so we only zero once.
static uint8_t s_autoZeroedLeftId = 0;
static uint8_t s_autoZeroedRightId = 0;

// Track previous controller connection state for disconnect detection
static bool s_controllerWasConnected = false;

// ---------------------------------------------------------------------------
// Arm preset definitions
// ---------------------------------------------------------------------------
struct ArmPreset {
    float leftPos;
    float rightPos;   // In target space (before right motor negation)
    const char* name;
};

// Home positions that L1 cycles through
static const ArmPreset HOME_PRESETS[] = {
    {  0.0f,    0.0f,   "Front" },
    { -1.79f,  -1.79f,  "Up" },
    { -3.54f,  -3.54f,  "Back" },
    {  0.0f,   -3.54f,  "L-Front/R-Back" },
};
static const int HOME_PRESET_COUNT = 4;

// R2 trigger target for each home preset (at full pull)
static const ArmPreset TRIGGER_TARGETS[] = {
    { -1.79f,  -1.79f,  "Up" },             // Front -> Up
    {  0.55f,  -4.15f,  "Stand" },           // Up -> Stand on Arms
    { -1.79f,  -1.79f,  "Up" },             // Back -> Up
    { -3.54f,   0.0f,   "R-Front/L-Back" }, // L-front/R-back -> swap
};

// ---------------------------------------------------------------------------
// Stick control state
// ---------------------------------------------------------------------------
static float s_basePosition = 0.0f;          // Accumulated Y-axis jog position (rad)
static unsigned long s_lastStickUpdateMs = 0; // Rate limiter
static uint16_t s_prevButtons = 0;            // For button edge detection
static int s_homePresetIndex = 0;             // Current home preset (0-3)
static float s_zeroOffset = 0.0f;            // Accumulated zero-point offset from L3 resets

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

    // Balance WiFi and Bluetooth in the coexistence arbiter.
    // ESP32 shares a single 2.4GHz radio between WiFi and BT.
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    LOG_INFO("Main", "Coex preference set to BALANCE");

    // Initialize Bluepad32 controller manager
    g_controllerManager.begin();

    // Initialize DSHOT drive (spawns drive task on CPU0)
    g_driveManager.begin();

    // Initialize CAN bus motor manager (TWAI + motor scan)
    g_motorManager.begin();

    LOG_INFO("Main", "Setup complete. Entering main loop.");
    LOG_INFO("Main", "Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
    LOG_INFO("Main", "Free PSRAM: %lu bytes", (unsigned long)ESP.getFreePsram());
}

// ---------------------------------------------------------------------------
// Motor trim processing
// ---------------------------------------------------------------------------
// D-pad bitmask values (from Bluepad32):
//   Up=1, Down=2, Right=4, Left=8
// Misc button bitmask:
//   System=0x01
// ---------------------------------------------------------------------------

static void initMotorForTrim(uint8_t motorId, bool setZero) {
    // Stop motor first to ensure clean RESET state, clearing any faults
    g_motorManager.stopMotor(motorId, true);
    delay(10);

    // On first connect, assume arms are in front and set current position as zero
    if (setZero) {
        g_motorManager.setMechanicalZero(motorId);
        LOG_INFO("Trim", "Auto-zeroed motor %d (arms assumed in front)", motorId);
        delay(10);
    }

    // Set run mode to Position PP (interpolated position control)
    g_motorManager.writeUint8Param(motorId, RobstrideParam::RUN_MODE,
                                   RobstrideMode::POSITION_PP);
    delay(10);

    // Enable the motor
    g_motorManager.enableMotor(motorId);
    LOG_INFO("Trim", "Initialized motor %d for trim (stop%s->PP->enable)",
             motorId, setZero ? "->zero" : "");
}

// Check if a motor is in RUNNING state. If not (e.g. after power cycle),
// re-initialize it for trim. Returns true if the motor is ready.
// autoZeroTracker persists across reconnects (never reset) so we only
// set mechanical zero once per boot, assuming arms start in front.
static bool ensureMotorReady(uint8_t motorId, uint8_t& initTracker,
                             uint8_t& autoZeroTracker) {
    int idx = -1;
    for (int i = 0; i < g_motorManager.getMotorCount(); i++) {
        if (g_motorManager.getMotorId(i) == motorId) {
            idx = i;
            break;
        }
    }

    // Motor not found on bus -- can't trim
    if (idx < 0) {
        return false;
    }

    const RobstrideMotorStatus& status = g_motorManager.getMotorStatus(idx);

    // If motor is stale (no recent feedback), skip
    if (status.stale) {
        return false;
    }

    // If motor is already running and we've initialized this ID, it's ready
    if (status.enabled && initTracker == motorId) {
        return true;
    }

    // Motor is not running (RESET after power cycle, fault, etc.) -- re-init
    // Only set mechanical zero on the very first initialization after boot
    bool needsZero = (autoZeroTracker != motorId);
    initMotorForTrim(motorId, needsZero);
    initTracker = motorId;
    if (needsZero) {
        autoZeroTracker = motorId;
    }
    return true;
}

// Resolve the CAN ID for the "left" motor.
// Uses the explicit role assignment if set, otherwise falls back to motor index 0.
static uint8_t resolveLeftMotorId() {
    uint8_t id = g_motorManager.getLeftMotorId();
    if (id > 0) {
        return id;
    }
    // Fallback: use first discovered motor if available
    if (g_motorManager.getMotorCount() >= 1) {
        return g_motorManager.getMotorId(0);
    }
    return 0;
}

// Resolve the CAN ID for the "right" motor.
// Uses the explicit role assignment if set, otherwise falls back to motor index 1.
static uint8_t resolveRightMotorId() {
    uint8_t id = g_motorManager.getRightMotorId();
    if (id > 0) {
        return id;
    }
    // Fallback: use second discovered motor if available
    if (g_motorManager.getMotorCount() >= 2) {
        return g_motorManager.getMotorId(1);
    }
    return 0;
}

static void processTrim() {
    // Get the first connected controller's state
    const ControllerState& state = g_controllerManager.getState(0);
    if (!state.connected) {
        // Safety: if controller just disconnected, stop all motors so they go slack
        if (s_controllerWasConnected) {
            uint8_t leftId = resolveLeftMotorId();
            uint8_t rightId = resolveRightMotorId();
            if (leftId > 0) {
                g_motorManager.stopMotor(leftId, false);
                LOG_INFO("Trim", "Controller lost -- stopped left motor (ID %d)", leftId);
            }
            if (rightId > 0) {
                g_motorManager.stopMotor(rightId, false);
                LOG_INFO("Trim", "Controller lost -- stopped right motor (ID %d)", rightId);
            }
            // Reset trim and stick state so motors get re-initialized on reconnect
            s_trimInitLeftId = 0;
            s_trimInitRightId = 0;
            g_trimTargetLeft = 0.0f;
            g_trimTargetRight = 0.0f;
            s_basePosition = 0.0f;
            s_zeroOffset = 0.0f;
            s_homePresetIndex = 0;
            s_controllerWasConnected = false;
        }
        s_prevDpad = 0;
        s_prevMiscButtons = 0;
        return;
    }

    // Track that a controller is connected (for disconnect detection)
    s_controllerWasConnected = true;

    uint8_t dpad = state.dpad;
    uint16_t misc = state.miscButtons;

    // Detect rising edges for Sys button (edge-triggered, not repeating)
    uint16_t miscPressed = misc & ~s_prevMiscButtons;

    // Update previous state for next frame
    s_prevDpad = dpad;
    s_prevMiscButtons = misc;

    unsigned long now = millis();

    // Resolve motor IDs (explicit role or fallback to discovered index)
    uint8_t leftId = resolveLeftMotorId();
    uint8_t rightId = resolveRightMotorId();

    // --- D-pad: nudge motors (repeats while held) ---
    // Any d-pad direction currently held triggers a trim step,
    // rate-limited by TRIM_REPEAT_MS.
    if (dpad & 0x0F) {
        if (now - s_lastTrimStepMs >= TRIM_REPEAT_MS) {
            // D-pad Up: nudge left motor positive
            if (dpad & 0x01) {
                if (leftId > 0 && ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
                    g_trimTargetLeft += TRIM_STEP_RAD;
                    g_motorManager.writeFloatParam(leftId, RobstrideParam::LOC_REF,
                                                   g_trimTargetLeft);
                    LOG_INFO("Trim", "Left motor (ID %d) target: %.3f rad", leftId, g_trimTargetLeft);
                }
            }

            // D-pad Down: nudge left motor negative
            if (dpad & 0x02) {
                if (leftId > 0 && ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
                    g_trimTargetLeft -= TRIM_STEP_RAD;
                    g_motorManager.writeFloatParam(leftId, RobstrideParam::LOC_REF,
                                                   g_trimTargetLeft);
                    LOG_INFO("Trim", "Left motor (ID %d) target: %.3f rad", leftId, g_trimTargetLeft);
                }
            }

            // D-pad Right: nudge right motor positive
            if (dpad & 0x04) {
                if (rightId > 0 && ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
                    g_trimTargetRight += TRIM_STEP_RAD;
                    g_motorManager.writeFloatParam(rightId, RobstrideParam::LOC_REF,
                                                   -g_trimTargetRight);
                    LOG_INFO("Trim", "Right motor (ID %d) target: %.3f rad", rightId, g_trimTargetRight);
                }
            }

            // D-pad Left: nudge right motor negative
            if (dpad & 0x08) {
                if (rightId > 0 && ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
                    g_trimTargetRight -= TRIM_STEP_RAD;
                    g_motorManager.writeFloatParam(rightId, RobstrideParam::LOC_REF,
                                                   -g_trimTargetRight);
                    LOG_INFO("Trim", "Right motor (ID %d) target: %.3f rad", rightId, g_trimTargetRight);
                }
            }

            s_lastTrimStepMs = now;
        }
    }

    // --- Sys button: set mechanical zero on both motors (edge + debounce) ---
    if ((miscPressed & 0x01) && (now - s_lastSysMs >= SYS_DEBOUNCE_MS)) {
        // Reset all position state FIRST so processStickControl() won't
        // immediately send a stale offset against the new zero
        g_trimTargetLeft = 0.0f;
        g_trimTargetRight = 0.0f;
        s_basePosition = 0.0f;
        s_zeroOffset = 0.0f;
        s_homePresetIndex = 0;

        // Send set-mechanical-zero commands
        if (leftId > 0) {
            g_motorManager.setMechanicalZero(leftId);
            LOG_INFO("Trim", "Set mechanical zero on left motor (ID %d)", leftId);
        }
        if (rightId > 0) {
            g_motorManager.setMechanicalZero(rightId);
            LOG_INFO("Trim", "Set mechanical zero on right motor (ID %d)", rightId);
        }

        // Give the motors time to process the zero-set before sending position
        delay(250);

        // Command motors to hold at position 0 (the new zero)
        if (leftId > 0) {
            g_motorManager.writeFloatParam(leftId, RobstrideParam::LOC_REF, 0.0f);
        }
        if (rightId > 0) {
            g_motorManager.writeFloatParam(rightId, RobstrideParam::LOC_REF, 0.0f);
        }

        s_lastSysMs = now;
        LOG_INFO("Trim", "All position state reset to zero.");
    }
}

// ---------------------------------------------------------------------------
// Left stick motor control
// ---------------------------------------------------------------------------
// Left stick Y: velocity jog (both motors move together, preserves asymmetry)
// Left stick X: positional difference between motors (0 to PI rad)
// L3 button:    smart return to home (nearest safe front, above/below aware)
// L1 button:    cycle through home presets (Front, Up, Back, L-Front/R-Back)
// R2 trigger:   interpolate from home position to trigger target
// ---------------------------------------------------------------------------
static void processStickControl() {
    const ControllerState& state = g_controllerManager.getState(0);
    if (!state.connected) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - s_lastStickUpdateMs;
    if (elapsed < STICK_UPDATE_MS) {
        return;
    }
    s_lastStickUpdateMs = now;

    float dt = elapsed / 1000.0f;

    // Resolve motor IDs
    uint8_t leftId = resolveLeftMotorId();
    uint8_t rightId = resolveRightMotorId();
    if (leftId == 0 && rightId == 0) {
        return;
    }

    // Detect button rising edges
    uint16_t buttons = state.buttons;
    uint16_t buttonsPressed = buttons & ~s_prevButtons;
    s_prevButtons = buttons;

    // --- L3: smart return to home (nearest safe front, above/below aware) ---
    if (buttonsPressed & 0x0100) {
        float TWO_PI = 2.0f * PI;
        float frac = fmodf(s_basePosition, TWO_PI);
        if (frac > PI)  { frac -= TWO_PI; }
        if (frac < -PI) { frac += TWO_PI; }

        // Arms above (frac <= 0): complete rotation forward via ceil (stays above)
        // Arms below (frac > 0):  go back via floor (stays below)
        float nearestHome;
        if (frac <= 0.0f) {
            nearestHome = ceilf(s_basePosition / TWO_PI) * TWO_PI;
        } else {
            nearestHome = floorf(s_basePosition / TWO_PI) * TWO_PI;
        }

        s_zeroOffset += nearestHome;
        s_basePosition = 0.0f;
        LOG_INFO("Stick", "L3: return to home (offset=%.2f)", s_zeroOffset);
    }

    // --- L1: cycle through home presets ---
    if (buttonsPressed & 0x0010) {
        s_homePresetIndex = (s_homePresetIndex + 1) % HOME_PRESET_COUNT;
        s_basePosition = 0.0f;
        s_zeroOffset = 0.0f;
        LOG_INFO("Stick", "L1: home -> %s", HOME_PRESETS[s_homePresetIndex].name);
    }

    // --- R2 trigger: interpolate between home and trigger target ---
    const ArmPreset& home = HOME_PRESETS[s_homePresetIndex];
    const ArmPreset& trig = TRIGGER_TARGETS[s_homePresetIndex];

    float triggerNorm = 0.0f;
    int16_t r2val = state.r2;
    if (r2val > R2_TRIGGER_DEADZONE) {
        triggerNorm = (float)(r2val - R2_TRIGGER_DEADZONE) / (1023.0f - R2_TRIGGER_DEADZONE);
        if (triggerNorm > 1.0f) { triggerNorm = 1.0f; }
    }

    float homeLeftPos  = home.leftPos  + (trig.leftPos  - home.leftPos)  * triggerNorm;
    float homeRightPos = home.rightPos + (trig.rightPos - home.rightPos) * triggerNorm;

    // Apply dead zone to stick axes
    int16_t ly = state.ly;
    int16_t lx = state.lx;
    if (abs(ly) < CONTROLLER_DEADZONE) {
        ly = 0;
    }
    if (abs(lx) < CONTROLLER_DEADZONE) {
        lx = 0;
    }

    // Accumulate base position from Y-axis jog
    if (ly != 0) {
        s_basePosition += (ly / 512.0f) * STICK_MAX_JOG_RAD_S * dt;
    }

    // Compute positional difference from X-axis with deadband rescale + expo
    // Rescale lx so the range [DEADZONE..512] maps smoothly to [0..1]
    float normLx = 0.0f;
    if (lx != 0) {
        float absLx = fabsf((float)lx);
        float maxRange = 512.0f - CONTROLLER_DEADZONE;
        float rescaled = (absLx - CONTROLLER_DEADZONE) / maxRange;
        if (rescaled > 1.0f) {
            rescaled = 1.0f;
        }
        // Apply expo: blend linear and cubic for fine control near center
        float cubic = rescaled * rescaled * rescaled;
        float expoOut = (1.0f - DRIVE_EXPO) * rescaled + DRIVE_EXPO * cubic;
        // Restore sign
        if (lx < 0) {
            normLx = -expoOut;
        } else {
            normLx = expoOut;
        }
    }
    float difference = normLx * PI;

    // Compute final targets: preset + trigger + jog + difference + trim + offset
    float leftTarget  = homeLeftPos  + s_basePosition + difference / 2.0f + g_trimTargetLeft  + s_zeroOffset;
    float rightTarget = homeRightPos + s_basePosition - difference / 2.0f + g_trimTargetRight + s_zeroOffset;

    // Ensure motors are ready and send position commands
    if (leftId > 0) {
        if (ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
            g_motorManager.writeFloatParam(leftId, RobstrideParam::LOC_REF, leftTarget);
        }
    }
    if (rightId > 0) {
        if (ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
            g_motorManager.writeFloatParam(rightId, RobstrideParam::LOC_REF, -rightTarget);
        }
    }
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
static unsigned long s_totalWifiUs = 0;
static unsigned long s_totalDisplayUs = 0;
static unsigned long s_totalMotorUs = 0;

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

    // (Drive runs on its own FreeRTOS task on CPU0 -- no update() call needed)

    // 1b. Process motor trim (d-pad nudge + Sys zero)
    processTrim();

    // 1c. Process left stick motor control (jog + difference)
    processStickControl();

    // 2. Poll CAN bus for motor feedback
    t0 = micros();
    g_motorManager.poll();
    t1 = micros();
    s_totalMotorUs += (t1 - t0);

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
            LOG_INFO("Main", "  M5=%lu us, Ctrl=%lu us, Motor=%lu us, WiFi=%lu us, Disp=%lu us (avg)",
                     s_totalM5Us / count, s_totalCtrlUs / count,
                     s_totalMotorUs / count,
                     s_totalWifiUs / count, s_totalDisplayUs / count);
            LOG_INFO("Main", "  Controllers=%d, Motors=%d, Heap=%lu",
                     g_controllerManager.getConnectedCount(),
                     g_motorManager.getMotorCount(),
                     (unsigned long)ESP.getFreeHeap());
        }
        // Reset counters
        s_loopCount = 0;
        s_totalLoopUs = 0;
        s_maxLoopUs = 0;
        s_totalM5Us = 0;
        s_totalCtrlUs = 0;
        s_totalWifiUs = 0;
        s_totalDisplayUs = 0;
        s_totalMotorUs = 0;
        s_lastTimingLog = now;
    }

    // Yield to RTOS to prevent watchdog trigger.
    // Using yield() instead of delay(1) to avoid adding 1ms of unnecessary
    // latency per loop iteration. yield() lets other tasks run without blocking.
    yield();
}

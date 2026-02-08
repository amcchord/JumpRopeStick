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
#include "settings_manager.h"

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
SettingsManager g_settingsManager;

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
    {  0.65f,  -4.25f,  "Stand" },           // Up -> Stand on Arms
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
// IMU state
// ---------------------------------------------------------------------------
volatile bool g_isUpsideDown = false;         // Cross-core flag for drive inversion
static float s_referenceAccelX = 0.0f;        // Captured at boot (gravity axis reference)
static float s_pitchAngle = 0.0f;             // Current pitch in radians (0=level, neg=nose-down)
static float s_gyroPitchRate = 0.0f;          // Gyro pitch rate for PID D-term (rad/s)
static unsigned long s_lastImuMs = 0;

// Web-accessible copies of state (read by web_server.cpp)
float g_pitchAngleForWeb = 0.0f;
int g_selfRightStateForWeb = 0;
int g_noseDownStateForWeb = 0;

// ---------------------------------------------------------------------------
// Self-righting state machine (Select button)
// ---------------------------------------------------------------------------
enum SelfRightState { SR_IDLE, SR_PREP, SR_PUSH, SR_DONE };
static SelfRightState s_selfRightState = SR_IDLE;
static unsigned long s_selfRightMs = 0;       // Timestamp for state transitions
static bool s_prevSelectBtn = false;          // Edge detection for Select button

// ---------------------------------------------------------------------------
// Nose-down balance state machine (X button)
// ---------------------------------------------------------------------------
enum NoseDownState { ND_IDLE, ND_SELF_RIGHTING, ND_TIPPING, ND_BALANCING, ND_EXITING };
static NoseDownState s_noseDownState = ND_IDLE;
static unsigned long s_noseDownMs = 0;        // Timestamp for state transitions
static unsigned long s_balanceStartMs = 0;    // When PID balancing began
static float s_rampProgress = 0.0f;          // Pitch-gated ramp [0..1] (replaces time-based ramp)
static unsigned long s_lastRampMs = 0;       // Last time we advanced the ramp
static bool s_prevXBtn = false;               // Edge detection for X button

// PID state for nose-down balance
static float s_pidIntegral = 0.0f;
static float s_pidPrevError = 0.0f;

// Nose-down self-righting sub-state (reuses SR logic)
enum NdSelfRightSub { NDSR_PREP, NDSR_PUSH, NDSR_DONE };
static NdSelfRightSub s_ndSrSub = NDSR_PREP;
static unsigned long s_ndSrMs = 0;

// Arm positions captured at nose-down exit start
static float s_exitStartLeft = 0.0f;
static float s_exitStartRight = 0.0f;

// Pitch confirmation counter for tipping -> balancing transition
static int s_pitchConfirmCount = 0;

// ---------------------------------------------------------------------------
// Ground Slap state machine (triggered by button mode 3)
// ---------------------------------------------------------------------------
// Oscillates arms between -0.10 and +0.10 rad, 3 cycles, ending at 0.0
static const int GROUND_SLAP_CYCLES = 3;
static const float GROUND_SLAP_AMP = 0.10f;
static const unsigned long GROUND_SLAP_HALF_MS = 75; // time per half-cycle

enum GroundSlapState { GS_IDLE, GS_RUNNING };
static GroundSlapState s_groundSlapState = GS_IDLE;
static unsigned long s_groundSlapMs = 0;    // timestamp of last phase change
static int s_groundSlapPhase = 0;           // 0..5 for 3 cycles (2 phases each)
static const int GROUND_SLAP_TOTAL_PHASES = GROUND_SLAP_CYCLES * 2;

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

    // Capture IMU reference orientation (robot is flat and level at boot)
    // Axis mapping: X = vertical (gravity), Y = forward, Z = lateral (roll)
    M5.Imu.update();
    auto imuData = M5.Imu.getImuData();
    s_referenceAccelX = imuData.accel.x;
    LOG_INFO("Main", "IMU reference accelX=%.3f", s_referenceAccelX);

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

    // Initialize settings manager (loads presets + speed limit from NVS)
    g_settingsManager.begin();

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

    // Set PP mode parameters:
    //   PP_SPEED        - max velocity during profiled move
    //   PP_ACCELERATION  - accel/decel rate (higher = snappier)
    //   LIMIT_CUR       - motor current limit
    //   LIMIT_SPD       - general safety speed backstop
    float spdLimit = g_settingsManager.getMotorSpeedLimit();
    float accel    = g_settingsManager.getMotorAcceleration();
    float curLimit = g_settingsManager.getMotorCurrentLimit();
    g_motorManager.writeFloatParam(motorId, RobstrideParam::LIMIT_CUR, curLimit);
    delay(10);
    g_motorManager.writeFloatParam(motorId, RobstrideParam::PP_SPEED, spdLimit);
    delay(10);
    g_motorManager.writeFloatParam(motorId, RobstrideParam::PP_ACCELERATION, accel);
    delay(10);
    g_motorManager.writeFloatParam(motorId, RobstrideParam::LIMIT_SPD, spdLimit);
    delay(10);

    // Enable the motor
    g_motorManager.enableMotor(motorId);
    LOG_INFO("Trim", "Init motor %d (stop%s->PP->enable, spd=%.1f accel=%.1f cur=%.1f)",
             motorId, setZero ? "->zero" : "", spdLimit, accel, curLimit);
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

// Forward declaration
static void sendMotorPosition(uint8_t motorId, float position);

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
                    sendMotorPosition(leftId, g_trimTargetLeft);
                    LOG_INFO("Trim", "Left motor (ID %d) target: %.3f rad", leftId, g_trimTargetLeft);
                }
            }

            // D-pad Down: nudge left motor negative
            if (dpad & 0x02) {
                if (leftId > 0 && ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
                    g_trimTargetLeft -= TRIM_STEP_RAD;
                    sendMotorPosition(leftId, g_trimTargetLeft);
                    LOG_INFO("Trim", "Left motor (ID %d) target: %.3f rad", leftId, g_trimTargetLeft);
                }
            }

            // D-pad Right: nudge right motor positive
            if (dpad & 0x04) {
                if (rightId > 0 && ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
                    g_trimTargetRight += TRIM_STEP_RAD;
                    sendMotorPosition(rightId, -g_trimTargetRight);
                    LOG_INFO("Trim", "Right motor (ID %d) target: %.3f rad", rightId, g_trimTargetRight);
                }
            }

            // D-pad Left: nudge right motor negative
            if (dpad & 0x08) {
                if (rightId > 0 && ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
                    g_trimTargetRight -= TRIM_STEP_RAD;
                    sendMotorPosition(rightId, -g_trimTargetRight);
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
            sendMotorPosition(leftId, 0.0f);
        }
        if (rightId > 0) {
            sendMotorPosition(rightId, 0.0f);
        }

        s_lastSysMs = now;
        LOG_INFO("Trim", "All position state reset to zero.");
    }
}

// ---------------------------------------------------------------------------
// IMU update -- 100Hz polling, pitch angle, upside-down detection
// ---------------------------------------------------------------------------
static void updateIMU() {
    unsigned long now = millis();
    if (now - s_lastImuMs < IMU_UPDATE_MS) {
        return;
    }
    s_lastImuMs = now;

    M5.Imu.update();
    auto data = M5.Imu.getImuData();

    // Axis mapping (from empirical data):
    //   X = vertical (gravity): +1g level, -1g upside-down
    //   Y = forward:            -1g nose-up, +1g nose-down
    //   Z = lateral (roll):     +1g left-side-down

    // Pitch angle: 0 = level, negative = nose tilting down, +90 = nose up, -90 = nose down
    s_pitchAngle = atan2f(-data.accel.y, data.accel.x);

    // Store gyro pitch rate for PID D-term
    // Pitch rotation is around the Z axis (lateral axis)
    // Convert from degrees/s to radians/s
    s_gyroPitchRate = data.gyro.z * (PI / 180.0f);

    // Upside-down detection with hysteresis using accel.x (gravity axis)
    // If accel.x has opposite sign from reference AND exceeds threshold -> upside down
    // If same sign AND exceeds threshold -> right side up
    // Otherwise hold previous value
    if ((data.accel.x * s_referenceAccelX) < 0.0f && fabsf(data.accel.x) > IMU_FLIP_THRESHOLD) {
        g_isUpsideDown = true;
    } else if ((data.accel.x * s_referenceAccelX) > 0.0f && fabsf(data.accel.x) > IMU_FLIP_THRESHOLD) {
        g_isUpsideDown = false;
    }
    // else: hold previous value (hysteresis in the dead zone)

    // Periodic IMU telemetry log (every 500ms)
    static unsigned long s_lastImuLogMs = 0;
    if (now - s_lastImuLogMs >= 500) {
        s_lastImuLogMs = now;
        float pitchDeg = s_pitchAngle * (180.0f / PI);
        LOG_INFO("IMU", "pitch=%.1f deg  accel=(%.2f,%.2f,%.2f)  gyro=(%.1f,%.1f,%.1f)  flip=%s",
                 pitchDeg,
                 data.accel.x, data.accel.y, data.accel.z,
                 data.gyro.x, data.gyro.y, data.gyro.z,
                 g_isUpsideDown ? "YES" : "no");
    }
}

// Forward declaration (defined below processStickControl)
static void commandArms(float leftTarget, float rightTarget);

// ---------------------------------------------------------------------------
// Helper: send position command to a single motor, always including speed.
// In POSITION_PP mode the motor's trajectory planner uses PP_SPEED (0x7024)
// to cap the velocity during profiled moves. LIMIT_SPD (0x7017) is a general
// safety limit and does NOT control the PP profiler speed.
// We write PP_SPEED before every position command so the user's speed setting
// is always respected, even after a motor fault/reconnect.
// ---------------------------------------------------------------------------
static void sendMotorPosition(uint8_t motorId, float position) {
    float spd = g_settingsManager.getMotorSpeedLimit();
    g_motorManager.writeFloatParam(motorId, RobstrideParam::PP_SPEED, spd);
    g_motorManager.writeFloatParam(motorId, RobstrideParam::LOC_REF, position);
}

// ---------------------------------------------------------------------------
// Execute a button action based on its configured mode
// ---------------------------------------------------------------------------
static void executeButtonAction(uint8_t mode, float posLeft, float posRight, const char* btnName) {
    switch (mode) {
        case BTN_MODE_POSITION:
            // Position mode is handled as a HOLD -- see button hold logic below.
            // This case only fires on edge for non-hold modes.
            break;

        case BTN_MODE_FORWARD_360:
            // Full forward rotation from current position
            s_basePosition += TWO_PI;
            LOG_INFO("Stick", "%s: forward 360 (base=%.2f)", btnName, s_basePosition);
            break;

        case BTN_MODE_BACKWARD_360:
            // Full backward rotation from current position
            s_basePosition -= TWO_PI;
            LOG_INFO("Stick", "%s: backward 360 (base=%.2f)", btnName, s_basePosition);
            break;

        case BTN_MODE_GROUND_SLAP:
            // Start ground slap sequence
            s_basePosition = 0.0f;
            s_zeroOffset = 0.0f;
            g_trimTargetLeft = 0.0f;
            g_trimTargetRight = 0.0f;
            s_homePresetIndex = 0;  // reset to Front
            s_groundSlapPhase = 0;
            s_groundSlapMs = millis();
            s_groundSlapState = GS_RUNNING;
            // Command first phase immediately: arms to -0.10
            commandArms(-GROUND_SLAP_AMP, -GROUND_SLAP_AMP);
            LOG_INFO("Stick", "%s: ground slap started", btnName);
            break;
    }
}

// ---------------------------------------------------------------------------
// Process ground slap animation (called each frame from processStickControl)
// ---------------------------------------------------------------------------
static void processGroundSlap() {
    if (s_groundSlapState != GS_RUNNING) {
        return;
    }

    unsigned long now = millis();
    if (now - s_groundSlapMs < GROUND_SLAP_HALF_MS) {
        return;  // still waiting for current phase to complete
    }

    s_groundSlapPhase++;
    s_groundSlapMs = now;

    if (s_groundSlapPhase >= GROUND_SLAP_TOTAL_PHASES) {
        // All cycles done -- return to zero
        commandArms(0.0f, 0.0f);
        s_groundSlapState = GS_IDLE;
        LOG_INFO("Stick", "Ground slap complete");
        return;
    }

    // Odd phases: +amp, Even phases: -amp
    float target;
    if (s_groundSlapPhase % 2 == 0) {
        target = -GROUND_SLAP_AMP;
    } else {
        target = GROUND_SLAP_AMP;
    }
    commandArms(target, target);
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
        g_trimTargetLeft = 0.0f;
        g_trimTargetRight = 0.0f;
        LOG_INFO("Stick", "L1: home -> %s", HOME_PRESETS[s_homePresetIndex].name);
    }

    // --- Y/B/A buttons: edge-triggered for non-Position modes ---
    if (buttonsPressed & 0x0008) {
        if (g_settingsManager.getYMode() != BTN_MODE_POSITION) {
            executeButtonAction(g_settingsManager.getYMode(),
                                0, 0, "Y");
        }
    }
    if (buttonsPressed & 0x0002) {
        if (g_settingsManager.getBMode() != BTN_MODE_POSITION) {
            executeButtonAction(g_settingsManager.getBMode(),
                                0, 0, "B");
        }
    }
    if (buttonsPressed & 0x0001) {
        if (g_settingsManager.getAMode() != BTN_MODE_POSITION) {
            executeButtonAction(g_settingsManager.getAMode(),
                                0, 0, "A");
        }
    }

    // --- Y/B/A Position mode: hold to override arm targets ---
    // Check current button state (not edge) so position is held while pressed.
    // Last checked button wins if multiple are held simultaneously.
    bool btnPosHeld = false;
    float btnPosLeft = 0.0f;
    float btnPosRight = 0.0f;

    if ((buttons & 0x0008) && g_settingsManager.getYMode() == BTN_MODE_POSITION) {
        btnPosHeld = true;
        btnPosLeft = g_settingsManager.getYLeft();
        btnPosRight = g_settingsManager.getYRight();
    }
    if ((buttons & 0x0002) && g_settingsManager.getBMode() == BTN_MODE_POSITION) {
        btnPosHeld = true;
        btnPosLeft = g_settingsManager.getBLeft();
        btnPosRight = g_settingsManager.getBRight();
    }
    if ((buttons & 0x0001) && g_settingsManager.getAMode() == BTN_MODE_POSITION) {
        btnPosHeld = true;
        btnPosLeft = g_settingsManager.getALeft();
        btnPosRight = g_settingsManager.getARight();
    }

    // --- Process ground slap animation if active ---
    processGroundSlap();

    // While ground slap is running, skip normal stick/trigger position control
    // so the slap animation isn't overwritten each frame
    if (s_groundSlapState == GS_RUNNING) {
        return;
    }

    // If a position-mode button is held, command arms directly and skip stick control
    if (btnPosHeld) {
        if (leftId > 0) {
            if (ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
                sendMotorPosition(leftId, btnPosLeft);
            }
        }
        if (rightId > 0) {
            if (ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
                sendMotorPosition(rightId, -btnPosRight);
            }
        }
        return;
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

    // Ensure motors are ready and send position + speed commands
    if (leftId > 0) {
        if (ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
            sendMotorPosition(leftId, leftTarget);
        }
    }
    if (rightId > 0) {
        if (ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
            sendMotorPosition(rightId, -rightTarget);
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: command both arm motors to target positions (target space)
// ---------------------------------------------------------------------------
static void commandArms(float leftTarget, float rightTarget) {
    uint8_t leftId = resolveLeftMotorId();
    uint8_t rightId = resolveRightMotorId();
    if (leftId > 0) {
        if (ensureMotorReady(leftId, s_trimInitLeftId, s_autoZeroedLeftId)) {
            sendMotorPosition(leftId, leftTarget);
        }
    }
    if (rightId > 0) {
        if (ensureMotorReady(rightId, s_trimInitRightId, s_autoZeroedRightId)) {
            // Right motor is negated
            sendMotorPosition(rightId, -rightTarget);
        }
    }
}

// ---------------------------------------------------------------------------
// Self-righting state machine (Select button)
// ---------------------------------------------------------------------------
static void processSelfRight() {
    const ControllerState& state = g_controllerManager.getState(0);
    if (!state.connected) {
        // If controller lost during self-right, abort
        if (s_selfRightState != SR_IDLE) {
            s_selfRightState = SR_IDLE;
            LOG_INFO("SelfRight", "Aborted -- controller lost");
        }
        return;
    }

    unsigned long now = millis();

    // Edge detection for Select button (miscButtons bit 1)
    bool selectNow = (state.miscButtons & 0x02) != 0;
    bool selectPressed = selectNow && !s_prevSelectBtn;
    s_prevSelectBtn = selectNow;

    switch (s_selfRightState) {
        case SR_IDLE:
            if (selectPressed) {
                // Command arms to prep position ("Up" -- touches ground when inverted)
                commandArms(SELF_RIGHT_PREP_POS, SELF_RIGHT_PREP_POS);
                s_selfRightMs = now;
                s_selfRightState = SR_PREP;
                LOG_INFO("SelfRight", "Starting -- arms to prep (%.2f)", SELF_RIGHT_PREP_POS);
            }
            break;

        case SR_PREP:
            if (now - s_selfRightMs >= SELF_RIGHT_PREP_MS) {
                // Arms are in position, now push + drive forward
                commandArms(SELF_RIGHT_PUSH_POS, SELF_RIGHT_PUSH_POS);
                s_selfRightMs = now;
                s_selfRightState = SR_PUSH;
                LOG_INFO("SelfRight", "Pushing -- arms to %.2f (arms only)",
                         SELF_RIGHT_PUSH_POS);
            }
            break;

        case SR_PUSH:
            if (now - s_selfRightMs >= SELF_RIGHT_PUSH_MS) {
                s_selfRightState = SR_DONE;
                LOG_INFO("SelfRight", "Push complete, resetting");
            }
            break;

        case SR_DONE:
            // Reset arm state and return to Front
            s_homePresetIndex = 0;
            s_basePosition = 0.0f;
            s_zeroOffset = 0.0f;
            g_trimTargetLeft = 0.0f;
            g_trimTargetRight = 0.0f;
            commandArms(0.0f, 0.0f);
            s_selfRightState = SR_IDLE;
            LOG_INFO("SelfRight", "Done -- arms to Front, back to idle");
            break;
    }
}

// ---------------------------------------------------------------------------
// Nose-down PID balance state machine (X button)
// ---------------------------------------------------------------------------
static void processNoseDown() {
    const ControllerState& state = g_controllerManager.getState(0);
    if (!state.connected) {
        // If controller lost during nose-down, abort
        if (s_noseDownState != ND_IDLE) {
            s_noseDownState = ND_IDLE;
            s_pidIntegral = 0.0f;
            s_pidPrevError = 0.0f;
            LOG_INFO("NoseDown", "Aborted -- controller lost");
        }
        return;
    }

    unsigned long now = millis();

    // Edge detection for X button (buttons bit 2 = 0x0004)
    bool xNow = (state.buttons & 0x0004) != 0;
    bool xPressed = xNow && !s_prevXBtn;
    s_prevXBtn = xNow;

    switch (s_noseDownState) {
        case ND_IDLE: {
            if (xPressed) {
                if (g_isUpsideDown) {
                    // Need to self-right first before tipping
                    commandArms(SELF_RIGHT_PREP_POS, SELF_RIGHT_PREP_POS);
                    s_ndSrSub = NDSR_PREP;
                    s_ndSrMs = now;
                    s_noseDownState = ND_SELF_RIGHTING;
                    LOG_INFO("NoseDown", "Starting from upside-down -- self-righting first");
                } else {
                    // Already level, go straight to tipping (arms only, no drive)
                    commandArms(ND_TIP_LEFT, ND_TIP_RIGHT);
                    s_noseDownMs = now;
                    s_pitchConfirmCount = 0;
                    s_noseDownState = ND_TIPPING;
                    LOG_INFO("NoseDown", "Starting -- tipping forward (arms only)");
                }
            }
            break;
        }

        case ND_SELF_RIGHTING: {
            // Reuse self-right arm sweep logic as a sub-state
            switch (s_ndSrSub) {
                case NDSR_PREP:
                    if (now - s_ndSrMs >= SELF_RIGHT_PREP_MS) {
                        commandArms(SELF_RIGHT_PUSH_POS, SELF_RIGHT_PUSH_POS);
                        s_ndSrMs = now;
                        s_ndSrSub = NDSR_PUSH;
                        LOG_INFO("NoseDown", "Self-right: pushing");
                    }
                    break;
                case NDSR_PUSH:
                    if (now - s_ndSrMs >= SELF_RIGHT_PUSH_MS) {
                        s_ndSrSub = NDSR_DONE;
                        LOG_INFO("NoseDown", "Self-right: push done");
                    }
                    break;
                case NDSR_DONE:
                    // Now level -- transition to tipping (arms only, no drive)
                    commandArms(ND_TIP_LEFT, ND_TIP_RIGHT);
                    s_noseDownMs = now;
                    s_pitchConfirmCount = 0;
                    s_noseDownState = ND_TIPPING;
                    LOG_INFO("NoseDown", "Self-right complete -- now tipping forward (arms only)");
                    break;
            }
            break;
        }

        case ND_TIPPING: {
            unsigned long tipElapsed = now - s_noseDownMs;
            // Use negative pitch (nose-down only), not fabsf which would accept upside-down
            float pitchDeg = -s_pitchAngle * (180.0f / PI);  // positive when nose is down

            // Periodic logging during tipping (every 500ms)
            static unsigned long s_lastTipLogMs = 0;
            if (now - s_lastTipLogMs >= 500) {
                s_lastTipLogMs = now;
                LOG_INFO("NoseDown", "Tipping: pitch=%.1f deg, confirm=%d/%d, elapsed=%lu ms",
                         pitchDeg, s_pitchConfirmCount, ND_PITCH_CONFIRM_COUNT, tipElapsed);
            }

            // Ignore pitch readings during settle period (arm jolt creates accel spikes)
            if (tipElapsed < ND_TIP_SETTLE_MS) {
                break;
            }

            // Require sustained nose-down pitch above threshold (not just a single spike)
            if (pitchDeg > ND_PITCH_ENGAGED_DEG) {
                s_pitchConfirmCount++;
                if (s_pitchConfirmCount >= ND_PITCH_CONFIRM_COUNT) {
                    // Confirmed! Start PID balance (arms only, no drive)
                    s_balanceStartMs = now;
                    s_lastRampMs = now;
                    s_rampProgress = 0.0f;
                    s_pidIntegral = 0.0f;
                    s_pidPrevError = 0.0f;
                    s_noseDownState = ND_BALANCING;
                    LOG_INFO("NoseDown", "Pitch %.1f deg confirmed -- PID engaged, ramping arms slowly", pitchDeg);
                }
            } else {
                // Reset counter if pitch drops below threshold
                s_pitchConfirmCount = 0;
            }

            if (tipElapsed >= ND_TIP_TIMEOUT_MS) {
                // Timeout -- couldn't reach balance, abort
                commandArms(0.0f, 0.0f);
                s_homePresetIndex = 0;
                s_basePosition = 0.0f;
                s_zeroOffset = 0.0f;
                s_noseDownState = ND_IDLE;
                LOG_INFO("NoseDown", "Timeout -- aborting, pitch was %.1f deg", pitchDeg);
            }
            break;
        }

        case ND_BALANCING: {
            // Check if we fell out of balance -- re-enter tipping
            // Use negative pitch: nose-down is negative, so -pitchAngle is positive when nose-down
            {
                float noseDownDeg = -s_pitchAngle * (180.0f / PI);  // positive when nose is down
                if (noseDownDeg < ND_PITCH_LOST_DEG) {
                    LOG_INFO("NoseDown", "Lost balance (noseDown=%.1f deg) -- re-entering tipping", noseDownDeg);
                    commandArms(ND_TIP_LEFT, ND_TIP_RIGHT);
                    s_noseDownMs = now;
                    s_pitchConfirmCount = 0;
                    s_rampProgress = 0.0f;
                    s_pidIntegral = 0.0f;
                    s_pidPrevError = 0.0f;
                    s_noseDownState = ND_TIPPING;
                    break;
                }
            }

            // Check for X press to exit
            if (xPressed) {
                // Capture current arm positions for smooth exit ramp
                s_exitStartLeft  = ND_TIP_LEFT  + (ND_BALANCE_LEFT  - ND_TIP_LEFT)  * s_rampProgress;
                s_exitStartRight = ND_TIP_RIGHT + (ND_BALANCE_RIGHT - ND_TIP_RIGHT) * s_rampProgress;

                s_noseDownMs = now;
                s_noseDownState = ND_EXITING;
                s_pidIntegral = 0.0f;
                s_pidPrevError = 0.0f;
                LOG_INFO("NoseDown", "Exiting -- sweeping arms to Front (ramp was %.0f%%)", s_rampProgress * 100.0f);
                break;
            }

            // Pitch-gated ramp: only advance when pitch error is small
            {
                float errorDeg = fabsf(ND_PITCH_SETPOINT - s_pitchAngle) * (180.0f / PI);
                float rampDt = (float)(now - s_lastRampMs) / (float)ND_ARM_RAMP_MS;
                s_lastRampMs = now;
                if (errorDeg < ND_RAMP_ERROR_GATE_DEG) {
                    // Pitch is close to setpoint -- advance ramp
                    s_rampProgress += rampDt;
                    if (s_rampProgress > 1.0f) { s_rampProgress = 1.0f; }
                }
                // else: pitch error too large, freeze ramp and let PID stabilize
            }
            float nominalLeft  = ND_TIP_LEFT  + (ND_BALANCE_LEFT  - ND_TIP_LEFT)  * s_rampProgress;
            float nominalRight = ND_TIP_RIGHT + (ND_BALANCE_RIGHT - ND_TIP_RIGHT) * s_rampProgress;

            // Compute world-frame angles from vertical for gain scheduling
            float phiL = -(nominalLeft + PI);
            float phiR = -(nominalRight + PI);
            float sensL = -cosf(phiL);
            float sensR = -cosf(phiR);
            float totalSens = sensL + sensR;

            // PID error
            float error = ND_PITCH_SETPOINT - s_pitchAngle;
            float dt = IMU_UPDATE_MS / 1000.0f;

            // Integral with anti-windup
            s_pidIntegral += error * dt;
            if (s_pidIntegral > ND_PID_INTEGRAL_LIMIT) {
                s_pidIntegral = ND_PID_INTEGRAL_LIMIT;
            }
            if (s_pidIntegral < -ND_PID_INTEGRAL_LIMIT) {
                s_pidIntegral = -ND_PID_INTEGRAL_LIMIT;
            }

            // Derivative from gyro pitch rate (cleaner than differentiating accel)
            float derivative = -s_gyroPitchRate;

            // PID output
            float pidOut = ND_PID_KP * error + ND_PID_KI * s_pidIntegral + ND_PID_KD * derivative;
            if (pidOut > ND_PID_OUTPUT_LIMIT) { pidOut = ND_PID_OUTPUT_LIMIT; }
            if (pidOut < -ND_PID_OUTPUT_LIMIT) { pidOut = -ND_PID_OUTPUT_LIMIT; }

            s_pidPrevError = error;

            // Gain-scheduled offset distribution
            float leftTarget = nominalLeft;
            float rightTarget = nominalRight;

            if (fabsf(totalSens) > ND_MIN_SENSITIVITY) {
                // Uniform mode: both arms move same direction, scaled by total sensitivity
                float offset = pidOut / totalSens;
                // Hard clamp to prevent arm position explosions near low-sensitivity zones
                if (offset > ND_MAX_ARM_OFFSET) { offset = ND_MAX_ARM_OFFSET; }
                if (offset < -ND_MAX_ARM_OFFSET) { offset = -ND_MAX_ARM_OFFSET; }
                leftTarget  = nominalLeft  + offset;
                rightTarget = nominalRight + offset;
            } else {
                // Differential mode: arms on opposite sides of zero-crossing
                float diffSens = sensL - sensR;
                if (fabsf(diffSens) > ND_MIN_SENSITIVITY) {
                    float diffOffset = pidOut / diffSens;
                    // Hard clamp to prevent arm position explosions
                    if (diffOffset > ND_MAX_ARM_OFFSET) { diffOffset = ND_MAX_ARM_OFFSET; }
                    if (diffOffset < -ND_MAX_ARM_OFFSET) { diffOffset = -ND_MAX_ARM_OFFSET; }
                    leftTarget  = nominalLeft  + diffOffset;
                    rightTarget = nominalRight - diffOffset;
                }
                // else: both sensitivities near zero, just follow ramp
            }

            commandArms(leftTarget, rightTarget);

            // Periodic PID telemetry (every 250ms)
            static unsigned long s_lastBalLogMs = 0;
            if (now - s_lastBalLogMs >= 250) {
                s_lastBalLogMs = now;
                float pitchDeg = s_pitchAngle * (180.0f / PI);
                LOG_INFO("NoseDown", "BAL: pitch=%.1f err=%.2f pid=%.2f ramp=%.0f%% nomL=%.2f nomR=%.2f armL=%.2f armR=%.2f sens=%.2f",
                         pitchDeg, error, pidOut, s_rampProgress * 100.0f,
                         nominalLeft, nominalRight, leftTarget, rightTarget, totalSens);
            }
            break;
        }

        case ND_EXITING: {
            unsigned long elapsed = now - s_noseDownMs;

            // Arm sweep over ND_EXIT_MS (no drive, arms only)
            float progress = (float)elapsed / (float)ND_EXIT_MS;
            if (progress > 1.0f) { progress = 1.0f; }

            // Sweep arms from exit start position through "Up" to "Front"
            // First half: move to "Up" (-1.79, -1.79)
            // Second half: move from "Up" to "Front" (0.0, 0.0)
            float leftTarget, rightTarget;
            if (progress < 0.5f) {
                float subT = progress * 2.0f; // 0..1 for first half
                leftTarget  = s_exitStartLeft  + (-1.79f - s_exitStartLeft)  * subT;
                rightTarget = s_exitStartRight + (-1.79f - s_exitStartRight) * subT;
            } else {
                float subT = (progress - 0.5f) * 2.0f; // 0..1 for second half
                leftTarget  = -1.79f + (0.0f - (-1.79f)) * subT;
                rightTarget = -1.79f + (0.0f - (-1.79f)) * subT;
            }
            commandArms(leftTarget, rightTarget);

            if (progress >= 1.0f) {
                // Exit complete
                s_homePresetIndex = 0;
                s_basePosition = 0.0f;
                s_zeroOffset = 0.0f;
                g_trimTargetLeft = 0.0f;
                g_trimTargetRight = 0.0f;
                commandArms(0.0f, 0.0f);
                s_noseDownState = ND_IDLE;
                LOG_INFO("NoseDown", "Exit complete -- back to idle");
            }
            break;
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

    // 1a. Update IMU: pitch angle, upside-down detection (100Hz internally rate-limited)
    updateIMU();

    // 1a2. Push inversion flag to drive task
    g_driveManager.setInverted(g_isUpsideDown);

    // 1b. Process motor trim (d-pad nudge + Sys zero)
    processTrim();

    // 1b2. Process self-righting (Select button)
    processSelfRight();

    // 1b3. Process nose-down balance (X button)
    processNoseDown();

    // 1c. Process left stick motor control (jog + difference)
    // Skipped when self-righting or nose-down mode is active to prevent interference
    if (s_selfRightState == SR_IDLE && s_noseDownState == ND_IDLE) {
        processStickControl();
    }

    // 1d. Push motor tuning params to running motors if changed via web UI.
    if (g_settingsManager.consumeMotorParamsDirty()) {
        float newSpd   = g_settingsManager.getMotorSpeedLimit();
        float newAccel = g_settingsManager.getMotorAcceleration();
        float newCur   = g_settingsManager.getMotorCurrentLimit();
        uint8_t leftId = resolveLeftMotorId();
        uint8_t rightId = resolveRightMotorId();
        if (leftId > 0) {
            g_motorManager.writeFloatParam(leftId, RobstrideParam::PP_SPEED, newSpd);
            g_motorManager.writeFloatParam(leftId, RobstrideParam::PP_ACCELERATION, newAccel);
            g_motorManager.writeFloatParam(leftId, RobstrideParam::LIMIT_CUR, newCur);
            g_motorManager.writeFloatParam(leftId, RobstrideParam::LIMIT_SPD, newSpd);
        }
        if (rightId > 0) {
            g_motorManager.writeFloatParam(rightId, RobstrideParam::PP_SPEED, newSpd);
            g_motorManager.writeFloatParam(rightId, RobstrideParam::PP_ACCELERATION, newAccel);
            g_motorManager.writeFloatParam(rightId, RobstrideParam::LIMIT_CUR, newCur);
            g_motorManager.writeFloatParam(rightId, RobstrideParam::LIMIT_SPD, newSpd);
        }
        LOG_INFO("Main", "Motor params pushed: spd=%.1f accel=%.1f cur=%.1f", newSpd, newAccel, newCur);
    }

    // Update web-accessible state copies
    g_pitchAngleForWeb = s_pitchAngle;
    g_selfRightStateForWeb = (int)s_selfRightState;
    g_noseDownStateForWeb = (int)s_noseDownState;

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

    // ---------------------------------------------------------------------------
    // Periodic motor speed parameter readback (debug)
    // Cycles through reading PP_SPEED and LIMIT_SPD from each motor every 5s.
    // Results are stored in RobstrideMotorStatus and logged.
    // ---------------------------------------------------------------------------
    {
        static unsigned long s_lastSpeedReadMs = 0;
        static int s_paramReadStep = 0;
        unsigned long now2 = millis();
        if (now2 - s_lastSpeedReadMs >= 5000 && !g_motorManager.isParamReadPending()) {
            uint8_t leftId = resolveLeftMotorId();
            uint8_t rightId = resolveRightMotorId();
            bool sent = false;
            // Params to read per motor: PP_SPEED, PP_ACCEL, LIMIT_SPD, LIMIT_CUR, RUN_MODE
            static const uint16_t paramsPerMotor[] = {
                RobstrideParam::PP_SPEED,
                RobstrideParam::PP_ACCELERATION,
                RobstrideParam::LIMIT_SPD,
                RobstrideParam::LIMIT_CUR,
                RobstrideParam::RUN_MODE
            };
            static const int PARAMS_PER_MOTOR = 5;
            static const int STEP_COUNT = PARAMS_PER_MOTOR * 2;  // left + right
            for (int attempt = 0; attempt < STEP_COUNT && !sent; attempt++) {
                int step = (s_paramReadStep + attempt) % STEP_COUNT;
                uint8_t mId = (step < PARAMS_PER_MOTOR) ? leftId : rightId;
                uint16_t param = paramsPerMotor[step % PARAMS_PER_MOTOR];
                if (mId > 0) {
                    sent = g_motorManager.requestParamRead(mId, param);
                    if (sent) {
                        s_paramReadStep = (step + 1) % STEP_COUNT;
                        s_lastSpeedReadMs = now2;
                    }
                } else {
                    s_paramReadStep = (step + 1) % STEP_COUNT;
                }
            }
        }
    }

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
            // Log motor parameter readback values (debug)
            for (int mi = 0; mi < g_motorManager.getMotorCount(); mi++) {
                const RobstrideMotorStatus& ms = g_motorManager.getMotorStatus(mi);
                uint8_t mId = g_motorManager.getMotorId(mi);
                const char* role = g_motorManager.getRoleLabel(mId);
                LOG_INFO("Main", "  Motor %d [%s]: ppSpd=%.1f ppAcc=%.1f limSpd=%.1f limCur=%.1f vel=%.1f",
                         mId, role, ms.ppSpeed, ms.ppAccel, ms.limitSpd, ms.limitCur, ms.velocity);
            }
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

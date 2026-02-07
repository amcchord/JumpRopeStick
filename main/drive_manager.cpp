// =============================================================================
// Drive Manager Module - Implementation
// =============================================================================
// Standard RC servo PPM via ESP32 LEDC peripheral. Runs as a FreeRTOS task
// on CPU0 for deterministic 50Hz timing, isolated from display/WiFi on CPU1.
//
// Arcade-style differential drive with expo curves.
// Bidirectional: 1500us = stop, 1000us = full reverse, 2000us = full forward.
// =============================================================================

#include "drive_manager.h"
#include "config.h"
#include "debug_log.h"
#include "controller_manager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include <controller/uni_gamepad.h>  // For BUTTON_SHOULDER_L/R constants

static const char* TAG = "Drive";

// External reference to controller manager
extern ControllerManager g_controllerManager;

// Axis range from Bluepad32
static const float AXIS_MAX = 512.0f;

// LEDC resolution and duty cycle calculations
// At 16-bit resolution with 50Hz, one full period = 20000us
// duty = (pulseUs / 20000) * 65535
static const uint32_t LEDC_FULL_DUTY = (1 << LEDC_SERVO_RESOLUTION) - 1;  // 65535
static const uint32_t SERVO_PERIOD_US = 1000000 / SERVO_FREQ_HZ;          // 20000us

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void DriveManager::begin() {
    LOG_INFO(TAG, "Initializing servo PPM drive...");

    if (!initLedc()) {
        LOG_ERROR(TAG, "LEDC initialization failed, drive disabled");
        return;
    }

    // Start with servos at center (stopped)
    writeServo(LEDC_SERVO_LEFT_CH, SERVO_CENTER_US);
    writeServo(LEDC_SERVO_RIGHT_CH, SERVO_CENTER_US);

    _initialized = true;

    // Spawn the drive control task on CPU0
    BaseType_t result = xTaskCreatePinnedToCore(
        driveTaskFunc,          // Task function
        "drive",                // Name
        DRIVE_TASK_STACK,       // Stack size
        this,                   // Parameter (DriveManager instance)
        DRIVE_TASK_PRIORITY,    // Priority
        NULL,                   // Task handle (not needed)
        DRIVE_TASK_CORE         // Core ID
    );

    if (result != pdPASS) {
        LOG_ERROR(TAG, "Failed to create drive task");
        _initialized = false;
        return;
    }

    LOG_INFO(TAG, "Drive task started on CPU%d (PPM %dHz, L=G%d ch%d, R=G%d ch%d, mix@%dHz)",
             DRIVE_TASK_CORE, SERVO_FREQ_HZ,
             PIN_SERVO_LEFT, LEDC_SERVO_LEFT_CH,
             PIN_SERVO_RIGHT, LEDC_SERVO_RIGHT_CH,
             1000 / DRIVE_UPDATE_MS);
}

uint16_t DriveManager::getLeftPulse() const {
    return _leftPulseUs;
}

uint16_t DriveManager::getRightPulse() const {
    return _rightPulseUs;
}

float DriveManager::getLeftDrive() const {
    return _leftDrive;
}

float DriveManager::getRightDrive() const {
    return _rightDrive;
}

// ---------------------------------------------------------------------------
// LEDC initialization
// ---------------------------------------------------------------------------

bool DriveManager::initLedc() {
    // Configure LEDC timer for 50Hz servo signal
    ledc_timer_config_t timerCfg = {};
    timerCfg.speed_mode = (ledc_mode_t)LEDC_SERVO_SPEED_MODE;
    timerCfg.timer_num = (ledc_timer_t)LEDC_SERVO_TIMER;
    timerCfg.duty_resolution = (ledc_timer_bit_t)LEDC_SERVO_RESOLUTION;
    timerCfg.freq_hz = SERVO_FREQ_HZ;
    timerCfg.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timerCfg);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return false;
    }

    // Configure left servo channel
    ledc_channel_config_t chCfg = {};
    chCfg.speed_mode = (ledc_mode_t)LEDC_SERVO_SPEED_MODE;
    chCfg.channel = (ledc_channel_t)LEDC_SERVO_LEFT_CH;
    chCfg.timer_sel = (ledc_timer_t)LEDC_SERVO_TIMER;
    chCfg.intr_type = LEDC_INTR_DISABLE;
    chCfg.gpio_num = PIN_SERVO_LEFT;
    chCfg.duty = 0;
    chCfg.hpoint = 0;

    err = ledc_channel_config(&chCfg);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC left channel config failed: %s", esp_err_to_name(err));
        return false;
    }

    // Configure right servo channel
    chCfg.channel = (ledc_channel_t)LEDC_SERVO_RIGHT_CH;
    chCfg.gpio_num = PIN_SERVO_RIGHT;

    err = ledc_channel_config(&chCfg);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC right channel config failed: %s", esp_err_to_name(err));
        return false;
    }

    LOG_INFO(TAG, "LEDC servo ready: 2 channels, %dHz, %d-bit resolution",
             SERVO_FREQ_HZ, LEDC_SERVO_RESOLUTION);
    return true;
}

// ---------------------------------------------------------------------------
// Servo output
// ---------------------------------------------------------------------------

void DriveManager::writeServo(uint8_t channel, uint16_t pulseUs) {
    // Convert pulse width in microseconds to LEDC duty cycle
    // duty = (pulseUs / periodUs) * maxDuty
    uint32_t duty = (uint32_t)pulseUs * LEDC_FULL_DUTY / SERVO_PERIOD_US;
    ledc_set_duty((ledc_mode_t)LEDC_SERVO_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty((ledc_mode_t)LEDC_SERVO_SPEED_MODE, (ledc_channel_t)channel);
}

uint16_t DriveManager::driveToMicroseconds(float drive) {
    // Map -1.0..1.0 to SERVO_MIN_US..SERVO_MAX_US (center at 0.0 = SERVO_CENTER_US)
    if (drive > 1.0f) { drive = 1.0f; }
    if (drive < -1.0f) { drive = -1.0f; }

    // Linear map: center + drive * half_range
    float halfRange = (float)(SERVO_MAX_US - SERVO_MIN_US) / 2.0f;
    float result = (float)SERVO_CENTER_US + drive * halfRange;
    return (uint16_t)(result + 0.5f);
}

// ---------------------------------------------------------------------------
// Expo helper
// ---------------------------------------------------------------------------

float DriveManager::applyExpo(float input, float expo) {
    // Blend between linear and cubic: out = (1-expo)*in + expo*in^3
    // Preserves sign, gives finer control near center while keeping full range.
    float cubic = input * input * input;
    return (1.0f - expo) * input + expo * cubic;
}

// ---------------------------------------------------------------------------
// FreeRTOS drive task -- runs on CPU0
// ---------------------------------------------------------------------------

void DriveManager::driveTaskFunc(void* param) {
    DriveManager* self = static_cast<DriveManager*>(param);

    LOG_INFO(TAG, "Drive task running on core %d", xPortGetCoreID());

    TickType_t lastWake = xTaskGetTickCount();
    unsigned long lastLogMs = millis();

    for (;;) {
        // Sleep until next 20ms tick (50Hz)
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DRIVE_UPDATE_MS));

        unsigned long now = millis();

        // Find the first connected controller
        const ControllerState* activeCtrl = nullptr;
        for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
            const ControllerState& state = g_controllerManager.getState(i);
            if (state.connected) {
                activeCtrl = &state;
                break;
            }
        }

        uint16_t leftUs = SERVO_CENTER_US;
        uint16_t rightUs = SERVO_CENTER_US;
        float leftDrive = 0.0f;
        float rightDrive = 0.0f;

        if (activeCtrl != nullptr) {
            // Use right stick only for drive (left stick is reserved for motor control)
            float rawX = (float)activeCtrl->rx;
            float rawY = (float)activeCtrl->ry;

            // Normalize to [-1.0, 1.0]
            float normX = rawX / AXIS_MAX;
            float normY = rawY / AXIS_MAX;
            if (normX > 1.0f) { normX = 1.0f; }
            if (normX < -1.0f) { normX = -1.0f; }
            if (normY > 1.0f) { normY = 1.0f; }
            if (normY < -1.0f) { normY = -1.0f; }

            // Throttle: negate Y so stick-up = positive (forward)
            float throttle = -normY;
            float turn = normX;

            // Apply expo curve to both axes independently
            throttle = applyExpo(throttle, DRIVE_EXPO);
            turn     = applyExpo(turn, DRIVE_EXPO);

            // Arcade mix: differential drive
            // Turn is subtracted from left, added to right so that
            // stick-right makes the robot turn right.
            leftDrive  = throttle - turn;
            rightDrive = throttle + turn;
            if (leftDrive > 1.0f) { leftDrive = 1.0f; }
            if (leftDrive < -1.0f) { leftDrive = -1.0f; }
            if (rightDrive > 1.0f) { rightDrive = 1.0f; }
            if (rightDrive < -1.0f) { rightDrive = -1.0f; }

            // Slow mode: hold R3 (right stick click) to limit output
            bool slowMode = (activeCtrl->buttons & BUTTON_THUMB_R) != 0;
            if (slowMode) {
                leftDrive  *= DRIVE_SLOW_MODE_SCALE;
                rightDrive *= DRIVE_SLOW_MODE_SCALE;
            }

            // Convert to servo pulse widths
            leftUs  = driveToMicroseconds(leftDrive);
            rightUs = driveToMicroseconds(rightDrive);
        }

        // Write to servo hardware
        writeServo(LEDC_SERVO_LEFT_CH, leftUs);
        writeServo(LEDC_SERVO_RIGHT_CH, rightUs);

        // Update shared state for display/web (volatile writes)
        self->_leftDrive = leftDrive;
        self->_rightDrive = rightDrive;
        self->_leftPulseUs = leftUs;
        self->_rightPulseUs = rightUs;

        // Periodic log (every 500ms)
        if ((now - lastLogMs) >= 500) {
            lastLogMs = now;
            LOG_INFO(TAG, "PPM L=%uus R=%uus  drive L=%.2f R=%.2f",
                     leftUs, rightUs, leftDrive, rightDrive);
        }
    }
}

// =============================================================================
// Drive Manager Module - Implementation
// =============================================================================
// Arcade-style differential drive with expo curves.
// Uses ESP32 LEDC for servo PPM output at 50Hz.
// =============================================================================

#include "drive_manager.h"
#include "config.h"
#include "debug_log.h"
#include "controller_manager.h"

#include <driver/ledc.h>
#include <math.h>

static const char* TAG = "Drive";

// External reference to controller manager
extern ControllerManager g_controllerManager;

// Axis range from Bluepad32
static const float AXIS_MAX = 512.0f;

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void DriveManager::begin() {
    LOG_INFO(TAG, "Initializing servo drive...");

    // Configure LEDC timer (shared by both channels)
    ledc_timer_config_t timerCfg = {};
    timerCfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    timerCfg.timer_num       = LEDC_TIMER_0;
    timerCfg.duty_resolution = (ledc_timer_bit_t)LEDC_SERVO_RESOLUTION;
    timerCfg.freq_hz         = LEDC_SERVO_FREQ;
    timerCfg.clk_cfg         = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timerCfg);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return;
    }

    // Configure left servo channel
    ledc_channel_config_t leftCh = {};
    leftCh.speed_mode = LEDC_LOW_SPEED_MODE;
    leftCh.channel    = (ledc_channel_t)LEDC_SERVO_LEFT_CH;
    leftCh.timer_sel  = LEDC_TIMER_0;
    leftCh.intr_type  = LEDC_INTR_DISABLE;
    leftCh.gpio_num   = PIN_SERVO_LEFT;
    leftCh.duty       = 0;
    leftCh.hpoint     = 0;

    err = ledc_channel_config(&leftCh);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC left channel config failed: %s", esp_err_to_name(err));
        return;
    }

    // Configure right servo channel
    ledc_channel_config_t rightCh = {};
    rightCh.speed_mode = LEDC_LOW_SPEED_MODE;
    rightCh.channel    = (ledc_channel_t)LEDC_SERVO_RIGHT_CH;
    rightCh.timer_sel  = LEDC_TIMER_0;
    rightCh.intr_type  = LEDC_INTR_DISABLE;
    rightCh.gpio_num   = PIN_SERVO_RIGHT;
    rightCh.duty       = 0;
    rightCh.hpoint     = 0;

    err = ledc_channel_config(&rightCh);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "LEDC right channel config failed: %s", esp_err_to_name(err));
        return;
    }

    // Set both servos to center (stopped)
    writeServo(LEDC_SERVO_LEFT_CH, SERVO_CENTER_US);
    writeServo(LEDC_SERVO_RIGHT_CH, SERVO_CENTER_US);

    _leftPulseUs = SERVO_CENTER_US;
    _rightPulseUs = SERVO_CENTER_US;
    _initialized = true;

    LOG_INFO(TAG, "Servo drive ready (L=G%d, R=G%d, %dHz control loop)",
             PIN_SERVO_LEFT, PIN_SERVO_RIGHT, 1000 / DRIVE_UPDATE_MS);
}

void DriveManager::update() {
    if (!_initialized) {
        return;
    }

    // Rate-limit to 50Hz (20ms)
    unsigned long now = millis();
    if (now - _lastUpdateMs < DRIVE_UPDATE_MS) {
        return;
    }
    _lastUpdateMs = now;

    // Find the first connected controller
    const ControllerState* activeCtrl = nullptr;
    for (int i = 0; i < CONTROLLER_MAX_COUNT; i++) {
        const ControllerState& state = g_controllerManager.getState(i);
        if (state.connected) {
            activeCtrl = &state;
            break;
        }
    }

    // Failsafe: no controller -> stop
    if (activeCtrl == nullptr) {
        _leftDrive = 0.0f;
        _rightDrive = 0.0f;
        _leftPulseUs = SERVO_CENTER_US;
        _rightPulseUs = SERVO_CENTER_US;
        writeServo(LEDC_SERVO_LEFT_CH, SERVO_CENTER_US);
        writeServo(LEDC_SERVO_RIGHT_CH, SERVO_CENTER_US);
        return;
    }

    // -----------------------------------------------------------------------
    // Stick selection: use whichever stick has larger total deflection
    // -----------------------------------------------------------------------
    float lMag = (float)(activeCtrl->lx) * (float)(activeCtrl->lx)
               + (float)(activeCtrl->ly) * (float)(activeCtrl->ly);
    float rMag = (float)(activeCtrl->rx) * (float)(activeCtrl->rx)
               + (float)(activeCtrl->ry) * (float)(activeCtrl->ry);

    float rawX, rawY;
    if (lMag >= rMag) {
        rawX = (float)activeCtrl->lx;
        rawY = (float)activeCtrl->ly;
    } else {
        rawX = (float)activeCtrl->rx;
        rawY = (float)activeCtrl->ry;
    }

    // -----------------------------------------------------------------------
    // Normalize to [-1.0, 1.0]
    // -----------------------------------------------------------------------
    float normX = rawX / AXIS_MAX;  // Turn: positive = right
    float normY = rawY / AXIS_MAX;  // Raw Y: negative = stick up

    // Clamp in case axis slightly exceeds range
    if (normX > 1.0f) { normX = 1.0f; }
    if (normX < -1.0f) { normX = -1.0f; }
    if (normY > 1.0f) { normY = 1.0f; }
    if (normY < -1.0f) { normY = -1.0f; }

    // Throttle: negate Y so stick-up = positive (forward)
    float throttle = -normY;
    float turn = normX;

    // -----------------------------------------------------------------------
    // Apply expo curve to both axes independently
    // -----------------------------------------------------------------------
    throttle = applyExpo(throttle, DRIVE_EXPO);
    turn     = applyExpo(turn, DRIVE_EXPO);

    // -----------------------------------------------------------------------
    // Arcade mix: differential drive
    // -----------------------------------------------------------------------
    float left  = throttle + turn;
    float right = throttle - turn;

    // Clamp to [-1.0, 1.0]
    if (left > 1.0f) { left = 1.0f; }
    if (left < -1.0f) { left = -1.0f; }
    if (right > 1.0f) { right = 1.0f; }
    if (right < -1.0f) { right = -1.0f; }

    // -----------------------------------------------------------------------
    // Output to servos
    // -----------------------------------------------------------------------
    _leftDrive = left;
    _rightDrive = right;
    _leftPulseUs = driveToMicroseconds(left);
    _rightPulseUs = driveToMicroseconds(right);

    writeServo(LEDC_SERVO_LEFT_CH, _leftPulseUs);
    writeServo(LEDC_SERVO_RIGHT_CH, _rightPulseUs);
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
// Private helpers
// ---------------------------------------------------------------------------

float DriveManager::applyExpo(float input, float expo) const {
    // Blend between linear and cubic: out = (1-expo)*in + expo*in^3
    // Preserves sign, gives finer control near center while keeping full range.
    float cubic = input * input * input;
    return (1.0f - expo) * input + expo * cubic;
}

uint16_t DriveManager::driveToMicroseconds(float drive) const {
    // Map -1.0..1.0 to SERVO_MIN_US..SERVO_MAX_US
    // 0.0 -> SERVO_CENTER_US (1500)
    float us = SERVO_CENTER_US + drive * (float)(SERVO_MAX_US - SERVO_CENTER_US);
    if (us < (float)SERVO_MIN_US) { us = (float)SERVO_MIN_US; }
    if (us > (float)SERVO_MAX_US) { us = (float)SERVO_MAX_US; }
    return (uint16_t)(us + 0.5f);
}

void DriveManager::writeServo(uint8_t channel, uint16_t pulseUs) const {
    // Convert pulse width in microseconds to LEDC duty value.
    // At 50Hz, period = 20000us.  At 16-bit resolution, max duty = 65536.
    // duty = (pulseUs / 20000) * 65536
    uint32_t maxDuty = (1U << LEDC_SERVO_RESOLUTION);
    uint32_t duty = (uint32_t)((float)pulseUs / 20000.0f * (float)maxDuty);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

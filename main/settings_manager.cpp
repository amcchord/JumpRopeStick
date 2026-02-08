// =============================================================================
// Settings Manager - Implementation
// =============================================================================
// Persists user-configurable settings (button modes, presets, motor speed limit)
// to ESP32 NVS (Non-Volatile Storage) using the Preferences library.
// =============================================================================

#include "settings_manager.h"
#include "config.h"
#include "debug_log.h"

#include <Preferences.h>

static const char* TAG = "Settings";

void SettingsManager::begin() {
    loadSettings();
}

// ---- Mode Getters ----

uint8_t SettingsManager::getYMode() const { return _yMode; }
uint8_t SettingsManager::getBMode() const { return _bMode; }
uint8_t SettingsManager::getAMode() const { return _aMode; }

// ---- Position Getters ----

float SettingsManager::getYLeft() const { return _yLeft; }
float SettingsManager::getYRight() const { return _yRight; }

float SettingsManager::getBLeft() const { return _bLeft; }
float SettingsManager::getBRight() const { return _bRight; }

float SettingsManager::getALeft() const { return _aLeft; }
float SettingsManager::getARight() const { return _aRight; }

float SettingsManager::getMotorSpeedLimit() const { return _motorSpeedLimit; }

// ---- Config Setters (mode + position, save to NVS immediately) ----

static uint8_t clampMode(uint8_t mode) {
    if (mode >= BTN_MODE_COUNT) { return BTN_MODE_POSITION; }
    return mode;
}

void SettingsManager::setYConfig(uint8_t mode, float left, float right) {
    _yMode = clampMode(mode);
    _yLeft = left;
    _yRight = right;
    saveSettings();
    LOG_INFO(TAG, "Y config updated: mode=%d L=%.3f R=%.3f", _yMode, _yLeft, _yRight);
}

void SettingsManager::setBConfig(uint8_t mode, float left, float right) {
    _bMode = clampMode(mode);
    _bLeft = left;
    _bRight = right;
    saveSettings();
    LOG_INFO(TAG, "B config updated: mode=%d L=%.3f R=%.3f", _bMode, _bLeft, _bRight);
}

void SettingsManager::setAConfig(uint8_t mode, float left, float right) {
    _aMode = clampMode(mode);
    _aLeft = left;
    _aRight = right;
    saveSettings();
    LOG_INFO(TAG, "A config updated: mode=%d L=%.3f R=%.3f", _aMode, _aLeft, _aRight);
}

// Legacy setters (position only, mode unchanged)
void SettingsManager::setYPreset(float left, float right) {
    _yLeft = left;
    _yRight = right;
    saveSettings();
    LOG_INFO(TAG, "Y preset updated: L=%.3f R=%.3f", _yLeft, _yRight);
}

void SettingsManager::setBPreset(float left, float right) {
    _bLeft = left;
    _bRight = right;
    saveSettings();
    LOG_INFO(TAG, "B preset updated: L=%.3f R=%.3f", _bLeft, _bRight);
}

void SettingsManager::setAPreset(float left, float right) {
    _aLeft = left;
    _aRight = right;
    saveSettings();
    LOG_INFO(TAG, "A preset updated: L=%.3f R=%.3f", _aLeft, _aRight);
}

// ---- Motor Tuning ----

float SettingsManager::getMotorAcceleration() const { return _motorAcceleration; }
float SettingsManager::getMotorCurrentLimit() const { return _motorCurrentLimit; }

void SettingsManager::setMotorSpeedLimit(float limit) {
    if (limit < 0.1f) { limit = 0.1f; }
    if (limit > 50.0f) { limit = 50.0f; }
    _motorSpeedLimit = limit;
    _motorParamsDirty = true;
    saveSettings();
    LOG_INFO(TAG, "Motor speed limit updated: %.1f rad/s", _motorSpeedLimit);
}

void SettingsManager::setMotorAcceleration(float accel) {
    if (accel < 1.0f) { accel = 1.0f; }
    if (accel > 500.0f) { accel = 500.0f; }
    _motorAcceleration = accel;
    _motorParamsDirty = true;
    saveSettings();
    LOG_INFO(TAG, "Motor acceleration updated: %.1f rad/s^2", _motorAcceleration);
}

void SettingsManager::setMotorCurrentLimit(float limit) {
    if (limit < 0.5f) { limit = 0.5f; }
    if (limit > 40.0f) { limit = 40.0f; }
    _motorCurrentLimit = limit;
    _motorParamsDirty = true;
    saveSettings();
    LOG_INFO(TAG, "Motor current limit updated: %.1f A", _motorCurrentLimit);
}

bool SettingsManager::consumeMotorParamsDirty() {
    if (_motorParamsDirty) {
        _motorParamsDirty = false;
        return true;
    }
    return false;
}

// ---- NVS Persistence ----

void SettingsManager::loadSettings() {
    Preferences prefs;
    prefs.begin("settings", true);  // read-only

    _yMode  = prefs.getUChar("yM", BTN_MODE_POSITION);
    _bMode  = prefs.getUChar("bM", BTN_MODE_POSITION);
    _aMode  = prefs.getUChar("aM", BTN_MODE_POSITION);

    _yLeft  = prefs.getFloat("yL", DEFAULT_Y_PRESET_LEFT);
    _yRight = prefs.getFloat("yR", DEFAULT_Y_PRESET_RIGHT);
    _bLeft  = prefs.getFloat("bL", DEFAULT_B_PRESET_LEFT);
    _bRight = prefs.getFloat("bR", DEFAULT_B_PRESET_RIGHT);
    _aLeft  = prefs.getFloat("aL", DEFAULT_A_PRESET_LEFT);
    _aRight = prefs.getFloat("aR", DEFAULT_A_PRESET_RIGHT);
    _motorSpeedLimit   = prefs.getFloat("spdLim", MOTOR_SPEED_LIMIT);
    _motorAcceleration = prefs.getFloat("ppAccel", MOTOR_PP_ACCELERATION);
    _motorCurrentLimit = prefs.getFloat("curLim", MOTOR_CURRENT_LIMIT);

    // Clamp modes in case NVS has stale data
    _yMode = clampMode(_yMode);
    _bMode = clampMode(_bMode);
    _aMode = clampMode(_aMode);

    prefs.end();

    LOG_INFO(TAG, "Loaded: Y(m=%d,%.2f,%.2f) B(m=%d,%.2f,%.2f) A(m=%d,%.2f,%.2f)",
             _yMode, _yLeft, _yRight, _bMode, _bLeft, _bRight,
             _aMode, _aLeft, _aRight);
    LOG_INFO(TAG, "  Motor: spd=%.1f accel=%.1f curLim=%.1f",
             _motorSpeedLimit, _motorAcceleration, _motorCurrentLimit);
}

void SettingsManager::saveSettings() {
    Preferences prefs;
    prefs.begin("settings", false);  // read-write

    prefs.putUChar("yM", _yMode);
    prefs.putUChar("bM", _bMode);
    prefs.putUChar("aM", _aMode);

    prefs.putFloat("yL", _yLeft);
    prefs.putFloat("yR", _yRight);
    prefs.putFloat("bL", _bLeft);
    prefs.putFloat("bR", _bRight);
    prefs.putFloat("aL", _aLeft);
    prefs.putFloat("aR", _aRight);
    prefs.putFloat("spdLim", _motorSpeedLimit);
    prefs.putFloat("ppAccel", _motorAcceleration);
    prefs.putFloat("curLim", _motorCurrentLimit);

    prefs.end();
    LOG_INFO(TAG, "Settings saved to NVS");
}

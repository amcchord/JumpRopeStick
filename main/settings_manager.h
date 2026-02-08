#pragma once

// =============================================================================
// Settings Manager Module
// =============================================================================
// Manages user-configurable settings persisted to NVS (Non-Volatile Storage).
// Stores Y/B/A button action modes, arm presets, and motor speed limit.
//
// Button action modes:
//   0 = Go to Position (uses left/right radian values)
//   1 = Forward 360 (full forward arm rotation)
//   2 = Backward 360 (full backward arm rotation)
//   3 = Ground Slap (rapid back-and-forth near zero)
// =============================================================================

#include <Arduino.h>

// Button action mode constants
#define BTN_MODE_POSITION      0
#define BTN_MODE_FORWARD_360   1
#define BTN_MODE_BACKWARD_360  2
#define BTN_MODE_GROUND_SLAP   3
#define BTN_MODE_COUNT         4

class SettingsManager {
public:
    // Load settings from NVS. Call once in setup().
    void begin();

    // ---- Y Button ----
    uint8_t getYMode() const;
    float getYLeft() const;
    float getYRight() const;
    void setYConfig(uint8_t mode, float left, float right);

    // ---- B Button ----
    uint8_t getBMode() const;
    float getBLeft() const;
    float getBRight() const;
    void setBConfig(uint8_t mode, float left, float right);

    // ---- A Button ----
    uint8_t getAMode() const;
    float getALeft() const;
    float getARight() const;
    void setAConfig(uint8_t mode, float left, float right);

    // ---- Motor Tuning ----
    float getMotorSpeedLimit() const;
    void setMotorSpeedLimit(float limit);

    float getMotorAcceleration() const;
    void setMotorAcceleration(float accel);

    float getMotorCurrentLimit() const;
    void setMotorCurrentLimit(float limit);

    // Returns true (once) if any motor param was changed since last check.
    // Calling this clears the flag.
    bool consumeMotorParamsDirty();

    // Legacy setters (kept for backward compatibility with existing POST handler)
    void setYPreset(float left, float right);
    void setBPreset(float left, float right);
    void setAPreset(float left, float right);

private:
    // Button modes (0-3)
    uint8_t _yMode = BTN_MODE_POSITION;
    uint8_t _bMode = BTN_MODE_POSITION;
    uint8_t _aMode = BTN_MODE_POSITION;

    // Y button preset (left, right) in radians
    float _yLeft = 0.0f;
    float _yRight = 0.0f;

    // B button preset (left, right) in radians
    float _bLeft = 0.0f;
    float _bRight = 0.0f;

    // A button preset (left, right) in radians
    float _aLeft = 0.0f;
    float _aRight = 0.0f;

    // Motor tuning params
    float _motorSpeedLimit = 25.0f;
    float _motorAcceleration = 200.0f;
    float _motorCurrentLimit = 23.0f;

    // Dirty flag: set when any motor param changes, cleared by consumeMotorParamsDirty()
    bool _motorParamsDirty = false;

    // NVS persistence
    void loadSettings();
    void saveSettings();
};

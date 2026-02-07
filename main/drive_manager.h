#pragma once

// =============================================================================
// Drive Manager Module
// =============================================================================
// Reads joystick input from either stick (whichever has larger deflection),
// applies an expo curve for fine control, performs arcade-style mixing for
// differential drive, and outputs standard RC servo PPM via ESP32 LEDC.
//
// Runs as a dedicated FreeRTOS task on CPU0 at 50Hz for deterministic timing,
// completely isolated from display/WiFi/web on CPU1.
//
// Usage:
//   DriveManager drive;
//   drive.begin();                         // Spawns the drive task on CPU0
//   uint16_t left  = drive.getLeftPulse(); // 1000-2000 us
//   uint16_t right = drive.getRightPulse();// 1000-2000 us
//   float    ld    = drive.getLeftDrive(); // -1.0 to 1.0
// =============================================================================

#include <Arduino.h>

class DriveManager {
public:
    // Initialize LEDC servo channels and spawn the drive task on CPU0.
    // Must be called once in setup().
    void begin();

    // Current servo pulse widths in microseconds (1000-2000, center 1500).
    // Thread-safe: volatile, read from CPU1, written from CPU0.
    uint16_t getLeftPulse() const;
    uint16_t getRightPulse() const;

    // Current drive values as normalized floats (-1.0 to 1.0).
    // Thread-safe: volatile, read from CPU1, written from CPU0.
    float getLeftDrive() const;
    float getRightDrive() const;

private:
    bool _initialized = false;

    // Output state -- volatile for cross-core visibility (written on CPU0, read on CPU1)
    volatile uint16_t _leftPulseUs = 1500;
    volatile uint16_t _rightPulseUs = 1500;
    volatile float _leftDrive = 0.0f;
    volatile float _rightDrive = 0.0f;

    // Initialize LEDC timer and channels for servo PWM.
    bool initLedc();

    // Write a pulse width (in microseconds) to a LEDC servo channel.
    static void writeServo(uint8_t channel, uint16_t pulseUs);

    // Map a normalized drive value (-1.0 to 1.0) to a servo pulse width (us).
    // -1.0 -> SERVO_MIN_US (1000), 0.0 -> SERVO_CENTER_US (1500), 1.0 -> SERVO_MAX_US (2000)
    static uint16_t driveToMicroseconds(float drive);

    // Apply expo curve: blends linear and cubic for fine low-speed control.
    // Input and output are in [-1.0, 1.0].
    static float applyExpo(float input, float expo);

    // The FreeRTOS task function (static, receives DriveManager* as param).
    static void driveTaskFunc(void* param);
};

// Global instance (declared in sketch.cpp)
extern DriveManager g_driveManager;

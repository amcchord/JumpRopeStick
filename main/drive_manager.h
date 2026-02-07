#pragma once

// =============================================================================
// Drive Manager Module
// =============================================================================
// Reads joystick input from either stick (whichever has larger deflection),
// applies an expo curve for fine control, performs arcade-style mixing for
// differential drive, and outputs standard RC PPM servo signals via ESP32 LEDC.
//
// Control loop runs at 50Hz (20ms) for responsive drive.
//
// Usage:
//   DriveManager drive;
//   drive.begin();          // Call once in setup()
//   drive.update();         // Call every loop iteration (rate-limited internally)
//   uint16_t leftUs  = drive.getLeftPulse();   // 1000-2000
//   uint16_t rightUs = drive.getRightPulse();  // 1000-2000
// =============================================================================

#include <Arduino.h>

class DriveManager {
public:
    // Initialize LEDC PWM channels for both servos.
    // Must be called once in setup().
    void begin();

    // Run the drive control loop (rate-limited to 50Hz internally).
    // Reads the first connected controller, computes mixing, writes PWM.
    // Call every loop iteration.
    void update();

    // Current servo pulse widths in microseconds (1000-2000, center 1500).
    uint16_t getLeftPulse() const;
    uint16_t getRightPulse() const;

    // Current drive values as normalized floats (-1.0 to 1.0).
    float getLeftDrive() const;
    float getRightDrive() const;

private:
    unsigned long _lastUpdateMs = 0;
    bool _initialized = false;

    // Current output state
    uint16_t _leftPulseUs = 1500;
    uint16_t _rightPulseUs = 1500;
    float _leftDrive = 0.0f;
    float _rightDrive = 0.0f;

    // Apply expo curve: blends linear and cubic for fine low-speed control.
    // Input and output are in [-1.0, 1.0].
    float applyExpo(float input, float expo) const;

    // Convert a normalized drive value (-1.0 to 1.0) to servo pulse width (us).
    uint16_t driveToMicroseconds(float drive) const;

    // Write a pulse width to a LEDC channel.
    void writeServo(uint8_t channel, uint16_t pulseUs) const;
};

// Global instance (declared in sketch.cpp)
extern DriveManager g_driveManager;

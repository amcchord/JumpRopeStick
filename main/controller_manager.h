#pragma once

// =============================================================================
// Controller Manager Module
// =============================================================================
// Wraps Bluepad32 to provide multi-gamepad support with dead zone handling,
// state tracking, and connection management.
//
// Usage:
//   ControllerManager controllers;
//   controllers.begin();         // Call once in setup()
//   controllers.update();        // Call every loop iteration
//   const auto& state = controllers.getState(0);
//   if (state.connected) { ... }
// =============================================================================

#include <Arduino.h>
#include <Bluepad32.h>
#include "config.h"

// Per-controller state snapshot (safe to read from any context)
struct ControllerState {
    bool connected;     // Is this controller currently connected?

    // Axes (with dead zone applied): -512 to 512
    int16_t lx;         // Left stick X
    int16_t ly;         // Left stick Y
    int16_t rx;         // Right stick X
    int16_t ry;         // Right stick Y

    // Triggers (L2/R2 analog): 0 to 1023
    int16_t l2;         // Left trigger (L2)
    int16_t r2;         // Right trigger (R2)

    // Button bitmask
    uint16_t buttons;

    // D-pad bitmask
    uint8_t dpad;

    // Controller info
    char modelName[32];
};

class ControllerManager {
public:
    // Initialize Bluepad32 and register callbacks.
    // Must be called after Bluepad32 BTstack is initialized (in setup()).
    void begin();

    // Poll Bluepad32 for new data and update controller states.
    // Returns true if any controller data was updated.
    bool update();

    // Get the state of a specific controller (0-3).
    const ControllerState& getState(int index) const;

    // Get the number of currently connected controllers.
    int getConnectedCount() const;

    // Static callbacks for Bluepad32 (must be static for C callback interface)
    static void onConnected(ControllerPtr ctl);
    static void onDisconnected(ControllerPtr ctl);

private:
    // Apply dead zone to an axis value
    int16_t applyDeadZone(int16_t value) const;
};

// Global access (needed for Bluepad32 static callbacks)
extern ControllerManager g_controllerManager;

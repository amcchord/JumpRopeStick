#pragma once

// =============================================================================
// Motor Manager Module
// =============================================================================
// Manages Robstride motors over CAN bus via ESP32 TWAI peripheral.
// Handles motor discovery, status polling, and feedback parsing.
//
// The TJA1051T/3 transceiver on the Mini CAN Unit converts TWAI signals
// to/from the physical CAN bus. The ESP32 handles the CAN protocol.
//
// Usage:
//   MotorManager motors;
//   motors.begin();          // Call once in setup() -- inits TWAI, scans bus
//   motors.poll();           // Call every loop iteration -- processes CAN RX
// =============================================================================

#include <Arduino.h>
#include "robstride_protocol.h"

class MotorManager {
public:
    // Initialize TWAI peripheral and scan for motors on the CAN bus.
    // Must be called after config pins are available.
    void begin();

    // Process incoming CAN messages. Call every loop iteration.
    // Non-blocking -- returns immediately if no messages pending.
    void poll();

    // Re-scan the bus for motors (can be called anytime).
    void rescan();

    // ---- Status Accessors ----

    // Number of discovered motors (0 if none found or CAN not running).
    int getMotorCount() const;

    // Get status for motor at index (0-based, up to getMotorCount()-1).
    // Returns a zeroed struct if index is out of range.
    const RobstrideMotorStatus& getMotorStatus(int index) const;

    // Get the CAN ID for motor at index.
    uint8_t getMotorId(int index) const;

    // Is the TWAI driver running?
    bool isRunning() const;

    // ---- Motor Role Configuration (persisted to NVS) ----

    // Get/set the CAN ID assigned to left motor (0 = unassigned)
    uint8_t getLeftMotorId() const;
    void setLeftMotorId(uint8_t id);

    // Get/set the CAN ID assigned to right motor (0 = unassigned)
    uint8_t getRightMotorId() const;
    void setRightMotorId(uint8_t id);

    // Get the role label for a motor CAN ID: "L", "R", or "" if unassigned
    const char* getRoleLabel(uint8_t motorId) const;

    // Get status for the left/right motor (returns EMPTY_STATUS if unassigned or not found)
    const RobstrideMotorStatus& getLeftMotorStatus() const;
    const RobstrideMotorStatus& getRightMotorStatus() const;

private:
    // TWAI state
    bool _running = false;
    uint8_t _masterId = ROBSTRIDE_MASTER_ID;

    // Discovered motors (fixed-size array, no heap allocation)
    static const int MAX_MOTORS = 8;
    int _motorCount = 0;
    uint8_t _motorIds[MAX_MOTORS];
    RobstrideMotorStatus _motorStatus[MAX_MOTORS];

    // Motor role assignments (persisted to NVS)
    uint8_t _leftMotorId = 0;    // 0 = unassigned
    uint8_t _rightMotorId = 0;   // 0 = unassigned

    // NVS helpers
    void loadConfig();
    void saveConfig();

    // Status polling (GET_ID pings to trigger feedback with live position)
    unsigned long _lastStatusPollMs = 0;
    int _statusPollMotorIndex = 0;  // Round-robin through motors

    // Voltage polling
    unsigned long _lastVbusPollMs = 0;
    int _vbusPollMotorIndex = 0;   // Round-robin through motors

    // Parameter read state (for async VBUS reads)
    bool _paramReadPending = false;
    uint16_t _paramReadIndex = 0;
    uint8_t _paramReadMotorId = 0;
    float _paramReadValue = 0;
    unsigned long _paramReadStartMs = 0;

    // ---- Internal Methods ----

    // Initialize TWAI driver
    bool initTwai();

    // Scan for motors on the bus
    void scanMotors();

    // Find internal index for a motor ID (-1 if not found)
    int findMotorIndex(uint8_t motorId) const;

    // Add a motor if not already known. Returns index or -1 if full.
    int addMotor(uint8_t motorId);

    // Build extended CAN ID for Robstride protocol
    uint32_t buildExtendedId(uint8_t commType, uint8_t motorId, uint16_t extraData = 0);

    // Send a CAN message
    bool sendMessage(uint32_t id, const uint8_t* data, uint8_t len);

    // Process a single received CAN message
    void processMessage(uint32_t canId, const uint8_t* data, uint8_t len);

    // Parse motor feedback (comm type 0x02)
    void parseMotorFeedback(uint8_t motorId, uint32_t canId, const uint8_t* data);

    // Parse parameter response
    void parseParameterResponse(uint8_t motorId, const uint8_t* data);

    // Request a parameter read from a motor
    bool requestParameter(uint8_t motorId, uint16_t paramIndex);

    // Periodic status pings (GET_ID to trigger feedback with live position)
    void pollStatus();

    // Check for stale/disconnected motors
    void checkStaleness();

    // Remove a motor from the list by index (shifts remaining entries)
    void removeMotor(int index);

    // Periodic VBUS voltage polling
    void pollVoltage();

    // Conversion helpers
    static float uintToFloat(uint16_t x, float xMin, float xMax, int bits);
    static uint16_t floatToUint(float x, float xMin, float xMax, int bits);
};

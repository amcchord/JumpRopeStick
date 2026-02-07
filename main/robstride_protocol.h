#pragma once

// =============================================================================
// Robstride Motor CAN Protocol Definitions
// =============================================================================
// Protocol constants for Robstride RS00-RS06 series motors.
// Cross-referenced from:
//   - JumpRopeM5 library
//   - sirwart/robstride Python SDK
//   - RobStride/robstride_ros_sample (official ROS2 C++)
//
// CAN 2.0B Extended (29-bit) frames at 1 Mbps.
//
// Extended CAN ID format:
//   Bits 28-24: Communication type (5 bits)
//   Bits 23-16: Extra data (varies by command)
//   Bits 15-8:  Master/Host CAN ID
//   Bits 7-0:   Motor CAN ID
// =============================================================================

#include <stdint.h>

// =============================================================================
// Default Master ID
// =============================================================================
static const uint8_t ROBSTRIDE_MASTER_ID = 0xFD;

// =============================================================================
// Communication Types (bits 24-28 of extended CAN ID)
// =============================================================================
namespace RobstrideComm {
    static const uint8_t GET_ID                = 0x00;  // Motor discovery / identification
    static const uint8_t MOTION_CONTROL        = 0x01;  // MIT-style control (torque in CAN ID bits 8-23)
    static const uint8_t MOTOR_FEEDBACK        = 0x02;  // Status response from motor
    static const uint8_t MOTOR_ENABLE          = 0x03;  // Enable motor
    static const uint8_t MOTOR_STOP            = 0x04;  // Disable motor (data[0]=1 clears faults)
    static const uint8_t SET_MECHANICAL_ZERO   = 0x06;  // Set current pos as zero (data[0]=1)
    static const uint8_t SET_CAN_ID            = 0x07;  // Change motor CAN ID (new ID in bits 16-23)
    static const uint8_t GET_SINGLE_PARAM      = 0x11;  // Read parameter
    static const uint8_t SET_SINGLE_PARAM      = 0x12;  // Write parameter
    static const uint8_t ERROR_FEEDBACK        = 0x15;  // Fault feedback frame
    static const uint8_t MOTOR_DATA_SAVE       = 0x16;  // Save parameters to flash
    static const uint8_t BAUD_RATE_CHANGE      = 0x17;  // Change baud rate (needs reboot)
    static const uint8_t PROACTIVE_REPORT_SET  = 0x18;  // Set proactive reporting
    static const uint8_t MOTOR_MODE_SET        = 0x19;  // Set protocol mode (needs reboot)
}

// =============================================================================
// Control Modes (written to parameter 0x7005 as uint8)
// =============================================================================
namespace RobstrideMode {
    static const uint8_t OPERATION_CONTROL = 0;  // MIT-style: pos + vel + kp + kd + torque
    static const uint8_t POSITION_PP       = 1;  // Position mode (interpolated)
    static const uint8_t SPEED_CONTROL     = 2;  // Speed/velocity control
    static const uint8_t CURRENT_CONTROL   = 3;  // Current/torque control
    static const uint8_t ZERO_CALIBRATION  = 4;  // Zero point calibration
    static const uint8_t POSITION_CSP      = 5;  // Position mode (cyclic sync)
}

// =============================================================================
// Motor State (from feedback CAN ID bits 22-23)
// =============================================================================
namespace RobstrideState {
    static const uint8_t RESET       = 0;  // Motor in reset
    static const uint8_t CALIBRATION = 1;  // Motor calibrating
    static const uint8_t RUNNING     = 2;  // Motor enabled and running
}

// =============================================================================
// Parameter Indices (for read/write via comm types 0x11/0x12)
// =============================================================================
namespace RobstrideParam {
    // Read/Write parameters
    static const uint16_t RUN_MODE         = 0x7005;  // Control mode (uint8, 0-5)
    static const uint16_t IQ_REF           = 0x7006;  // Current command (float, -23~23A)
    static const uint16_t SPD_REF          = 0x700A;  // Speed command (float, rad/s)
    static const uint16_t LIMIT_TORQUE     = 0x700B;  // Torque limit (float, Nm)
    static const uint16_t CUR_KP           = 0x7010;  // Current Kp (float, default 0.125)
    static const uint16_t CUR_KI           = 0x7011;  // Current Ki (float, default 0.0158)
    static const uint16_t CUR_FILT_GAIN    = 0x7014;  // Current filter (float, 0~1.0)
    static const uint16_t LOC_REF          = 0x7016;  // Position command (float, rad)
    static const uint16_t LIMIT_SPD        = 0x7017;  // Speed limit (float, rad/s)
    static const uint16_t LIMIT_CUR        = 0x7018;  // Current limit (float, A)

    // Read-only parameters
    static const uint16_t MECH_POS         = 0x7019;  // Mechanical position (float, rad)
    static const uint16_t IQF              = 0x701A;  // Filtered Iq (float, A)
    static const uint16_t MECH_VEL         = 0x701B;  // Mechanical velocity (float, rad/s)
    static const uint16_t VBUS             = 0x701C;  // Bus voltage (float, V)
    static const uint16_t ROTATION         = 0x701D;  // Revolution count (int16)

    // Tuning parameters
    static const uint16_t LOC_KP           = 0x701E;  // Position Kp (float)
    static const uint16_t SPD_KP           = 0x701F;  // Speed Kp (float)
    static const uint16_t SPD_KI           = 0x7020;  // Speed Ki (float)
    static const uint16_t SPD_FILT_GAIN    = 0x7021;  // Speed filter gain (float)

    // Position mode (PP) parameters
    static const uint16_t PP_SPEED         = 0x7024;  // PP max speed (float, rad/s)
    static const uint16_t PP_ACCELERATION  = 0x7025;  // PP acceleration (float, rad/s^2)
    static const uint16_t SPD_ACCELERATION = 0x7026;  // Speed mode accel (float, rad/s^2)
}

// =============================================================================
// Error/Fault Codes (bits in feedback error field)
// =============================================================================
namespace RobstrideFault {
    static const uint8_t NONE              = 0x00;
    static const uint8_t UNDERVOLTAGE      = 0x01;  // Bus undervoltage
    static const uint8_t OVERCURRENT       = 0x02;  // Overcurrent
    static const uint8_t OVERTEMPERATURE   = 0x04;  // Over temperature
    static const uint8_t ENCODER_MAGNETIC  = 0x08;  // Magnetic encoder fault
    static const uint8_t ENCODER_HALL      = 0x10;  // Hall encoder fault
    static const uint8_t UNCALIBRATED      = 0x20;  // Motor not calibrated
}

// =============================================================================
// Motor Specifications (position, velocity, torque limits and PID gains)
// =============================================================================
struct RobstrideMotorSpec {
    float positionLimit;   // rad (typically 4*PI)
    float velocityLimit;   // rad/s
    float torqueLimit;     // Nm
    float kpMax;           // Maximum Kp
    float kdMax;           // Maximum Kd
};

// Motor type enum
enum class RobstrideMotorType : uint8_t {
    RS00 = 0, RS01 = 1, RS02 = 2, RS03 = 3,
    RS04 = 4, RS05 = 5, RS06 = 6
};

// Specs lookup table (indexed by RobstrideMotorType)
// Confirmed by ROS sample ACTUATOR_OPERATION_MAPPING
static const RobstrideMotorSpec ROBSTRIDE_SPECS[] = {
    // RS00: position=4pi, vel=50, torque=17, kp=500, kd=5
    {4.0f * 3.14159265f, 50.0f, 17.0f, 500.0f, 5.0f},
    // RS01: position=4pi, vel=44, torque=17, kp=500, kd=5
    {4.0f * 3.14159265f, 44.0f, 17.0f, 500.0f, 5.0f},
    // RS02: position=4pi, vel=44, torque=17, kp=500, kd=5
    {4.0f * 3.14159265f, 44.0f, 17.0f, 500.0f, 5.0f},
    // RS03: position=4pi, vel=50, torque=60, kp=5000, kd=100
    {4.0f * 3.14159265f, 50.0f, 60.0f, 5000.0f, 100.0f},
    // RS04: position=4pi, vel=15, torque=120, kp=5000, kd=100
    {4.0f * 3.14159265f, 15.0f, 120.0f, 5000.0f, 100.0f},
    // RS05: position=4pi, vel=33, torque=17, kp=500, kd=5
    {4.0f * 3.14159265f, 33.0f, 17.0f, 500.0f, 5.0f},
    // RS06: position=4pi, vel=20, torque=60, kp=5000, kd=100
    {4.0f * 3.14159265f, 20.0f, 60.0f, 5000.0f, 100.0f},
};

static const int ROBSTRIDE_SPEC_COUNT = sizeof(ROBSTRIDE_SPECS) / sizeof(ROBSTRIDE_SPECS[0]);

// Default spec (RS02) used when motor type is unknown
static const RobstrideMotorSpec ROBSTRIDE_DEFAULT_SPEC = {
    4.0f * 3.14159265f, 44.0f, 17.0f, 500.0f, 5.0f
};

// =============================================================================
// Motor Status Structure
// =============================================================================
struct RobstrideMotorStatus {
    float position;      // Current position in radians
    float velocity;      // Current velocity in rad/s
    float torque;        // Current torque in Nm
    float temperature;   // Temperature in Celsius
    float voltage;       // Bus voltage in Volts (from VBUS param read)
    uint8_t errorCode;   // Error/fault code bits
    uint8_t mode;        // Motor state (0=reset, 1=calibration, 2=running)
    uint8_t runMode;     // Control mode (RobstrideMode, from RUN_MODE param)
    bool enabled;        // Motor enabled (mode == RUNNING)
    bool hasFault;       // Whether any fault bits are set
    bool stale;          // No feedback received recently (motor may be disconnected)
    unsigned long lastUpdateMs;  // millis() of last feedback received
};

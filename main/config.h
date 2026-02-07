#pragma once

// =============================================================================
// JumpRopeStick Configuration
// =============================================================================
// Edit this file to change WiFi credentials, pin assignments, and tuning values.
// =============================================================================

// -- WiFi Credentials --------------------------------------------------------
#define WIFI_SSID        "McWifi"
#define WIFI_PASSWORD    "gonewireless"

// -- Pin Assignments ---------------------------------------------------------
// Power
#define PIN_HOLD         4    // Must set HIGH to keep device powered on

// Servo PPM outputs (future use)
#define PIN_SERVO_LEFT   25   // Left wheel servo
#define PIN_SERVO_RIGHT  26   // Right wheel servo

// CAN bus via Mini CAN Unit (future use)
#define PIN_CAN_TX       32   // TWAI TX -> Mini CAN Unit (HY2.0-4P Yellow)
#define PIN_CAN_RX       33   // TWAI RX -> Mini CAN Unit (HY2.0-4P White)

// Buttons (directly on the M5StickC Plus 2)
#define PIN_BUTTON_A     37
#define PIN_BUTTON_B     39
#define PIN_BUTTON_C     35   // Power button

// -- WiFi Settings -----------------------------------------------------------
#define WIFI_RECONNECT_INTERVAL_MIN_MS   1000    // Start at 1 second
#define WIFI_RECONNECT_INTERVAL_MAX_MS   30000   // Max 30 seconds backoff
#define WIFI_CONNECT_TIMEOUT_MS          15000   // Initial connect timeout

// -- Web Server Settings -----------------------------------------------------
#define WEB_SERVER_PORT          80

// -- Controller Settings -----------------------------------------------------
#define CONTROLLER_MAX_COUNT     4       // Bluepad32 supports up to 4
#define CONTROLLER_DEADZONE      30      // Joystick dead zone (out of 512)

// -- Display Settings --------------------------------------------------------
#define DISPLAY_UPDATE_MS        200     // 5Hz display refresh rate
#define DISPLAY_WIDTH            135
#define DISPLAY_HEIGHT           240

// -- Debug Logging -----------------------------------------------------------
// Log levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#define LOG_LEVEL                3       // INFO level by default
#define LOG_SERIAL_BAUD          115200

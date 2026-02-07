#pragma once

// =============================================================================
// JumpRopeStick Configuration
// =============================================================================
// Edit this file to change WiFi credentials, pin assignments, and tuning values.
// =============================================================================

// -- WiFi Credentials --------------------------------------------------------
#define WIFI_SSID        "McLab"
#define WIFI_PASSWORD    "gogogadget"

// -- Pin Assignments ---------------------------------------------------------
// Power
#define PIN_HOLD         4    // Must set HIGH to keep device powered on

// Servo / ESC outputs (standard RC PPM)
#define PIN_SERVO_LEFT   25   // Left wheel ESC
#define PIN_SERVO_RIGHT  26   // Right wheel ESC

// CAN bus via Mini CAN Unit (TJA1051T/3)
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

// -- Drive / Servo Settings --------------------------------------------------
// Drive control loop
#define DRIVE_UPDATE_MS          10      // 100Hz control loop for low latency

// Expo curve: 0.0 = linear, 1.0 = full cubic.
// Blends linear and cubic: out = (1-expo)*in + expo*in^3
#define DRIVE_EXPO               0.7f

// Slow mode: hold L1 or R1 (paddle buttons) to limit max output.
// Value is the fraction of full output (0.30 = 30%).
#define DRIVE_SLOW_MODE_SCALE    0.30f

// Standard RC servo PPM signal
#define SERVO_MIN_US             1000    // Full reverse (or minimum throttle)
#define SERVO_MAX_US             2000    // Full forward (or maximum throttle)
#define SERVO_CENTER_US          1500    // Neutral / stop
#define SERVO_FREQ_HZ            50      // Standard 50Hz servo refresh rate

// LEDC channels for servo PWM (ESP32 has 16 channels, 0-15)
#define LEDC_SERVO_LEFT_CH       0
#define LEDC_SERVO_RIGHT_CH      1
#define LEDC_SERVO_TIMER         LEDC_TIMER_0
#define LEDC_SERVO_SPEED_MODE    LEDC_LOW_SPEED_MODE
#define LEDC_SERVO_RESOLUTION    16      // 16-bit resolution for fine pulse control

// FreeRTOS drive task (runs on CPU0 alongside BTstack)
#define DRIVE_TASK_CORE          0       // Pin to CPU0
#define DRIVE_TASK_PRIORITY      5       // Higher than default (1)
#define DRIVE_TASK_STACK         4096    // Stack size in bytes

// -- CAN Bus / Motor Settings -----------------------------------------------
#define CAN_BAUD_RATE            1000000     // 1 Mbps
#define CAN_MASTER_ID            0xFD        // Our CAN master address
#define CAN_SCAN_TIMEOUT_MS      300         // Motor scan timeout in ms
#define MOTOR_MAX_COUNT          8           // Max motors tracked
#define MOTOR_VBUS_POLL_MS       2000        // Voltage re-read interval in ms
#define MOTOR_STATUS_POLL_MS     100         // Status ping interval per motor (ms)
#define MOTOR_STALE_MS           2000        // Mark motor stale after no feedback (ms)
#define MOTOR_REMOVE_MS          10000       // Remove motor from list after no feedback (ms)

// -- Motor Trim Settings -----------------------------------------------------
#define TRIM_STEP_RAD            0.01f   // Position nudge per d-pad press (~0.6 degrees)

// -- Motor Stick Control Settings --------------------------------------------
#define STICK_UPDATE_MS          20      // Position command rate (50 Hz)
#define STICK_MAX_JOG_RAD_S      3.0f   // Max jog speed in rad/s at full stick deflection (~170 deg/s)
#define R2_TRIGGER_DEADZONE      20      // Analog trigger deadzone (out of 1023)

// -- Debug Logging -----------------------------------------------------------
// Log levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#define LOG_LEVEL                3       // INFO level by default
#define LOG_SERIAL_BAUD          115200

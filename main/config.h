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

// Speed mode: default is slow (30%), hold R1 (shoulder button) for full speed.
// Value is the fraction of full output (0.30 = 30%).
#define DRIVE_SLOW_MODE_SCALE    0.30f

// Drive output smoothing (exponential low-pass filter).
// 0.0 = no smoothing (instant), 1.0 = maximum smoothing (never reaches target).
// At 100Hz update rate, 0.15 gives a ~60ms rise time, 0.25 gives ~100ms.
#define DRIVE_SMOOTHING          0.15f

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

// -- Motor Speed / Tuning Settings -------------------------------------------
#define MOTOR_SPEED_LIMIT        25.0f   // PP mode max speed (rad/s). JumpRopeM4 used 25.
#define MOTOR_PP_ACCELERATION   200.0f   // PP mode acceleration (rad/s^2). JumpRopeM4 used 200.
#define MOTOR_CURRENT_LIMIT      23.0f   // Motor current limit (A). Max for RS01 is ~23A.

// -- Button Preset Defaults (Y / B / A) -------------------------------------
// These are the default arm positions (radians) for button presets.
// Users can customize these via the web settings page; values persist to NVS.
// Reference positions: Front=(0,0), Up=(-1.79,-1.79), Back=(-3.54,-3.54),
//   Stand on Arms=(0.65,-4.25)
#define DEFAULT_Y_PRESET_LEFT   -1.79f   // Y button left arm default (Up)
#define DEFAULT_Y_PRESET_RIGHT  -1.79f   // Y button right arm default (Up)
#define DEFAULT_B_PRESET_LEFT   -3.54f   // B button left arm default (Back)
#define DEFAULT_B_PRESET_RIGHT  -3.54f   // B button right arm default (Back)
#define DEFAULT_A_PRESET_LEFT    0.0f    // A button left arm default (Front)
#define DEFAULT_A_PRESET_RIGHT   0.0f    // A button right arm default (Front)

// -- Motor Trim Settings -----------------------------------------------------
#define TRIM_STEP_RAD            0.01f   // Position nudge per d-pad press (~0.6 degrees)

// -- Motor Stick Control Settings --------------------------------------------
#define STICK_UPDATE_MS          20      // Position command rate (50 Hz)
#define STICK_MAX_JOG_RAD_S      3.0f   // Max jog speed in rad/s at full stick deflection (~170 deg/s)
#define R2_TRIGGER_DEADZONE      20      // Analog trigger deadzone (out of 1023)

// -- IMU Settings ------------------------------------------------------------
#define IMU_UPDATE_MS            10      // 100Hz IMU polling (fast for PID balance)
#define IMU_FLIP_THRESHOLD       0.5f   // Accel threshold (g) for upside-down hysteresis

// -- Self-Righting Settings (Select button) ----------------------------------
#define SELF_RIGHT_PREP_POS     -1.79f  // "Up" position (touches ground when inverted)
#define SELF_RIGHT_PUSH_POS      0.5f   // Slightly past "Front" (strong push)
#define SELF_RIGHT_DRIVE         0.4f   // Forward drive override during push
#define SELF_RIGHT_PREP_MS       500    // Time for arms to reach prep position (ms)
#define SELF_RIGHT_PUSH_MS       800    // Time for push + flip (ms)

// -- Nose-Down Balance Settings (X button) -----------------------------------
// Arm positions (target space: left, right)
// Tipping position -- tips robot onto nose (empirical, asymmetric)
#define ND_TIP_LEFT             -5.45f
#define ND_TIP_RIGHT            -1.05f
// Final balance position -- arms pointing to sky (symmetric, ~Back)
#define ND_BALANCE_LEFT         -3.45f
#define ND_BALANCE_RIGHT        -3.45f
// Arm ramp duration from tipping to balance position (ms) -- very slow to maintain balance
#define ND_ARM_RAMP_MS           8000

// PID gains (will need empirical tuning)
#define ND_PID_KP                1.0f   // Proportional gain (reduced from 2.0 to lower oscillation)
#define ND_PID_KI                0.1f   // Integral gain
#define ND_PID_KD                1.5f   // Derivative gain (increased from 0.5 for more damping)
#define ND_PID_OUTPUT_LIMIT      0.8f   // Max arm offset from nominal (rad)
#define ND_PID_INTEGRAL_LIMIT    0.3f   // Anti-windup clamp

// Gain scheduling
#define ND_MIN_SENSITIVITY       0.2f   // Threshold to switch uniform vs differential mode
#define ND_MAX_ARM_OFFSET        0.5f   // Hard clamp on per-arm offset (rad) to prevent position explosions

// Nose-down setpoint (pitch angle in radians; 0=level, -pi/2=nose-down)
#define ND_PITCH_SETPOINT       -1.5708f  // -pi/2

// Pitch-gated ramp: only advance arm ramp when pitch error is small
#define ND_RAMP_ERROR_GATE_DEG   15.0f    // Freeze ramp when |pitch error| > this (degrees)

// Transition parameters
#define ND_TIP_SETTLE_MS         800    // Ignore pitch readings for this long after arms move (accel spike settle)
#define ND_TIP_TIMEOUT_MS        8000   // Abort if not balanced within this time (ms)
#define ND_PITCH_ENGAGED_DEG     60.0f  // Engage PID when pitch exceeds this (degrees)
#define ND_PITCH_CONFIRM_COUNT   10     // Require this many consecutive readings above threshold (~100ms at 100Hz)
#define ND_PITCH_LOST_DEG        30.0f  // If pitch drops below this during balance, re-enter tipping
#define ND_EXIT_MS               1200   // Duration of arm sweep to Front (ms)

// -- Debug Logging -----------------------------------------------------------
// Log levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#define LOG_LEVEL                3       // INFO level by default
#define LOG_SERIAL_BAUD          115200

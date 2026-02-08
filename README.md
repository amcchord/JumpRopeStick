# JumpRopeStick

A Bluetooth gamepad-controlled robot with articulated arms, running on an M5StickC Plus 2 (ESP32). Features CAN bus motor control for dual robot arms, servo-driven wheels, IMU-based self-righting and nose-down balancing, a WiFi web dashboard, and an on-device status display.

## Features

- **Bluetooth Gamepad Control** -- Connect up to 4 controllers simultaneously via Bluepad32 (optimized for 8BitDo Pro 3)
- **CAN Bus Arm Motors** -- Dual RobStride RS-02 motors in position-profiled mode for precise arm positioning
- **Servo PPM Wheel Drive** -- Tank-style driving with expo curve, configurable slow mode (30% default), and exponential smoothing
- **IMU Tricks** -- Upside-down detection with automatic drive inversion, self-righting sequence, and nose-down PID balance
- **WiFi Web Dashboard** -- Real-time status monitoring via WebSocket at 10 Hz, plus a settings page for tuning motor parameters and button presets (persisted to NVS flash)
- **On-Device Display** -- 1.14" TFT LCD showing WiFi status, controller inputs, motor state, and system info at 5 Hz
- **Configurable Button Presets** -- Y/B/A buttons support four modes: Position hold, Forward 360, Backward 360, and Ground Slap
- **Home Preset Cycling** -- Four arm home positions (Front, Up, Back, L-Front/R-Back) with R2 trigger interpolation to target poses

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | M5StickC Plus 2 (ESP32-PICO-V3-02, dual-core Xtensa LX6 @ 240 MHz) |
| **Flash / PSRAM** | 8 MB / 2 MB |
| **Display** | 1.14" TFT LCD, 135x240, ST7789V2 (SPI, RGB565) |
| **IMU** | MPU6886 (3-axis accelerometer + 3-axis gyroscope, I2C) |
| **Controller** | 8BitDo Pro 3 (Bluetooth Classic, TMR joysticks, Hall Effect triggers) |
| **Arm Motors** | RobStride RS-02 x2 (17 Nm max torque, 44 rad/s max speed, CAN bus) |
| **CAN Transceiver** | M5Stack Mini CAN Unit (TJA1051T, HY2.0-4P Grove connector) |
| **Wheel ESCs** | Standard RC ESCs driven via 50 Hz servo PPM |

### Pin Assignments

| Pin | Function | Notes |
|-----|----------|-------|
| G4 | HOLD | Must set HIGH to keep device powered on |
| G25 | Servo PPM Left Wheel | External header |
| G26 | Servo PPM Right Wheel | External header |
| G32 | CAN TX (TWAI) | Grove port yellow wire to Mini CAN Unit |
| G33 | CAN RX (TWAI) | Grove port white wire from Mini CAN Unit |
| G21 | I2C SDA | MPU6886 IMU + BM8563 RTC |
| G22 | I2C SCL | MPU6886 IMU + BM8563 RTC |
| G37 | Button A | Active LOW |
| G39 | Button B | Active LOW |
| G35 | Button C (Power) | Press >2 s to power on, >6 s to power off |

### CAN Bus Wiring

```
M5StickC Plus 2          Mini CAN Unit (TJA1051T)       RobStride Motor
  HY2.0-4P Port
  G32 (Yellow) -------> CAN_TX -----> CAN H ---------> CAN H
  G33 (White)  -------> CAN_RX -----> CAN L ---------> CAN L
  5V  (Red)    -------> VCC
  GND (Black)  -------> GND -------> GND -------------> GND
```

## Controls Reference

All controls are for the primary (first connected) gamepad.

### Driving

| Input | Action |
|-------|--------|
| Right stick Y | Forward / reverse drive |
| Right stick X | Steering (tank mix) |
| R1 (shoulder) | Hold for full speed (default is 30% slow mode) |

### Arm Control

| Input | Action |
|-------|--------|
| Left stick Y | Jog both arms forward / backward (velocity mode, up to 3 rad/s) |
| Left stick X | Spread arms apart -- positional difference between left and right (up to +/- PI rad) |
| R2 trigger | Interpolate from current home preset toward its trigger target |
| D-pad Up/Down | Fine-trim left arm position (+/- 0.01 rad per step, repeats while held) |
| D-pad Right/Left | Fine-trim right arm position (+/- 0.01 rad per step, repeats while held) |
| L1 | Cycle home presets: Front, Up, Back, L-Front/R-Back |
| L3 (stick click) | Smart return to nearest home position (direction-aware) |
| Sys | Set mechanical zero on both motors at current position |

### Button Presets (Y / B / A)

Each button can be configured to one of four modes via the web dashboard:

| Mode | Behavior |
|------|----------|
| **Position** | Hold button to move arms to a saved position; release to return |
| **Forward 360** | Single press adds a full forward rotation (+2 PI rad) |
| **Backward 360** | Single press adds a full backward rotation (-2 PI rad) |
| **Ground Slap** | Rapid 3-cycle oscillation of arms (+/- 0.10 rad) |

Default presets: Y = Up (-1.79 rad), B = Back (-3.54 rad), A = Front (0.0 rad).

### Special Moves

| Input | Action |
|-------|--------|
| Select | Self-righting sequence (arms sweep to push robot upright) |
| X | Toggle nose-down PID balance (tips forward onto nose, then balances with arms) |

## Architecture

JumpRopeStick runs on a dual-core ESP32 with a clear division of responsibilities:

- **CPU0** -- Bluepad32 / BTstack (Bluetooth gamepad host) and the drive FreeRTOS task (servo PPM output at 100 Hz)
- **CPU1** -- Main application loop (Arduino `setup()` / `loop()`) running all other modules

The main loop polls each module sequentially with no event bus or callback system between modules. This keeps the architecture simple and deterministic.

### Modules

| Module | File(s) | Purpose |
|--------|---------|---------|
| **Controller Manager** | `controller_manager.h/.cpp` | Bluepad32 wrapper, multi-controller state, dead zone, input normalization |
| **Drive Manager** | `drive_manager.h/.cpp` | Servo PPM output on a dedicated FreeRTOS task (CPU0, 100 Hz) with expo curve and smoothing |
| **Motor Manager** | `motor_manager.h/.cpp` | CAN bus (TWAI) driver for RobStride motors -- scan, enable, position commands, status polling |
| **Display Manager** | `display_manager.h/.cpp` | On-device LCD rendering via M5Unified double-buffered sprites at 5 Hz |
| **WiFi Manager** | `wifi_manager.h/.cpp` | Auto-connect and reconnect with exponential backoff (1 s to 30 s) |
| **Web Server** | `web_server.h/.cpp`, `web_ui.h` | HTTP server on port 80 + WebSocket at `/ws` broadcasting JSON status at 10 Hz |
| **Settings Manager** | `settings_manager.h/.cpp` | NVS-persisted user settings (button presets, motor tuning parameters) |
| **RobStride Protocol** | `robstride_protocol.h` | CAN frame ID encoding, parameter addresses, and protocol constants |
| **Debug Log** | `debug_log.h/.cpp` | Severity-leveled serial logging (ERROR / WARN / INFO / DEBUG) |

## Project Structure

```
JumpRopeStick/
├── main/                          # Application source code
│   ├── main.c                     # Bluepad32/BTstack init (runs on CPU0)
│   ├── sketch.cpp                 # Arduino setup/loop (runs on CPU1)
│   ├── config.h                   # All compile-time configuration
│   ├── controller_manager.h/.cpp  # Bluetooth gamepad wrapper
│   ├── drive_manager.h/.cpp       # Servo PPM wheel drive (FreeRTOS task)
│   ├── motor_manager.h/.cpp       # CAN bus motor control (TWAI + RobStride)
│   ├── display_manager.h/.cpp     # On-device LCD status display
│   ├── wifi_manager.h/.cpp        # WiFi connection management
│   ├── web_server.h/.cpp          # HTTP + WebSocket server
│   ├── web_ui.h                   # Embedded HTML/JS dashboard
│   ├── settings_manager.h/.cpp    # NVS-persisted user settings
│   ├── robstride_protocol.h       # CAN protocol definitions
│   └── debug_log.h/.cpp           # Serial debug logging
├── components/                    # Git submodules
│   ├── arduino/                   # Arduino core as ESP-IDF component
│   ├── bluepad32/                 # Bluetooth gamepad library
│   ├── bluepad32_arduino/         # Bluepad32 Arduino bindings
│   └── btstack/                   # Bluetooth stack (replaces default ESP32 BT)
├── docs/                          # Reference documentation
│   ├── architecture.md            # System architecture and design decisions
│   ├── hardware.md                # M5StickC Plus 2 hardware reference
│   ├── controllers.md             # Controller and Bluepad32 reference
│   ├── robstride_motors.md        # RobStride CAN motor protocol
│   └── Positions.md               # Known arm positions
├── patches/                       # ESP-IDF patches
├── platformio.ini                 # PlatformIO build configuration
├── sdkconfig.defaults             # ESP-IDF configuration defaults
└── partitions.csv                 # Custom flash partition table
```

## Building and Flashing

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- USB-C cable connected to the M5StickC Plus 2

### Clone

The project uses git submodules for Bluepad32, BTstack, and the Arduino ESP-IDF component. Clone with `--recursive`:

```bash
git clone --recursive https://github.com/your-username/JumpRopeStick.git
cd JumpRopeStick
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### Build and Upload

```bash
# Build
pio run

# Upload firmware
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

### Framework Note

This project uses the **ESP-IDF** framework with **Arduino added as a component** (not the Arduino framework directly). This is required because Bluepad32 replaces the standard ESP32 Bluetooth stack with BTstack, which is incompatible with Arduino-ESP32's built-in Bluetooth. The project is based on the [esp-idf-arduino-bluepad32-template](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template).

## Configuration

### WiFi Credentials

Edit `main/config.h`:

```cpp
#define WIFI_SSID        "McLab"
#define WIFI_PASSWORD    "gogogadget"
```

### Motor Tuning

Default motor parameters in `main/config.h` (also adjustable at runtime via the web dashboard):

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MOTOR_SPEED_LIMIT` | 25.0 rad/s | Max velocity during profiled moves |
| `MOTOR_PP_ACCELERATION` | 200.0 rad/s^2 | Acceleration / deceleration rate |
| `MOTOR_CURRENT_LIMIT` | 23.0 A | Motor current limit |

### Drive Tuning

| Parameter | Default | Description |
|-----------|---------|-------------|
| `DRIVE_EXPO` | 0.7 | Expo curve blend (0 = linear, 1 = full cubic) |
| `DRIVE_SLOW_MODE_SCALE` | 0.30 | Speed fraction in slow mode (hold R1 for full) |
| `DRIVE_SMOOTHING` | 0.15 | Exponential low-pass filter (0 = instant, 1 = max) |
| `DRIVE_UPDATE_MS` | 10 | Control loop period (100 Hz) |

### Pairing an 8BitDo Controller

1. Set the mode switch on the bottom of the controller to **A** (Android) or **D** (D-input)
2. Hold **Start + pairing button** until the LEDs flash rapidly
3. Bluepad32 will discover and connect the controller automatically

Most Bluetooth Classic gamepads are supported (Sony DualSense/DualShock, Nintendo Switch Pro, Xbox Wireless, etc.). See the [Bluepad32 docs](https://bluepad32.readthedocs.io/) for the full compatibility list.

### Web Dashboard

Once WiFi is connected, the device logs its IP address to serial. Open `http://<device-ip>/` in a browser to access:

- **Status page** -- Real-time controller inputs, motor positions, velocities, IMU pitch, system health (updated at 10 Hz via WebSocket)
- **Settings page** -- Adjust button preset positions and modes, motor speed/acceleration/current limits, and motor role assignments. Changes are saved to NVS flash and persist across reboots.

## Known Arm Positions

Reference positions in radians (left motor / right motor in target space, before right-motor negation):

| Position | Left | Right |
|----------|------|-------|
| Front | 0.00 | 0.00 |
| Straight Up | -1.79 | -1.79 |
| Back | -3.54 | -3.54 |
| Stand on Arms | 0.65 | -4.25 |
| Straight Down | 1.37 | -1.37 |

## Documentation

Detailed reference documentation is available in the `docs/` directory:

- **[architecture.md](docs/architecture.md)** -- System architecture, module communication, memory budget, and design decisions
- **[hardware.md](docs/hardware.md)** -- M5StickC Plus 2 pin map, power management, display specs, IMU details, and datasheets
- **[controllers.md](docs/controllers.md)** -- 8BitDo Pro 3 pairing, Bluepad32 API, controller data ranges, and WiFi/BT coexistence notes
- **[robstride_motors.md](docs/robstride_motors.md)** -- RobStride RS-02 specs, CAN frame format, control modes, TWAI configuration, and error codes
- **[Positions.md](docs/Positions.md)** -- Known arm motor positions

## License

MIT

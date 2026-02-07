# JumpRopeStick - Architecture & Design Decisions

## Project Overview

JumpRopeStick is a drivable robot controlled by 8BitDo Pro 3 Bluetooth gamepads.
It runs on an M5StickC Plus 2 (ESP32-PICO-V3-02) and features:

- Bluetooth gamepad input (up to 4 controllers via Bluepad32)
- Servo PPM output for wheels (G25, G26)
- CAN bus motor control for robot arms (RobStride RS-02 motors)
- IMU-based tricks (e.g., self-balancing)
- WiFi web dashboard for real-time status monitoring
- OLED display for on-device status

## Framework Decision: ESP-IDF + Arduino Component

**Why not pure Arduino?**
Bluepad32 requires replacing the ESP32 Bluetooth stack with BTstack. This is
incompatible with the standard Arduino-ESP32 Bluetooth implementation. The only
way to use Bluepad32 with Arduino APIs is via the ESP-IDF framework with Arduino
added as a component.

**What this means:**
- Build system uses ESP-IDF (CMake-based) managed by PlatformIO
- Arduino APIs (WiFi, Serial, etc.) are available as a component
- Arduino libraries (M5Unified, ESPAsyncWebServer) work normally
- Source code goes in `main/` directory (not `src/`)
- ESP-IDF configuration via `sdkconfig.defaults`
- Components (bluepad32, btstack, arduino) are git submodules in `components/`

**Template used:** https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

## Module Architecture

```
main.cpp (entry point)
  |
  |-- config.h (compile-time configuration)
  |-- debug_log.h/.cpp (serial logging with severity levels)
  |
  |-- wifi_manager.h/.cpp
  |     Auto-connect, auto-reconnect, status reporting
  |
  |-- web_server.h/.cpp + web_ui.h
  |     AsyncWebServer on port 80, WebSocket at /ws
  |     Broadcasts JSON status to all connected clients
  |
  |-- controller_manager.h/.cpp
  |     Bluepad32 wrapper, multi-controller state management
  |     Dead zone handling, input normalization
  |
  |-- display_manager.h/.cpp
  |     M5Unified display API, double-buffered rendering
  |     Status screens: WiFi, controller, outputs
  |
  |-- (future) servo_output.h/.cpp
  |     LEDC PWM on G25/G26 for servo control
  |
  |-- (future) can_motor.h/.cpp
  |     TWAI driver for RobStride motor communication
  |
  |-- (future) imu_manager.h/.cpp
        MPU6886 reading, sensor fusion, balance control
```

## Module Communication

Modules communicate through a shared state model. Each module owns its state
and exposes getters. The main loop orchestrates updates:

```
loop() {
    1. controller_manager.update()  // Read gamepad input
    2. wifi_manager.loop()          // Maintain WiFi connection
    3. web_server.broadcastStatus() // Send status to WebSocket clients
    4. display_manager.update()     // Refresh display
}
```

There is no event bus or callback system between modules. The main loop polls
each module sequentially. This keeps the architecture simple and predictable.

## Pin Assignment Summary

| Pin(s)  | Module            | Purpose                     |
|---------|-------------------|-----------------------------|
| G4      | Power             | HOLD pin (keep power on)    |
| G25     | Servo Output      | Left wheel PPM              |
| G26     | Servo Output      | Right wheel PPM             |
| G32     | CAN Motor         | TWAI TX to Mini CAN Unit    |
| G33     | CAN Motor         | TWAI RX from Mini CAN Unit  |
| G21     | IMU/RTC           | I2C SDA                     |
| G22     | IMU/RTC           | I2C SCL                     |
| G37     | Buttons           | Button A                    |
| G39     | Buttons           | Button B                    |
| G35     | Buttons           | Button C (Power)            |
| G15,13,14,12,5,27 | Display | SPI to ST7789V2       |

## Memory Budget

ESP32-PICO-V3-02 resources:
- **SRAM**: ~320KB (internal)
- **PSRAM**: 2MB (external, slower)
- **Flash**: 8MB

Estimated usage for Phase 1:
- WiFi stack: ~40KB
- Bluetooth (BTstack): ~60KB
- WebSocket server: ~20KB
- Display buffer (sprite): ~65KB (135*240*2 bytes)
- Application code + data: ~30KB
- **Total estimated**: ~215KB of ~320KB SRAM

PSRAM is available for large allocations (buffers, strings) if needed.
Monitor `ESP.getFreeHeap()` and `ESP.getFreePsram()` during development.

## WiFi Configuration

Credentials are stored in `config.h` as compile-time constants:
- SSID: "McWifi"
- Password: "gonewireless"

This keeps configuration simple and avoids filesystem complexity.
For production use, consider NVS (non-volatile storage) or a captive portal.

## WebSocket Protocol

The web server broadcasts a JSON status message to all connected WebSocket
clients at 10Hz (every 100ms). The message format:

```json
{
  "wifi": {
    "ssid": "McWifi",
    "ip": "192.168.1.100",
    "rssi": -45
  },
  "controllers": [
    {
      "id": 0,
      "connected": true,
      "lx": 0, "ly": 0,
      "rx": 0, "ry": 0,
      "lt": 0, "rt": 0,
      "buttons": 0,
      "dpad": 0
    }
  ],
  "servos": {
    "left": 1500,
    "right": 1500
  },
  "system": {
    "uptime_s": 123,
    "free_heap": 45000,
    "free_psram": 1800000
  }
}
```

## Future Phases (Not Yet Implemented)

### Phase 2: Servo Output
- LEDC PWM on G25/G26
- Map controller stick to servo pulse width (1000-2000us)
- Mixing for tank/arcade drive modes

### Phase 3: CAN Motor Control
- TWAI driver initialization
- RobStride RS-02 enable/disable/control
- MIT mode for arm control
- Safety: auto-disable on controller disconnect

### Phase 4: IMU & Balancing
- MPU6886 sensor reading at 100Hz+
- Complementary or Kalman filter for angle estimation
- PID balance controller
- Controller input as setpoint offset

### Phase 5: Advanced Features
- OTA firmware updates
- Controller rumble feedback
- Multiple drive modes (tank, arcade, mixed)
- Telemetry recording to SD card

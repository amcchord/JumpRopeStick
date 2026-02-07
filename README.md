# JumpRopeStick

A drivable robot controlled by 8BitDo Bluetooth gamepads, running on an M5StickC Plus 2.

## Features

- **Bluetooth Gamepad Input** - Connect up to 4 controllers via Bluepad32
- **WiFi Web Dashboard** - Real-time status monitoring via WebSocket
- **On-Device Display** - WiFi status, controller inputs, and output values
- **Servo PPM Output** - Wheel control on G25/G26 (future)
- **CAN Bus Motors** - RobStride arm control (future)
- **IMU** - Self-balancing tricks (future)

## Hardware

- **MCU**: M5StickC Plus 2 (ESP32-PICO-V3-02)
- **Controller**: 8BitDo Pro 3 (Bluetooth Classic)
- **CAN Transceiver**: M5Stack Mini CAN Unit (TJA1051T)
- **Motors**: RobStride RS-02 (CAN bus)

## Building

Requires PlatformIO. The project uses ESP-IDF framework with Arduino as a component
(required for Bluepad32 Bluetooth gamepad support).

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor serial output
pio device monitor
```

## WiFi Configuration

Edit `main/config.h`:
```cpp
#define WIFI_SSID     "McWifi"
#define WIFI_PASSWORD "gonewireless"
```

## Pairing an 8BitDo Controller

1. Set the mode switch on the controller to "A" (Android) or "D" (D-input)
2. Hold Start + pairing button until LEDs flash rapidly
3. The controller will auto-connect to the ESP32

## Project Structure

```
main/
  main.c                 - Bluepad32/BTstack init (CPU0)
  sketch.cpp             - Arduino setup/loop (CPU1)
  config.h               - All configuration in one place
  debug_log.h/.cpp       - Serial debug logging with levels
  wifi_manager.h/.cpp    - WiFi connection with auto-reconnect
  web_server.h/.cpp      - HTTP + WebSocket status server
  web_ui.h               - Embedded HTML dashboard
  controller_manager.h/.cpp - Bluepad32 gamepad wrapper
  display_manager.h/.cpp - On-device LCD status display
components/
  arduino/               - Arduino core (git submodule)
  bluepad32/             - Bluepad32 library
  bluepad32_arduino/     - Bluepad32 Arduino bindings
  btstack/               - Bluetooth stack
docs/
  hardware.md            - M5StickC Plus 2 hardware reference
  controllers.md         - Controller and Bluepad32 reference
  robstride_motors.md    - RobStride CAN protocol reference
  architecture.md        - System architecture and design decisions
```

## License

MIT

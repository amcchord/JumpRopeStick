# Bluetooth Controller Reference

## 8BitDo Pro 3

- **Bluetooth**: Classic (BR/EDR) - compatible with original ESP32
- **Joysticks**: TMR joysticks with 12-bit ADC sampling
- **Triggers**: Hall Effect triggers (switchable linear/non-linear tactile modes)
- **Buttons**: ABXY (magnetic, swappable layout), D-pad, L1/R1/L2/R2, L3/R3
- **Extra**: R4/L4 remappable bumpers, Start/Select/Home
- **Modes**: Switch (S), Android (A), D-input (D), X-input (X) via 4-way mode switch
- **For ESP32 use**: Set to Android (A) or D-input (D) mode for best compatibility

### Pairing

1. Set the controller mode switch (bottom) to "A" (Android) or "D" (D-input)
2. Hold Start + pairing button until LEDs flash rapidly
3. Bluepad32 will discover and connect automatically

## Bluepad32 Library

Bluepad32 is the Bluetooth HID host library that receives gamepad input on ESP32.

- **Repository**: https://github.com/ricardoquesada/bluepad32
- **Docs**: https://bluepad32.readthedocs.io/en/stable/
- **License**: Apache 2.0

### Key Constraints

- **ESP32 (original) ONLY** for Bluetooth Classic controllers
  - ESP32-S3, ESP32-C3 only support BLE controllers
  - Most gamepads (including 8BitDo) use Bluetooth Classic
- Replaces the standard ESP32 Bluetooth stack (uses BTstack instead)
- Uses CPU0 only, leaving CPU1 for application code
- Supports up to 4 simultaneous controllers
- Very low latency (~5ms)

### Integration Method

For PlatformIO, Bluepad32 requires using the ESP-IDF framework with Arduino as a
component. The official template handles this:
https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

### Arduino API

```cpp
#include <Bluepad32.h>

// Maximum 4 controllers
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void onConnectedController(ControllerPtr ctl) {
    // Find empty slot
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            myControllers[i] = ctl;
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
            break;
        }
    }
}

void setup() {
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();  // Optional: clear paired devices
    BP32.enableVirtualDevice(false);  // Disable virtual device
}

void loop() {
    bool dataUpdated = BP32.update();
    if (dataUpdated) {
        for (auto ctl : myControllers) {
            if (ctl && ctl->isConnected() && ctl->hasData()) {
                // Read controller state
                int16_t lx = ctl->axisX();     // Left stick X: -511 to 512
                int16_t ly = ctl->axisY();     // Left stick Y: -511 to 512
                int16_t rx = ctl->axisRX();    // Right stick X: -511 to 512
                int16_t ry = ctl->axisRY();    // Right stick Y: -511 to 512
                int16_t lt = ctl->brake();     // Left trigger: 0 to 1023
                int16_t rt = ctl->throttle();  // Right trigger: 0 to 1023
                uint16_t buttons = ctl->buttons();
                uint8_t dpad = ctl->dpad();

                // Button masks
                // BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y
                // BUTTON_SHOULDER_L, BUTTON_SHOULDER_R
                // BUTTON_THUMB_L, BUTTON_THUMB_R
            }
        }
    }
}
```

### Controller Data Ranges

| Input      | Range          | Notes                    |
|------------|----------------|--------------------------|
| axisX()    | -511 to 512    | Left stick horizontal    |
| axisY()    | -511 to 512    | Left stick vertical      |
| axisRX()   | -511 to 512    | Right stick horizontal   |
| axisRY()   | -511 to 512    | Right stick vertical     |
| brake()    | 0 to 1023      | Left trigger (L2)        |
| throttle() | 0 to 1023      | Right trigger (R2)       |
| buttons()  | Bitmask uint16 | See button constants     |
| dpad()     | Bitmask uint8  | DPAD_UP/DOWN/LEFT/RIGHT  |

### Supported Controllers (non-exhaustive)

- 8BitDo Pro 2 / Pro 3 / Lite / SN30 / Zero
- Sony DualSense (PS5), DualShock 4 (PS4), DualShock 3 (PS3)
- Nintendo Switch Pro Controller, Joy-Cons
- Xbox Wireless Controller
- Generic Bluetooth HID gamepads
- Bluetooth keyboards and mice

## WiFi + Bluetooth Coexistence

ESP32 shares a single 2.4GHz radio between WiFi and Bluetooth. When both are active:
- Expect slightly higher latency on both WiFi and Bluetooth (~10-20ms added)
- Bluetooth throughput may decrease
- Bluepad32 handles coexistence internally via time-division multiplexing
- WiFi should use station mode (not AP mode) for best coexistence

Recommendations:
- Keep WebSocket update rate reasonable (10Hz, not 60Hz)
- Use short WiFi packets (JSON status messages are fine)
- Monitor heap memory when both are active
